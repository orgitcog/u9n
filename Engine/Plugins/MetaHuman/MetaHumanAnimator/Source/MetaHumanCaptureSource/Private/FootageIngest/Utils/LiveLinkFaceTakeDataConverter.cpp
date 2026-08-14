// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceTakeDataConverter.h"
#include "MetaHumanMediaSourceReader.h"
#include "MetaHumanWaveFileWriter.h"
#include "ImageSequenceWriter.h"
#include "MetaHumanDepthConverter.h"
#include "MetaHumanCaptureSourceLog.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Async/Async.h"

#include "Error/ScopeGuard.h"

#define LOCTEXT_NAMESPACE "MetaHumanTakeDataConverter"

/**
* Utility class that closes an input/output media operation upon destruction
*/
template<typename TMediaIOType>
struct FScopedMediaIO
{
	TSharedPtr<TMediaIOType> InputOutputOp;

	FScopedMediaIO()
		: InputOutputOp{ TMediaIOType::Create() }
	{
	}

	~FScopedMediaIO()
	{
		InputOutputOp->Close();
	}

	const TSharedPtr<TMediaIOType>& operator->() const
	{
		return InputOutputOp;
	}
};

bool FLiveLinkFaceTakeDataConverter::FrameLogEntry::Parse(const FString& InLogLine, FrameLogEntry& OutLogEntry)
{
	TArray<FString> Tokens;
	InLogLine.ParseIntoArray(Tokens, TEXT(","));

	if (Tokens.Num() < 5 || Tokens.Num() > 6)
	{
		return false;
	}

	OutLogEntry = FrameLogEntry{ MoveTemp(Tokens) };
	return true;
}

FLiveLinkFaceTakeDataConverter::FrameLogEntry::FrameLogEntry(TArray<FString>&& InTokens) : Tokens(MoveTemp(InTokens))
{
}

char FLiveLinkFaceTakeDataConverter::FrameLogEntry::EntryType()
{
	return !Tokens[0].IsEmpty() ? Tokens[0][0] : InvalidType;
}

int64 FLiveLinkFaceTakeDataConverter::FrameLogEntry::FrameIndex()
{
	return FCString::Atoi64(*Tokens[1]);
}

double FLiveLinkFaceTakeDataConverter::FrameLogEntry::Time()
{
	const int64 Numerator = FCString::Atoi64(*Tokens[2]);
	const double Denominator = FCString::Atod(*Tokens[3]);
	return Numerator / Denominator;
}

bool FLiveLinkFaceTakeDataConverter::FrameLogEntry::Timecode(FTimecode& OutTimecode)
{
	TArray<FString> TimecodeTokens;
	Tokens[4].ParseIntoArray(TimecodeTokens, TEXT(":"));
	if (TimecodeTokens.Num() != 4 && TimecodeTokens.Num() != 3)
	{
		return false;
	}

	// Limit hours to 0-23 else we can't accurately
	// show the clip in sequencer.
	const int32 Hours = FCString::Atoi(*TimecodeTokens[0]) % 24;
	const int32 Mins = FCString::Atoi(*TimecodeTokens[1]);

	int32 Secs = 0;
	int32 Frames = 0;
	bool bIsDropFrame = TimecodeTokens[2].Contains(TEXT(";"));
	if (bIsDropFrame)
	{
		// TimecodeTokens[2] == 00;00
		TArray<FString> SecondsAndFrames;
		TimecodeTokens[2].ParseIntoArray(SecondsAndFrames, TEXT(";"));

		if (SecondsAndFrames.Num() != 2)
		{
			return false;
		}

		Secs = FCString::Atoi(*SecondsAndFrames[0]);
		Frames = FMath::RoundHalfFromZero(FCString::Atof(*SecondsAndFrames[1]));
	}
	else
	{
		Secs = FCString::Atoi(*TimecodeTokens[2]);
		Frames = FMath::RoundHalfFromZero(FCString::Atof(*TimecodeTokens[3]));
	}

	// iPhone timecode is never drop frame - always either 30 or 60 fps
	OutTimecode = FTimecode(Hours, Mins, Secs, Frames, bIsDropFrame);
	return true;
}

bool FLiveLinkFaceTakeDataConverter::FrameLogEntry::IsDroppedFrame()
{
	// If it's the old log format that didn't include dropped frame info, assume it wasn't dropped
	if (Tokens.Num() == 5)
	{
		return false;
	}

	return Tokens[5] != TEXT("0");
}

