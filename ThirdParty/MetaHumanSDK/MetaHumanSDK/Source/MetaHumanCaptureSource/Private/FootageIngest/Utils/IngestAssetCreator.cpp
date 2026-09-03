// Copyright Epic Games, Inc. All Rights Reserved.

#include "IngestAssetCreator.h"

#include "Misc/ScopedSlowTask.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ImgMediaSource.h"
#include "Sound/SoundWave.h"
#include "AssetImportTask.h"
#include "ObjectTools.h"
#include "ImageSequenceUtils.h"
#include "MetaHumanCameraCalibrationImporterFactory.h"

#include "ParseTakeUtils.h"

#include "MetaHumanCaptureSourceLog.h"

#define LOCTEXT_NAMESPACE "IngestAssetCreator"

const FText FIngestAssetCreator::AudioImportFailedText = LOCTEXT("IngestError_AudioImport", "Error importing audio clip");

namespace UE::MetaHuman::Private
{

static FString CreateTimecodeString(const FTimecode& InTimecode, const FFrameRate InFrameRate)
{
	// The extra parentheses are added because negative timecodes have a peculiar leading dash placement which makes them look like 
	// someone is trying to break up a sentence rather than trying to indicate a negative number.
	return FString::Printf(
		TEXT("(%s %s @ %.2f fps)"), 
		*InTimecode.ToString(), 
		InTimecode.bDropFrameFormat ? TEXT("DF") : TEXT("ND"), InFrameRate.AsDecimal()
	);
};

// Tries to determine a video rate for the entire take
static TOptional<FFrameRate> DetermineTakeVideoRate(const FMetaHumanTake& InMetaHumanTake)
{
	TArray<FFrameRate> VideoRates;
	VideoRates.Reserve(InMetaHumanTake.Views.Num());

	for (const FMetaHumanTakeView& View : InMetaHumanTake.Views)
	{
		if (View.VideoTimecodeRate != FFrameRate())
		{
			VideoRates.Emplace(View.VideoTimecodeRate);
		}
		else if (View.Video && View.Video->FrameRateOverride != FFrameRate())
		{
			// No video timecode rate, we make a guess that it matches the video frame rate
			VideoRates.Emplace(View.Video->FrameRateOverride);
		}
	}

	if (VideoRates.IsEmpty())
	{
		return {};
	}

	const FFrameRate FirstVideoRate = VideoRates[0];

	for (int32 Idx = 1; Idx < VideoRates.Num(); ++Idx)
	{
		if (VideoRates[Idx] != FirstVideoRate)
		{
			// We found mismatched video rates, we don't handle this situation
			return {};
		}
	}

	return FirstVideoRate;
}

static FFrameRate EstimateSmpteTimecodeRate(const FFrameRate InVideoFrameRate)
{
	if (FMath::IsNearlyEqual(InVideoFrameRate.AsDecimal(), 60.0))
	{
		return FFrameRate(30'000, 1'000);
	}

	if (FMath::IsNearlyEqual(InVideoFrameRate.AsDecimal(), 59.94))
	{
		// 29.97
		return FFrameRate(30'000, 1'001);
	}

	if (FMath::IsNearlyEqual(InVideoFrameRate.AsDecimal(), 50.0))
	{
		FFrameRate(25'000, 1'000);
	}

	if (FMath::IsNearlyEqual(InVideoFrameRate.AsDecimal(), 48.0))
	{
		FFrameRate(24'000, 1'000);
	}

	return InVideoFrameRate;
}

static TOptional<FSoundWaveTimecodeInfo> CheckForAudioTimecodeOverride(
	const UAssetImportTask* InAssetImportTask,
	const FMetaHumanTake& InMetaHumanTake,
	const FSoundWaveTimecodeInfo& InSoundWaveTimecodeInfo
)
{
	if (!IsValid(InAssetImportTask))
	{
		return {};
	}

	const bool bTimecodeRateIsSampleRate = InSoundWaveTimecodeInfo.TimecodeRate == FFrameRate(InSoundWaveTimecodeInfo.NumSamplesPerSecond, 1);
	const bool bSoundWaveTimecodeRateIsValid = InSoundWaveTimecodeInfo.TimecodeRate != FFrameRate() && !bTimecodeRateIsSampleRate;

	if (bTimecodeRateIsSampleRate)
	{
		UE_LOG(
			LogMetaHumanIngestAssetCreator,
			Display,
			TEXT(
				"Embedded timecode rate is %.2f fps (the sample rate). "
				"This usually indicates there is no timecode rate information in the wav file: %s"
			),
			InSoundWaveTimecodeInfo.TimecodeRate.AsDecimal(),
			*InAssetImportTask->Filename
		);
	}

	TOptional<FSoundWaveTimecodeInfo> Override;

	constexpr bool bTimecodeRollover = true;
	const FFrameRate EmbeddedTimecodeRate = InSoundWaveTimecodeInfo.TimecodeRate;
	const FTimecode EmbeddedTimecode = FTimecode(InSoundWaveTimecodeInfo.GetNumSecondsSinceMidnight(), EmbeddedTimecodeRate, bTimecodeRollover);
	const TOptional<FFrameRate> TakeVideoRate = DetermineTakeVideoRate(InMetaHumanTake);

	if (InMetaHumanTake.bAudioTimecodePresent && InMetaHumanTake.AudioTimecodeRate != FFrameRate())
	{
		// User is overriding both the audio timecode and the timecode rate, so simply use them as given.
		const FTimecode NewTimecode = InMetaHumanTake.AudioTimecode;
		const FFrameRate NewTimecodeRate = InMetaHumanTake.AudioTimecodeRate;
		const FTimespan Timespan = NewTimecode.ToTimespan(NewTimecodeRate);

		Override = InSoundWaveTimecodeInfo;
		Override->NumSamplesSinceMidnight = Timespan.GetTotalSeconds() * Override->NumSamplesPerSecond;
		Override->bTimecodeIsDropFrame = NewTimecode.bDropFrameFormat;
		Override->TimecodeRate = NewTimecodeRate;

		UE_LOG(
			LogMetaHumanIngestAssetCreator,
			Display,
			TEXT("Overriding embedded audio timecode %s with %s. Taking the audio timecode from the take metadata file"),
			*CreateTimecodeString(EmbeddedTimecode, EmbeddedTimecodeRate),
			*CreateTimecodeString(NewTimecode, NewTimecodeRate)
		);
	}
	else if (InMetaHumanTake.bAudioTimecodePresent && TakeVideoRate)
	{
		// User is overriding just the audio timecode (they have not specified the rate), so we make the assumption that
		// they're trying to manually align things via the take metadata file using the video rate.
		const FTimecode NewTimecode = InMetaHumanTake.AudioTimecode;
		const FFrameRate NewTimecodeRate = *TakeVideoRate;
		const FTimespan Timespan = NewTimecode.ToTimespan(NewTimecodeRate);

		Override = InSoundWaveTimecodeInfo;
		Override->NumSamplesSinceMidnight = Timespan.GetTotalSeconds() * Override->NumSamplesPerSecond;
		Override->bTimecodeIsDropFrame = NewTimecode.bDropFrameFormat;
		Override->TimecodeRate = NewTimecodeRate;

		UE_LOG(
			LogMetaHumanIngestAssetCreator,
			Warning,
			TEXT(
				"Overriding embedded audio timecode %s with %s. "
				"Taking the audio timecode from the take metadata file but assuming the timecode rate matches the video rate"
			),
			*CreateTimecodeString(EmbeddedTimecode, EmbeddedTimecodeRate),
			*CreateTimecodeString(NewTimecode, NewTimecodeRate)
		);
	}
	else if (InMetaHumanTake.AudioTimecodeRate != FFrameRate())
	{
		// User is partially overriding the wav timecode (just the timecode rate)
		Override = InSoundWaveTimecodeInfo;
		Override->TimecodeRate = InMetaHumanTake.AudioTimecodeRate;

		const FTimecode NewTimecode = FTimecode(Override->GetNumSecondsSinceMidnight(), Override->TimecodeRate, bTimecodeRollover);

		UE_LOG(
			LogMetaHumanIngestAssetCreator,
			Display,
			TEXT(
				"Overriding embedded audio timecode %s with %s. "
				"Taking the embedded audio timecode but with the timecode rate from the take metadata file"
			),
			*CreateTimecodeString(EmbeddedTimecode, EmbeddedTimecodeRate),
			*CreateTimecodeString(NewTimecode, Override->TimecodeRate)
		);
	}
	else if (!bSoundWaveTimecodeRateIsValid)
	{
		// Sound wave timecode rate is invalid, so try to find a usable rate from other sources
		if (TakeVideoRate)
		{
			// Here we make an assumption that the audio device doing the recording is using an SMPTE timecode rate (<= 30 fps)
			const FFrameRate NewTimecodeRate = EstimateSmpteTimecodeRate(*TakeVideoRate);
			Override = InSoundWaveTimecodeInfo;
			Override->TimecodeRate = NewTimecodeRate;

			const FTimecode NewTimecode = FTimecode(Override->GetNumSecondsSinceMidnight(), Override->TimecodeRate, bTimecodeRollover);

			UE_LOG(
				LogMetaHumanIngestAssetCreator,
				Display,
				TEXT(
					"Overriding embedded audio timecode %s with %s. "
					"Taking the embedded audio timecode but estimating an SMPTE audio timecode rate from the video"
				),
				*CreateTimecodeString(EmbeddedTimecode, EmbeddedTimecodeRate),
				*CreateTimecodeString(NewTimecode, Override->TimecodeRate)
			);
		}
		else
		{
			// Set timecode rate to 0 to preserve the embedded audio timecode. 
			// This way at least we import the original value, so it may be possible to recover it through other means
			Override = InSoundWaveTimecodeInfo;
			Override->TimecodeRate = FFrameRate(0, 1'000);

			const FTimecode NewTimecode = FTimecode(Override->GetNumSecondsSinceMidnight(), Override->TimecodeRate, bTimecodeRollover);

			UE_LOG(
				LogMetaHumanIngestAssetCreator,
				Display,
				TEXT(
					"Overriding embedded audio timecode %s with %s. "
					"Taking the embedded audio timecode but we could not determine an audio timecode rate"
				),
				*CreateTimecodeString(EmbeddedTimecode, EmbeddedTimecodeRate),
				*CreateTimecodeString(NewTimecode, Override->TimecodeRate)
			);
			check(false);
		}
	}

	return Override;
}

static TOptional<FSoundWaveTimecodeInfo> CheckMetadataForAudioTimecode(
	const UAssetImportTask* InAssetImportTask,
	const FMetaHumanTake& InMetaHumanTake,
	const uint32 InSampleRate
)
{
	TOptional<FSoundWaveTimecodeInfo> Override;

	if (!IsValid(InAssetImportTask))
	{
		return Override;
	}

	if (InMetaHumanTake.bAudioTimecodePresent && InMetaHumanTake.AudioTimecodeRate != FFrameRate())
	{
		const FTimecode NewTimecode = InMetaHumanTake.AudioTimecode;
		const FFrameRate NewTimecodeRate = InMetaHumanTake.AudioTimecodeRate;
		const FTimespan Timespan = NewTimecode.ToTimespan(NewTimecodeRate);

		Override = FSoundWaveTimecodeInfo();
		Override->NumSamplesPerSecond = InSampleRate;
		Override->NumSamplesSinceMidnight = Timespan.GetTotalSeconds() * Override->NumSamplesPerSecond;
		Override->bTimecodeIsDropFrame = NewTimecode.bDropFrameFormat;
		Override->TimecodeRate = NewTimecodeRate;

		UE_LOG(
			LogMetaHumanIngestAssetCreator,
			Display,
			TEXT("No embedded audio timecode, using %s instead. Taking the audio timecode from the take metadata file"),
			*CreateTimecodeString(NewTimecode, Override->TimecodeRate)
		);
	}
	else if (InMetaHumanTake.bAudioTimecodePresent)
	{
		if (const TOptional<FFrameRate> TakeVideoRate = DetermineTakeVideoRate(InMetaHumanTake))
		{
			const FTimecode NewTimecode = InMetaHumanTake.AudioTimecode;
			const FFrameRate NewTimecodeRate = *TakeVideoRate;
			const FTimespan Timespan = NewTimecode.ToTimespan(NewTimecodeRate);

			Override = FSoundWaveTimecodeInfo();
			Override->NumSamplesPerSecond = InSampleRate;
			Override->NumSamplesSinceMidnight = Timespan.GetTotalSeconds() * Override->NumSamplesPerSecond;
			Override->bTimecodeIsDropFrame = NewTimecode.bDropFrameFormat;
			Override->TimecodeRate = NewTimecodeRate;

			UE_LOG(
				LogMetaHumanIngestAssetCreator,
				Display,
				TEXT(
					"No embedded audio timecode, using %s instead. "
					"Taking the audio timecode from the take metadata file but assuming the timecode rate matches the video rate"
				),
				*CreateTimecodeString(NewTimecode, Override->TimecodeRate)
			);
		}
	}

	return Override;
}

}

void FIngestAssetCreator::CreateAssets_GameThread(TArray<FCreateAssetsData>& InOutCreateAssetDataList,
												  TArray<FMetaHumanTake>& InOutTakeList,
												  FPerTakeCallback InPerTakeCallback)
{
	TArray<TakeId> TakesToDelete;
	auto PerTakesLambda = FPerTakeCallback([PerTakeCallback = MoveTemp(InPerTakeCallback), &TakesToDelete](FPerTakeResult InResult) mutable
	{
		if (InResult.Value.IsError() && InResult.Value.GetError().GetCode() != EMetaHumanCaptureError::Warning)
		{
			TakesToDelete.Add(InResult.Key);
		}

		PerTakeCallback(MoveTemp(InResult));
	}, EDelegateExecutionThread::InternalThread);

	CreateTakeAssets_GameThread(InOutCreateAssetDataList, InOutTakeList, PerTakesLambda);
	DeleteFailedTakes(MoveTemp(TakesToDelete), InOutTakeList, InOutCreateAssetDataList);

	VerifyIngestedData_GameThread(InOutCreateAssetDataList, InOutTakeList, PerTakesLambda);
	DeleteFailedTakes(MoveTemp(TakesToDelete), InOutTakeList, InOutCreateAssetDataList);
}

void FIngestAssetCreator::CreateTakeAssets_GameThread(TArray<FCreateAssetsData>& InOutCreateAssetsData,
													  TArray<FMetaHumanTake>& OutTakes,
													  const FPerTakeCallback& InPerTakeCallback)
{
	struct FTakeAssetImportTaskInfo
	{
		// The index in the current OutTakes array that this import task info refers to
		int32 TakeEntryIndex = INDEX_NONE;

		// The index of the audio clip to import for the given FMetaHumanTake
		int32 AudioClipIndex = INDEX_NONE;

		// True if the task refers to importing camera calibration files
		bool bIsCameraCalibrationAsset = false;
	};

	FScopedSlowTask CreateAssetsProgress(InOutCreateAssetsData.Num(), LOCTEXT("CreateAssetsTask", "Creating assets for ingested takes"));
	CreateAssetsProgress.MakeDialog();

	TMap<TObjectPtr<UAssetImportTask>, FTakeAssetImportTaskInfo> ImportTasksMap;

	for (int32 Index = 0; Index < InOutCreateAssetsData.Num(); ++Index)
	{
		FCreateAssetsData& CreateAssetData = InOutCreateAssetsData[Index];

		FMetaHumanTake& Take = OutTakes.AddDefaulted_GetRef();
		Take.TakeId = CreateAssetData.TakeId;

		CreateAssetsProgress.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("CreateAssetsForTakeMessage", "Creating assets ({0} of {1}) for {2}"), Index + 1, InOutCreateAssetsData.Num(), FText::FromString(CreateAssetData.PackagePath)));