bool FLiveLinkFaceTakeDataConverter::Initialize(const FConvertParams& InConvertParams)
{
	bInitialized = true;
	ConvertParams = InConvertParams;
	return true;
}


FLiveLinkFaceTakeDataConverter::FConvertResult FLiveLinkFaceTakeDataConverter::Convert(const FStopToken& InStopToken)
{
	check(bInitialized);

	TakeInfo = ConvertParams.TakeInfo;
	TargetIngestDirectory = ConvertParams.TargetIngestDirectory;
	TargetIngestPackagePath = ConvertParams.TargetIngestPackagePath;

	TArray<TPair<double, bool>> VideoFrameTimeInSeconds;
	TArray<TPair<double, bool>> DepthFrameTimeInSeconds;

	ParseFrameLog(VideoFrameTimeInSeconds, DepthFrameTimeInSeconds);

	BuildVideoDepthSyncMap(VideoFrameTimeInSeconds, DepthFrameTimeInSeconds);

	ExecuteAsyncTasks(InStopToken);

	NotifySuccess();

	FConvertResult Result;
	Result.TargetIngestPackagePath = TargetIngestPackagePath;
	Result.ImageSequenceDirectory = TargetVideoSequenceDirectory;
	Result.DepthSequenceDirectory = TargetDepthSequenceDirectory;
	Result.WAVFilePath = TargetWAVFilePath;
	Result.bVideoTimecodePresent = true;
	Result.VideoTimecode = VideoTimecode;
	Result.bAudioTimecodePresent = true;
	Result.AudioTimecode = AudioTimecode;
	Result.TimecodeRate = TimecodeRate;
	Result.CaptureExcludedFrames = CaptureExcludedFrames;
	return Result;
}

void FLiveLinkFaceTakeDataConverter::ExecuteAsyncTasks(const FStopToken& InStopToken)
{

	FAsyncTask<FIngestTask> ExtractWav(AsyncTaskProgresses.Num(),
									   FIngestTask::FTaskHandler::CreateRaw(this, &FLiveLinkFaceTakeDataConverter::ConvertMovToWav),
									   InStopToken);
	AsyncTaskProgresses.Emplace(0.0f);

	FAsyncTask<FIngestTask> ExtractImageSequence(AsyncTaskProgresses.Num(),
												 FIngestTask::FTaskHandler::CreateRaw(this, &FLiveLinkFaceTakeDataConverter::ConvertVideoToImageSequence),
												 InStopToken);
	AsyncTaskProgresses.Emplace(0.0f);

	FAsyncTask<FIngestTask> ExtractDepth(AsyncTaskProgresses.Num(),
										 FIngestTask::FTaskHandler::CreateRaw(this, &FLiveLinkFaceTakeDataConverter::ConvertVideoToDepth),
										 InStopToken);
	AsyncTaskProgresses.Emplace(0.0f);


	// Uses GThreadPool
	ExtractWav.StartBackgroundTask();
	ExtractImageSequence.StartBackgroundTask();
	ExtractDepth.StartBackgroundTask();

	ExtractWav.EnsureCompletion(false, true);
	ExtractImageSequence.EnsureCompletion(false, true);
	ExtractDepth.EnsureCompletion(true, true);

	AsyncTaskProgresses.Empty();
}

void FLiveLinkFaceTakeDataConverter::ConvertMovToWav(const FIngestTask& InTask, const FStopToken& InStopToken)
{
	if (InStopToken.IsStopRequested())
	{
		NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser));
		return;
	}

	const FString MOVFilePath = TakeInfo.GetMOVFilePath();
	UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Convert Mov To Wav: %s"), *MOVFilePath);

	const FString WAVFileName = FPaths::ChangeExtension(FPaths::GetCleanFilename(MOVFilePath), TEXT(".wav"));
	TargetWAVFilePath = TargetIngestDirectory / WAVFileName;

	const TSharedPtr<IMetaHumanMediaAudioSourceReader> Reader = IMetaHumanMediaAudioSourceReader::Create();
	if (!Reader->Open(TakeInfo.GetMOVFilePath()))
	{
		FText Message = FText::Format(LOCTEXT("VideoOpenFailed", "Failed to open the video file: {0}."), FText::FromString(MOVFilePath));
		NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
		return;
	}

	auto ScopedReaderClose = MakeScopeGuard([Reader]()
	{
		Reader->Close();
	});

	const FTimespan Duration = Reader->GetTotalDuration();

	if (Reader->GetFormat() != EMediaAudioSampleFormat::Int16)
	{
		FText Message = FText::Format(LOCTEXT("InvalidAudioFormatPCM", "Invalid audio format in file {0}. Only 16-bit PCM is currently supported."), FText::FromString(MOVFilePath));
		NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
		return;
	}

	const TSharedPtr<IMetaHumanWaveFileWriter> Writer = IMetaHumanWaveFileWriter::Create();
	if (!Writer->Open(TargetWAVFilePath, Reader->GetSampleRate(), Reader->GetChannels(), 16))
	{
		FText Message = FText::Format(LOCTEXT("WavFileCreationFailure", "Failed to create the audio file: {0}"), FText::FromString(TargetWAVFilePath));
		NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
		return;
	}

	auto ScopedWriterClose = MakeScopeGuard([Writer]()
	{
		Writer->Close();
	});

	while (true)
	{
		IMediaAudioSample* Sample = Reader->Next();
		if (!Sample)
		{
			break;
		}
		if (!Writer->Append(Sample))
		{
			FText Message = FText::Format(LOCTEXT("WavFileWriteFailure", "Failed to write onto the audio file: {0}"), FText::FromString(TargetWAVFilePath));
			NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
			return;
		}

		if (Duration.GetTotalSeconds() > 0.0)
		{
			const float LocalProgress = Sample->GetTime().Time.GetTotalSeconds() / Duration.GetTotalSeconds();
			OnAsyncTaskProgressUpdate(InTask, LocalProgress);
		}

		if (InStopToken.IsStopRequested())
		{
			NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser));
			return;
		}
	}
}

void FLiveLinkFaceTakeDataConverter::ConvertVideoToImageSequence(const FIngestTask& InTask, const FStopToken& InStopToken)
{
	ConvertVideoToImageSequenceImplementation(InTask, InStopToken);
}

bool FLiveLinkFaceTakeDataConverter::ConvertVideoToImageSequenceImplementation(const FIngestTask& InTask, const FStopToken& InStopToken)
{
	if (InStopToken.IsStopRequested())
	{
		NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser));
		return false;
	}

	const FString MOVFilePath = TakeInfo.GetMOVFilePath();
	TargetVideoSequenceDirectory = TargetIngestDirectory / TEXT("Video_Frames");

	UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Convert Mov To Image Sequence Start: %s"), *MOVFilePath);
	UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Writing the video frames into %s"), *TargetVideoSequenceDirectory);

	if (!IFileManager::Get().DirectoryExists(*TargetVideoSequenceDirectory))
	{
		if (!IFileManager::Get().MakeDirectory(*TargetVideoSequenceDirectory))
		{
			FText Message = FText::Format(LOCTEXT("MovToImSeqFailureDirCreation1", "Failed to create target directory: '{0}'."), FText::FromString(TargetVideoSequenceDirectory));
			NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
			return false;
		}
	}

	int32 LastVideoFrameIndex = -1;

	FScopedMediaIO<IMetaHumanMediaVideoSourceReader> VideoReader;
	FScopedMediaIO<IImageSequenceWriter> VideoWriter;

	if (!VideoReader->Open(MOVFilePath))
	{
		FText Message = FText::Format(LOCTEXT("MovToImSeqFailure", "Failed to convert .mov file: '{0}' to an image sequence."), FText::FromString(MOVFilePath));
		NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
		return false;
	}

	const double TotalDuration = VideoReader->GetTotalDuration().GetTotalSeconds();
	VideoReader->SetDefaultOrientation(TakeInfo.VideoMetadata.Orientation);

	if (!VideoWriter->Open(TargetVideoSequenceDirectory))
	{
		FText Message = FText::Format(LOCTEXT("ImSeqCreationFailure", "Failed to create the image sequence: '{0}'"), FText::FromString(TargetVideoSequenceDirectory));
		NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
		return false;
	}

	for (const TTuple<int32, int32, bool>& Tuple : VideoDepthSyncMap)
	{
		const int32 VideoFrameIndex = Tuple.Get<0>();

		IMediaTextureSample* VideoSample = nullptr;
		for (; LastVideoFrameIndex < VideoFrameIndex; ++LastVideoFrameIndex)
		{
			VideoSample = VideoReader->Next();
			if (!VideoSample)
			{
				FText Message = FText::Format(LOCTEXT("MovToImSeqFailure", "Failed to convert .mov file: '{0}' to an image sequence."), FText::FromString(MOVFilePath));
				NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
				return false;
			}
		}

		//	Write the video frame
		if (!VideoSample || !VideoWriter->Append(VideoSample))
		{
			FText Message = FText::Format(LOCTEXT("MovToImSeqFailure", "Failed to convert .mov file: '{0}' to an image sequence."), FText::FromString(MOVFilePath));
			NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
			return false;
		}

		const double Time = VideoSample->GetTime().Time.GetTotalSeconds();
		if (TotalDuration > 0.0)
		{
			OnAsyncTaskProgressUpdate(InTask, TotalProgressForImageSequence * Time / TotalDuration);
		}

		if (InStopToken.IsStopRequested())
		{
			NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser));
			return false;
		}
	}

	UE_LOG(LogTemp, Display, TEXT("Convert Mov To Image Sequence Finished: %s"), *MOVFilePath);
	return true;
}