		// Create the image sequence assets
		if (!CreateTakeAssetViews_GameThread(CreateAssetData, Take.Views))
		{
			FText Message = LOCTEXT("IngestError_ViewsCreation", "Failed to create views for assets");
			FPerTakeResult TakeResult = FPerTakeResult(Take.TakeId, FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
			InPerTakeCallback(MoveTemp(TakeResult));

			continue;
		}

		// TODO: FMetaHumanTake only supports one audio clip. Decide if we
		// need to support multiple audio clips for a single take
		check(CreateAssetData.AudioClips.Num() < 2);
		for (int32 AudioClipIndex = 0; AudioClipIndex < CreateAssetData.AudioClips.Num(); ++AudioClipIndex)
		{
			FCreateAssetsData::FAudioData& Audio = CreateAssetData.AudioClips[AudioClipIndex];

			Audio.Asset = GetAssetIfExists<USoundWave>(CreateAssetData.PackagePath, Audio.Name);

			if (Audio.Asset == nullptr)
			{
				// Need to import it from the WAVFilePath
				TObjectPtr<UAssetImportTask> ImportTask = NewObject<UAssetImportTask>();
				ImportTask->bAutomated = true;
				ImportTask->bReplaceExisting = true;
				ImportTask->bSave = false;
				ImportTask->DestinationPath = CreateAssetData.PackagePath;
				ImportTask->DestinationName = Audio.Name;
				ImportTask->Filename = Audio.WAVFile;

				FTakeAssetImportTaskInfo ImportTaskInfo;
				ImportTaskInfo.TakeEntryIndex = Index;
				ImportTaskInfo.AudioClipIndex = AudioClipIndex;
				ImportTasksMap.Emplace(MoveTemp(ImportTask), MoveTemp(ImportTaskInfo));
			}
			else
			{
				// The audio asset already exists, so we just set it in the take
				Take.Audio = Audio.Asset;
			}

			Take.bAudioTimecodePresent = Audio.Timecode.IsSet();

			if (Audio.Timecode)
			{
				Take.AudioTimecode = *Audio.Timecode;
			}

			if (Audio.TimecodeRate)
			{
				Take.AudioTimecodeRate = *Audio.TimecodeRate;
			}
		}

		// Handle the camera calibration
		FCreateAssetsData::FCalibrationData& Calibration = CreateAssetData.Calibration;
		Calibration.Asset = GetOrCreateAsset<UCameraCalibration>(CreateAssetData.PackagePath, Calibration.Name);
		Calibration.Asset->CameraCalibrations.Reset();
		Calibration.Asset->StereoPairs.Reset();

		if (Calibration.CalibrationFile.IsEmpty())
		{
			Calibration.Asset->ConvertFromTrackerNodeCameraModels(Calibration.CalibrationData);

			Take.CameraCalibration = Calibration.Asset;
		}
		else
		{
			// Need to import from the calibration file so create a import task
			TObjectPtr<UAssetImportTask> ImportTask = NewObject<UAssetImportTask>();
			ImportTask->bAutomated = true;
			ImportTask->bReplaceExisting = true;
			ImportTask->bSave = false;
			ImportTask->DestinationPath = CreateAssetData.PackagePath;
			ImportTask->DestinationName = Calibration.Name;
			ImportTask->Filename = Calibration.CalibrationFile;
			ImportTask->Factory = NewObject<UMetaHumanCameraCalibrationImporterFactory>();

			FTakeAssetImportTaskInfo ImportTaskInfo;
			ImportTaskInfo.TakeEntryIndex = Index;
			ImportTaskInfo.bIsCameraCalibrationAsset = true;
			ImportTasksMap.Emplace(MoveTemp(ImportTask), MoveTemp(ImportTaskInfo));
		}
		
		Take.CaptureExcludedFrames = CreateAssetData.CaptureExcludedFrames;
	}