void FLiveLinkFaceTakeDataConverter::ConvertVideoToDepth(const FIngestTask& InTask, const FStopToken& InStopToken)
{
	if (InStopToken.IsStopRequested())
	{
		NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser));
		return;
	}

	const FString MOVFilePath = TakeInfo.GetMOVFilePath();
	TargetDepthSequenceDirectory = TargetIngestDirectory / TEXT("Depth_Frames");

	UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Convert Mov To Depth Sequence Start: %s"), *MOVFilePath);
	UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Writing the depth frames into %s"), *TargetDepthSequenceDirectory);

	if (!IFileManager::Get().DirectoryExists(*TargetDepthSequenceDirectory))
	{
		if (!IFileManager::Get().MakeDirectory(*TargetDepthSequenceDirectory))
		{
			FText Message = FText::Format(LOCTEXT("MovToImSeqFailureDirCreation2", "Failed to create target directory: '{0}'."), FText::FromString(TargetDepthSequenceDirectory));
			NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
			return;
		}
	}

	int32 LastDepthFrameIndex = -1;
	FDepthConverter DepthConverter(TakeInfo.DepthMetadata.bShouldCompressFiles);

	if (!DepthConverter.Open(TakeInfo.GetDepthFilePath(), *TargetDepthSequenceDirectory))
	{
		NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, DepthConverter.GetError().ToString()));
		return;
	}

	// TODO: Review the orientation enum
	DepthConverter.SetGeometry(TakeInfo.DepthMetadata.Resolution, TakeInfo.DepthMetadata.Orientation);

	const float Step = 1.0f / VideoDepthSyncMap.Num();

	FDepthWriterTask::FOnWriteComplete OnWriteComplete = FDepthWriterTask::FOnWriteComplete::CreateLambda([this, Step, &InTask, &InStopToken](FDepthWriterTask::DepthWriteResult InResult)
	{
		if (InStopToken.IsStopRequested())
		{
			return;
		}

		if (InResult.IsValid())
		{
			int32 CurrentFrame = InResult.ClaimResult();

			const float LocalProgress = Step * (CurrentFrame + 1);
			OnAsyncTaskProgressUpdate(InTask, LocalProgress);
		}
		else
		{
			NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, InResult.ClaimError().GetMessage()));
		}
	});

	for (const TTuple<int32, int32, bool>& Tuple : VideoDepthSyncMap)
	{
		const int32 DepthFrameIndex = Tuple.Get<1>();

		for (; LastDepthFrameIndex < DepthFrameIndex; ++LastDepthFrameIndex)
		{
			if (!DepthConverter.Next())
			{
				NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, DepthConverter.GetError().ToString()));
				return;
			}
		}

		//	Write the depth frame asynchronously
		DepthConverter.WriteAsync(OnWriteComplete);

		if (InStopToken.IsStopRequested())
		{
			NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser));
			return;
		}
	}

	DepthConverter.WaitAsync();

	UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Convert Mov To Depth Sequence Finished: %s"), *MOVFilePath);
}

void FLiveLinkFaceTakeDataConverter::BuildVideoDepthSyncMap(const TArray<TPair<double, bool>>& InVideoFrameTimeInSeconds,
															const TArray<TPair<double, bool>>& InDepthFrameTimeInSeconds)
{
	//	Associate the depth frame within +-SyncTolerance seconds of the video frame
	const double SyncTolerance = 0.5 / TakeInfo.VideoMetadata.FrameRate;

	// Find the first video frame that has a matching depth frame
	int32 VideoIndexStart = -1;
	for (int32 VideoIndex = 0; VideoIndex < InVideoFrameTimeInSeconds.Num() && VideoIndexStart == -1; ++VideoIndex)
	{
		const double VideoFrameTime = InVideoFrameTimeInSeconds[VideoIndex].Key;
		for (int32 DepthIndex = 0; DepthIndex < InDepthFrameTimeInSeconds.Num(); ++DepthIndex)
		{
			const double DepthFrameTime = InDepthFrameTimeInSeconds[DepthIndex].Key;
			if (FMath::Abs(VideoFrameTime - DepthFrameTime) <= SyncTolerance)
			{
				VideoIndexStart = VideoIndex;
				break;
			}
			if (DepthFrameTime > VideoFrameTime)
			{
				break;
			}
		}
	}

	if (VideoIndexStart == -1)
	{
		return;
	}

	// Video frames and depth frames should now match every Step video frames
	check(TakeInfo.VideoMetadata.FrameRate >= 1.0);
	check(TakeInfo.DepthMetadata.FrameRate >= 1.0);
	// We require that video frame rate is an integer multiple of the depth frame rate.
	// In addition the video frame rate must be greater than the depth frame rate.
	check(FMath::IsNearlyZero(FMath::Fmod(TakeInfo.VideoMetadata.FrameRate, TakeInfo.DepthMetadata.FrameRate)));
	
	int32 Step = TakeInfo.VideoMetadata.FrameRate / TakeInfo.DepthMetadata.FrameRate;  
	int32 LastDepthFrameIndex = 0;
	int32 SyncFrameIndex = 0;
	FFrameRange CurrentExcludedFrameRange;
	for (int32 VideoIndex = VideoIndexStart; VideoIndex < InVideoFrameTimeInSeconds.Num(); VideoIndex += Step, ++SyncFrameIndex)
	{
		const double VideoFrameTime = InVideoFrameTimeInSeconds[VideoIndex].Key;
		for (int32 DepthIndex = LastDepthFrameIndex; DepthIndex < InDepthFrameTimeInSeconds.Num(); ++DepthIndex)
		{
			const double DepthFrameTime = InDepthFrameTimeInSeconds[DepthIndex].Key;
			const bool bMatchingVideoAndDepth = FMath::Abs(VideoFrameTime - DepthFrameTime) <= SyncTolerance;
			const bool bNoMatchingVideoAndDepth = (!bMatchingVideoAndDepth && (DepthFrameTime > VideoFrameTime));
			if (bMatchingVideoAndDepth || bNoMatchingVideoAndDepth)
			{
				bool bIsDroppedFrame = false;

				if (bMatchingVideoAndDepth)
				{
					bIsDroppedFrame = InVideoFrameTimeInSeconds[VideoIndex].Value || InDepthFrameTimeInSeconds[DepthIndex].Value;
					VideoDepthSyncMap.Add({ VideoIndex, DepthIndex, bIsDroppedFrame });
					LastDepthFrameIndex = DepthIndex;
				}
				else
				{
					bIsDroppedFrame = true;
					VideoDepthSyncMap.Add({ VideoIndex, LastDepthFrameIndex, bIsDroppedFrame });
				}

				if (bIsDroppedFrame)
				{
					if (CurrentExcludedFrameRange.StartFrame == -1)
					{
						CurrentExcludedFrameRange.StartFrame = SyncFrameIndex;
					}
					else if (CurrentExcludedFrameRange.EndFrame != SyncFrameIndex - 1)
					{
						CaptureExcludedFrames.Add(CurrentExcludedFrameRange);

						CurrentExcludedFrameRange.StartFrame = SyncFrameIndex;
					}

					CurrentExcludedFrameRange.EndFrame = SyncFrameIndex;
				}

				break;
			}
		}
	}

	if (CurrentExcludedFrameRange.StartFrame != -1)
	{
		CaptureExcludedFrames.Add(CurrentExcludedFrameRange);
	}
}