	// Run all the import asset tasks in a bundle
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	TArray<TObjectPtr<UAssetImportTask>> ImportTasks;
	ImportTasksMap.GetKeys(ImportTasks);
	AssetTools.ImportAssetTasks(ImportTasks);

	// Map import results back to the takes
	for (int32 Index = 0; Index < OutTakes.Num(); ++Index)
	{
		TMap<TObjectPtr<UAssetImportTask>, FTakeAssetImportTaskInfo> TakeTasks = 
			ImportTasksMap.FilterByPredicate([Index](const TPair<TObjectPtr<UAssetImportTask>, FTakeAssetImportTaskInfo>& InImportTaskPair)
		{
			return InImportTaskPair.Value.TakeEntryIndex == Index;
		});

		FMetaHumanTake& Take = OutTakes[Index];

		if (TakeTasks.IsEmpty())
		{
			TResult<void, FMetaHumanCaptureError> Result = CheckTakeAssets(Take, !InOutCreateAssetsData[Index].AudioClips.IsEmpty());

			if (Result.IsError())
			{
				FPerTakeResult TakeResult = FPerTakeResult(Take.TakeId, Result.ClaimError());
				InPerTakeCallback(MoveTemp(TakeResult));
			}

			continue;
		}

		TArray<TResult<void, FMetaHumanCaptureError>> TakeResults;
		TakeResults.Reserve(TakeTasks.Num());

		for (const TPair<TObjectPtr<UAssetImportTask>, FTakeAssetImportTaskInfo>& TakeImportTask : TakeTasks)
		{
			const FTakeAssetImportTaskInfo& ImportTaskInfo = TakeImportTask.Value;
			const TObjectPtr<UAssetImportTask>& ImportTask = TakeImportTask.Key;
			const FCreateAssetsData& CreateAssetData = InOutCreateAssetsData[ImportTaskInfo.TakeEntryIndex];

			if (ImportTaskInfo.AudioClipIndex != INDEX_NONE)
			{
				TakeResults.Add(AssignAudioAsset(ImportTask, Take));

				FString AudioAssetName = CreateAssetData.AudioClips[ImportTaskInfo.AudioClipIndex].Name;
				if (USoundWave* Asset = GetAssetIfExists<USoundWave>(CreateAssetData.PackagePath, AudioAssetName); Asset != nullptr)
				{
					UE_LOG(LogMetaHumanIngestAssetCreator, Display, TEXT("Sound Wave asset created successfully"));
				}
			}
			else if (ImportTaskInfo.bIsCameraCalibrationAsset)
			{
				TakeResults.Add(AssignCalibrationAsset(ImportTask, Take));

				if (UCameraCalibration* Asset = GetAssetIfExists<UCameraCalibration>(CreateAssetData.PackagePath, CreateAssetData.Calibration.Name); Asset != nullptr)
				{
					UE_LOG(LogMetaHumanIngestAssetCreator, Display, TEXT("Camera Calibration asset created successfully"));
				}
			}
		}

		for (TResult<void, FMetaHumanCaptureError> Result : TakeResults)
		{
			if (Result.IsError())
			{
				FPerTakeResult TakeResult = FPerTakeResult(Take.TakeId, Result.ClaimError());
				InPerTakeCallback(MoveTemp(TakeResult));

				break;
			}
		}
	}
}