void FLiveLinkFaceTakeDataConverter::ParseFrameLog(TArray<TPair<double, bool>>& OutVideoFrameTimeInSeconds,
												   TArray<TPair<double, bool>>& OutDepthFrameTimeInSeconds)
{
	const FString FrameLogPath = TakeInfo.GetFrameLogFilePath();

	TArray<FString> FrameLogLines;
	if (!FFileHelper::LoadFileToStringArray(FrameLogLines, *FrameLogPath))
	{
		FText Message = FText::Format(LOCTEXT("ReadFrameLogFailure", "Failed to read the frame log: {0}."), FText::FromString(FrameLogPath));
		NotifyFailure(FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
		return;
	}

	bool bAudioTimecodeFound = false;
	FFrameRate OriginalFrameRate;
	if (TakeInfo.VideoMetadata.FrameRate == 30.0)
	{
		OriginalFrameRate = FFrameRate{ 30, 1 };
	}
	else if (TakeInfo.VideoMetadata.FrameRate == 60.0)
	{
		OriginalFrameRate = FFrameRate{ 60, 1 };
	}

	for (const FString& Line : FrameLogLines)
	{
		FrameLogEntry LogEntry;
		if (!FrameLogEntry::Parse(Line, LogEntry))
		{
			continue;
		}
		
		if (LogEntry.EntryType() == FrameLogEntry::VideoType)
		{
			OutVideoFrameTimeInSeconds.Add({ LogEntry.Time(), LogEntry.IsDroppedFrame() });

			if (LogEntry.FrameIndex() == 0) // Take timecode from first video frame
			{
				FTimecode Timecode;
				if (!LogEntry.Timecode(Timecode))
				{
					continue;
				}

				// If frame rate isn't known, there's no sense to parse timecode
				if (OriginalFrameRate == FFrameRate{})
				{
					continue;
				}

				// Make sure resulting time code is 30fps to match the depth which is (currently always 30fps).
				// This probably isn't mandatory and we should at some point step away from this when we make
				// sure this assumption is not present in other parts of the MHA codebase.
				FFrameRate TargetFrameRate{30, 1};
				VideoTimecode = FTimecode::FromTimespan(Timecode.ToTimespan(OriginalFrameRate), TargetFrameRate, true);
				TimecodeRate = TargetFrameRate;
			}
		}
		else if (LogEntry.EntryType() == FrameLogEntry::DepthType)
		{
			OutDepthFrameTimeInSeconds.Add({ LogEntry.Time(), LogEntry.IsDroppedFrame() });
		}
		else if (LogEntry.EntryType() == FrameLogEntry::AudioType)
		{
			if (!bAudioTimecodeFound)
			{
				FTimecode Timecode;
				if (!LogEntry.Timecode(Timecode))
				{
					continue;
				}

				// If frame rate isn't known, there's no sense to parse timecode
				if (OriginalFrameRate == FFrameRate{})
				{
					continue;
				}

				// Make sure resulting time code is 30fps to match the depth which is (currently always 30fps).
				// This probably isn't mandatory and we should at some point step away from this when we make
				// sure this assumption is not present in other parts of the MHA codebase.
				FFrameRate TargetFrameRate{ 30, 1};
				AudioTimecode = FTimecode::FromTimespan(Timecode.ToTimespan(OriginalFrameRate), TargetFrameRate, true);
				bAudioTimecodeFound = true;
			}
		}
	}

	// If no audio timecode was specified, assume it's the same as video
	if (!bAudioTimecodeFound)
	{
		AudioTimecode = VideoTimecode;
	}
}

void FLiveLinkFaceTakeDataConverter::NotifyProgress(float InProgress)
{
	OnProgressDelegate.ExecuteIfBound(InProgress);
}

void FLiveLinkFaceTakeDataConverter::NotifyFailure(FMetaHumanCaptureError InError)
{
	OnFinishedDelegate.ExecuteIfBound(MoveTemp(InError));
}

void FLiveLinkFaceTakeDataConverter::NotifySuccess()
{
	OnProgressDelegate.ExecuteIfBound(1.0f);
	OnFinishedDelegate.ExecuteIfBound(ResultOk);
}

void FLiveLinkFaceTakeDataConverter::OnAsyncTaskProgressUpdate(const FIngestTask& InTask, float InAsyncTaskProgress)
{
	if (InAsyncTaskProgress < AsyncTaskProgresses[InTask.Id].load())
	{
		return;
	}

	AsyncTaskProgresses[InTask.Id].store(InAsyncTaskProgress, std::memory_order_relaxed);

	float TotalProgress = 0.0f;
	for (const std::atomic<float>& AsyncTaskProgress : AsyncTaskProgresses)
	{
		TotalProgress += AsyncTaskProgress.load();
	}

	NotifyProgress(TotalProgress / AsyncTaskProgresses.Num());
}


#undef LOCTEXT_NAMESPACE