bool FIngestAssetCreator::CreateTakeAssetViews_GameThread(FCreateAssetsData& InCreateAssetDate,
														  TArray<FMetaHumanTakeView>& OutViews)
{
	for (FCreateAssetsData::FViewData& View : InCreateAssetDate.Views)
	{
		for (FCreateAssetsData::FImageSequenceData* ImageSequence : { &View.Video, &View.Depth })
		{
			ImageSequence->Asset = GetOrCreateAsset<UImgMediaSource>(InCreateAssetDate.PackagePath, ImageSequence->Name);
			if (!ImageSequence->Asset)
			{
				UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to create UImgMediaSource '%s'"), *(ImageSequence->Name));
				return false;
			}

			ImageSequence->Asset->SetTokenizedSequencePath(ImageSequence->SequenceDirectory);
			ImageSequence->Asset->FrameRateOverride = ConvertFrameRate(ImageSequence->FrameRate);
			ImageSequence->Asset->StartTimecode = ImageSequence->Timecode;
		}

		FMetaHumanTakeView TakeView;
		TakeView.Video = View.Video.Asset;
		TakeView.bVideoTimecodePresent = View.Video.bTimecodePresent;
		TakeView.VideoTimecode = View.Video.Timecode;
		TakeView.VideoTimecodeRate = View.Video.TimecodeRate;
		TakeView.Depth = View.Depth.Asset;
		TakeView.bDepthTimecodePresent = View.Depth.bTimecodePresent;
		TakeView.DepthTimecode = View.Depth.Timecode;
		TakeView.DepthTimecodeRate = View.Depth.TimecodeRate;
		OutViews.Add(TakeView);
	}

	return true;
}

void FIngestAssetCreator::PrepareSoundWave(const FMetaHumanTake& InMetaHumanTake, const UAssetImportTask* InAssetImportTask, USoundWave* OutSoundWave)
{
	using namespace UE::MetaHuman::Private;

	if (!ensure(IsValid(InAssetImportTask)))
	{
		return;
	}

	if (!ensure(IsValid(OutSoundWave)))
	{
		return;
	}

	TOptional<FSoundWaveTimecodeInfo> TimecodeInfoOverride;

	if (const TOptional<FSoundWaveTimecodeInfo> SoundWaveTimecodeInfo = OutSoundWave->GetTimecodeInfo())
	{
		// We have embedded audio timecode information, check it for validity and compare it against the take metadata to see if we want to override it.
		TimecodeInfoOverride = CheckForAudioTimecodeOverride(InAssetImportTask, InMetaHumanTake, *SoundWaveTimecodeInfo);
	}
	else
	{
		// We have no embedded audio timecode information so check the take metadata to see if we can provide a timecode.
		const uint32 SampleRate = OutSoundWave->GetSampleRateForCurrentPlatform();
		const bool bIsValidSampleRate = SampleRate > 0;
		check(bIsValidSampleRate);

		if (bIsValidSampleRate)
		{
			TimecodeInfoOverride = CheckMetadataForAudioTimecode(InAssetImportTask, InMetaHumanTake, SampleRate);
		}
	}

	if (TimecodeInfoOverride)
	{
		OutSoundWave->SetTimecodeInfo(*TimecodeInfoOverride);
	}

	// Set the compression type to BinkAudio so it can be seekable in a sequencer track
	OutSoundWave->SetSoundAssetCompressionType(ESoundAssetCompressionType::BinkAudio);

	if (const TOptional<FSoundWaveTimecodeInfo> SoundWaveTimecodeInfo = OutSoundWave->GetTimecodeInfo())
	{
		const double FinalTimecodeRate = SoundWaveTimecodeInfo->TimecodeRate.AsDecimal();

		if (FinalTimecodeRate > 1000.0)
		{
			UE_LOG(
				LogMetaHumanIngestAssetCreator,
				Warning,
				TEXT("Sound wave timecode rate is very high (%.2f fps), this is usually an error: %s"),
				FinalTimecodeRate,
				*InAssetImportTask->DestinationPath
			);

			check(false);
		}
	}
}

TResult<void, FMetaHumanCaptureError> FIngestAssetCreator::AssignAudioAsset(const TObjectPtr<UAssetImportTask>& InAssetImportTask, FMetaHumanTake& OutTake)
{
	if (!ensure(IsValid(InAssetImportTask)))
	{
		UE_LOG(LogMetaHumanIngestAssetCreator, Error, TEXT("Failed to import audio (invalid asset import task)"));
		return FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, AudioImportFailedText.ToString());
	}

	const TArray<UObject*>& ImportTaskObjects = InAssetImportTask->GetObjects();

	if (ImportTaskObjects.IsEmpty())
	{
		UE_LOG(LogMetaHumanIngestAssetCreator, Error, TEXT("Failed to import audio (no objects found in the asset import task)"));
		return FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, AudioImportFailedText.ToString());
	}

	USoundWave* Audio = Cast<USoundWave>(ImportTaskObjects[0]);

	if (ensure(IsValid(Audio)))
	{
		PrepareSoundWave(OutTake, InAssetImportTask, Audio);
	}

	OutTake.Audio = Audio;

	return ResultOk;
}

TResult<void, FMetaHumanCaptureError> FIngestAssetCreator::AssignCalibrationAsset(const TObjectPtr<UAssetImportTask>& InAssetImportTask, FMetaHumanTake& OutTake)
{
	if (InAssetImportTask->GetObjects().IsEmpty())
	{
		FText Message = LOCTEXT("IngestError_CalibrationImport", "Error importing camera calibration");
		return FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString());
	}

	OutTake.CameraCalibration = Cast<UCameraCalibration>(InAssetImportTask->GetObjects()[0]);

	return ResultOk;
}

#define CHECK_UOBJECT_RET(Obj, Name) if (!Obj) { return FMetaHumanCaptureError(EMetaHumanCaptureError::NotFound, FString::Printf(TEXT("Asset doesn't exist: %s"), Name)); }
#define CHECK_RET(Condition, Message) if (!(Condition)) { return FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, FString::Printf(TEXT("Checking ingest result failed: %s"), Message)); }

void FIngestAssetCreator::VerifyIngestedData_GameThread(const TArray<FCreateAssetsData>& InCreateAssetsData,
														const TArray<FMetaHumanTake>& InTakes,
														const FPerTakeCallback& InPerTakeCallback)
{
	IFileManager& FileManager = IFileManager::Get();
	for (const FCreateAssetsData& AssetsData : InCreateAssetsData)
	{
		TResult<void, FMetaHumanCaptureError> Result = CheckCreatedTakeAssets_GameThread(AssetsData);

		const FMetaHumanTake* FoundTake = InTakes.FindByPredicate([TakeId = AssetsData.TakeId](const FMetaHumanTake& InElem)
		{
			return InElem.TakeId == TakeId;
		});

		checkf(FoundTake, TEXT("Take for created assets doesn't exist"));

		if (Result.IsError())
		{
			FText Message = FText::Format(LOCTEXT("IngestError_Validation_IngestedFilesMessage", "Validation of ingested files failed: {0}"), 
										  FText::FromString(Result.GetError().GetMessage()));

			FPerTakeResult TakeResult = FPerTakeResult(AssetsData.TakeId, FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
			InPerTakeCallback(MoveTemp(TakeResult));

			continue;
		}

		Result = CheckCreatedTakeStruct_GameThread(*FoundTake, !AssetsData.AudioClips.IsEmpty());

		if (Result.IsError())
		{
			FText Message = FText::Format(LOCTEXT("IngestError_Validation_TakeStructureMessage", "Validation of created Take structure failed: {0}"),
										  FText::FromString(Result.GetError().GetMessage()));

			FPerTakeResult TakeResult = FPerTakeResult(AssetsData.TakeId, FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString()));
			InPerTakeCallback(MoveTemp(TakeResult));

			continue;
		}

		FPerTakeResult TakeResult = FPerTakeResult(AssetsData.TakeId, Result);
		InPerTakeCallback(MoveTemp(TakeResult));
	}
}

TResult<void, FMetaHumanCaptureError> FIngestAssetCreator::CheckCreatedTakeAssets_GameThread(const FCreateAssetsData& InCreateAssetsData)
{
	IFileManager& FileManager = IFileManager::Get();

	for (const FCreateAssetsData::FViewData& ViewData : InCreateAssetsData.Views)
	{
		UImgMediaSource* ImageSequence = GetAssetIfExists<UImgMediaSource>(InCreateAssetsData.PackagePath, ViewData.Video.Name);
		CHECK_UOBJECT_RET(ImageSequence, *ViewData.Video.Name);

		UImgMediaSource* DepthSequence = GetAssetIfExists<UImgMediaSource>(InCreateAssetsData.PackagePath, ViewData.Depth.Name);
		CHECK_UOBJECT_RET(DepthSequence, *ViewData.Depth.Name);

		FString ImageSequencePath = ImageSequence->GetFullPath();
		FString DepthSequencePath = DepthSequence->GetFullPath();

		CHECK_RET(FPaths::DirectoryExists(ImageSequencePath), TEXT("Image Sequence directory doesn't exist"));
		CHECK_RET(FPaths::DirectoryExists(DepthSequencePath), TEXT("Depth Sequence directory doesn't exist"));

		TArray<FString> ImageSequenceFiles;
		CHECK_RET(FImageSequenceUtils::GetImageSequenceFilesFromPath(*ImageSequencePath, ImageSequenceFiles), TEXT("No supported image files could be found"));

		TArray<FString> DepthSequenceFiles;
		CHECK_RET(FImageSequenceUtils::GetImageSequenceFilesFromPath(*DepthSequencePath, DepthSequenceFiles), TEXT("No supported depth files could be found"));
	}

	for (const FCreateAssetsData::FAudioData& AudioData : InCreateAssetsData.AudioClips)
	{
		UObject* SoundWave = GetAssetIfExists(InCreateAssetsData.PackagePath, AudioData.Name);
		CHECK_UOBJECT_RET(SoundWave, *AudioData.Name);
	}

	FCreateAssetsData::FCalibrationData Calibration = InCreateAssetsData.Calibration;
	UCameraCalibration* CalibrationAsset = GetAssetIfExists<UCameraCalibration>(InCreateAssetsData.PackagePath, Calibration.Name);
	CHECK_UOBJECT_RET(CalibrationAsset, *Calibration.Name);

	for (const FExtendedLensFile& ExtendedLensFile : CalibrationAsset->CameraCalibrations)
	{
		CHECK_UOBJECT_RET(ExtendedLensFile.LensFile, TEXT("LensFile"));
	}

	return ResultOk;
}

TResult<void, FMetaHumanCaptureError> FIngestAssetCreator::CheckCreatedTakeStruct_GameThread(const FMetaHumanTake& InCreatedTakeStruct, bool bInShouldContainAudio)
{
	for (const FMetaHumanTakeView& TakeView : InCreatedTakeStruct.Views)
	{
		CHECK_RET(TakeView.Video, TEXT("Image Sequence asset not linked to the take"));
		CHECK_RET(TakeView.Depth, TEXT("Depth Sequence asset not linked to the take"));
	}

	CHECK_RET(InCreatedTakeStruct.CameraCalibration, TEXT("Camera Calibration asset not linked to the take"));

	if (bInShouldContainAudio)
	{
		CHECK_RET(InCreatedTakeStruct.Audio, TEXT("Sound Wave asset not linked to the take"));
	}

	return ResultOk;
}

template<typename ArrayElement>
int32 FindArrayIndexByTakeId(const TArray<ArrayElement>& InArray, TakeId InTakeId)
{
	static_assert(TIsClass<ArrayElement>::Value, "Array element must be a struct/class");
	static_assert(TIsIntegral<decltype(ArrayElement::TakeId)>::Value, "Array element must contain TakeId member");

	return InArray.IndexOfByPredicate([InTakeId](const ArrayElement& InElem)
	{
		return InElem.TakeId == InTakeId;
	});
}

void FIngestAssetCreator::DeleteFailedTakes(TArray<TakeId> InTakesToDelete,
											TArray<FMetaHumanTake>& OutTakeList,
											TArray<FCreateAssetsData>& OutCreateAssetDataList)
{
	for (TakeId TakeId : InTakesToDelete)
	{
		const int32 ItemIndex = FindArrayIndexByTakeId(OutTakeList, TakeId);
		const int32 AssetIndex = FindArrayIndexByTakeId(OutCreateAssetDataList, TakeId);

		if (ItemIndex != INDEX_NONE)
		{
			OutTakeList.RemoveAt(ItemIndex);
		}

		if (AssetIndex != INDEX_NONE)
		{
			OutCreateAssetDataList.RemoveAt(AssetIndex);
		}
	}
}

TResult<void, FMetaHumanCaptureError> FIngestAssetCreator::CheckTakeAssets(const FMetaHumanTake& InTake, bool bInHasAudio)
{
	FString Message = TEXT("");
	if (bInHasAudio && !InTake.Audio)
	{
		Message = LOCTEXT("IngestError_Initialize_ImportingAudioAssetsFailed", "Error importing audio assets").ToString();
	}
	else if (!InTake.CameraCalibration)
	{
		Message = LOCTEXT("IngestError_Initialized_ImportCalibrationAssetFailed", "Error importing calibration assets").ToString();
	}

	if (!Message.IsEmpty())
	{
		return FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, MoveTemp(Message));
	}

	return ResultOk;
}

void FIngestAssetCreator::RemoveAssetsByPath(const FString& InPackagePath)
{
	IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> AssetData;
	AssetRegistry.GetAssetsByPath(*(InPackagePath), AssetData);

	if (AssetData.IsEmpty())
	{
		return;
	}

	if (ObjectTools::DeleteAssets(AssetData, false) != AssetData.Num())
	{
		UE_LOG(LogMetaHumanIngestAssetCreator, Warning, TEXT("Not all assets are deleted"));
	}
}

UObject* FIngestAssetCreator::GetAssetIfExists(const FString& InTargetPackagePath, const FString& InAssetName)
{
	IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> AssetData;
	AssetRegistry.GetAssetsByPackageName(*(InTargetPackagePath / InAssetName), AssetData);

	return !AssetData.IsEmpty() ? AssetData[0].GetAsset() : nullptr;
}

UObject* FIngestAssetCreator::GetOrCreateAsset(const FString& InTargetPackagePath, const FString& InAssetName, UClass* InClass)
{
	if (UObject* FoundAsset = GetAssetIfExists(InTargetPackagePath, InAssetName))
	{
		return FoundAsset;
	}
	else
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		return AssetTools.CreateAsset(InAssetName, InTargetPackagePath, InClass, nullptr);
	}
}

#undef LOCTEXT_NAMESPACE