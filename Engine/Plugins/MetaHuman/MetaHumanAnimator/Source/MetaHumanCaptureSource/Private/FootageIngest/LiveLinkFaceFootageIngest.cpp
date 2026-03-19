// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceFootageIngest.h"
#include "LiveLinkFaceMetadata.h"
#include "MetaHumanCaptureSourceLog.h"
#include "MetaHumanEditorSettings.h"

#include "ImgMediaSource.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#include "Dom/JsonObject.h"

#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Algo/Transform.h"
#include "Utils/MetaHumanStringUtils.h"

#include "MetaHumanTrace.h"
#include "MetaHumanViewportModes.h"

#define LOCTEXT_NAMESPACE "LiveLinkFaceFootageIngest"

/////////////////////////////////////////////////////
// FLiveLinkFaceIngestBase

FLiveLinkFaceIngestBase::FLiveLinkFaceIngestBase(bool bInShouldCompressDepthFiles)
	: bShouldCompressDepthFiles(bInShouldCompressDepthFiles)
{
}

void FLiveLinkFaceIngestBase::Shutdown()
{
	// Cancel the startup in case it is running
	TArray<TakeId> EmptyList;
	CancelProcessing(EmptyList); //cancels all takes from the source
}

int32 FLiveLinkFaceIngestBase::GetNumTakes() const
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	return TakeInfoCache.Num();
}

TArray<TakeId> FLiveLinkFaceIngestBase::GetTakeIds() const
{
	TArray<TakeId> TakeIds;

	FScopeLock Lock(&TakeInfoCacheMutex);
	TakeInfoCache.GetKeys(TakeIds);
	return TakeIds;
}

FMetaHumanTakeInfo FLiveLinkFaceIngestBase::GetTakeInfo(TakeId InId) const
{
	FScopeLock Lock(&TakeInfoCacheMutex);

	check(TakeInfoCache.Contains(InId));
	const FLiveLinkFaceTakeInfo& LiveLinkTakeInfo = TakeInfoCache[InId];
	return LiveLinkTakeInfo.ConvertToMetaHumanTakeInfo(GetTakesOriginDirectory());
}

void FLiveLinkFaceIngestBase::GetTake(TakeId InTaskTakeId, FTakeConversionTaskInfo& OutTaskInfo)
{
	OutTaskInfo.TakeInfo = GetLiveLinkFaceTakeInfo(InTaskTakeId);

	if (TakeIngestStopTokens[InTaskTakeId].IsStopRequested())
	{
		OutTaskInfo.bCanceled = true;
		return;
	}

	FString TakesTargetRelativeDirectory = GetTakeIngestRelativePath(OutTaskInfo.TakeInfo);

	// Determine the target location where to place the extracted image sequences
	const FString TargetIngestDirectory = TargetIngestBaseDirectory / TakesTargetRelativeDirectory;
	const FString TargetIngestPackagePath = TargetIngestBasePackagePath / TakesTargetRelativeDirectory;

	if (!IFileManager::Get().DirectoryExists(*TargetIngestDirectory))
	{
		const bool bMakeTree = true;
		if (!IFileManager::Get().MakeDirectory(*TargetIngestDirectory, bMakeTree))
		{
			// TODO: Raise this as an error that can be displayed to the user. Can be done using a delegate or stored to be retrieved later
			OutTaskInfo.bHasErrors = true;
			OutTaskInfo.ErrorText = FText::Format(LOCTEXT("IngestDirectoryError", "Unable to create ingest directory '{0}' for take '{1}'"), 
				FText::FromString(TargetIngestDirectory), FText::FromString(OutTaskInfo.TakeInfo.GetTakeName()));
			return;
		}
	}


	FLiveLinkFaceTakeDataConverter TakeDataConverter;

	TakeDataConverter.OnProgress().BindLambda([this, &OutTaskInfo, InTaskTakeId](float InDataConverterProgress)
		{
			OutTaskInfo.Progress.store(InDataConverterProgress, std::memory_order_relaxed);

			float IndividualTaskProgress = OutTaskInfo.Progress.load(std::memory_order_relaxed);

			// Store the progress so it can be visualized in the UI
			TakeProgress[InTaskTakeId].store(IndividualTaskProgress, std::memory_order_relaxed);
		});

	TakeDataConverter.OnFinished().BindLambda([this, &OutTaskInfo, InTaskTakeId](TResult<void, FMetaHumanCaptureError> InResult)
		{
			if (InResult.IsValid())
			{
				return;
			}

			FMetaHumanCaptureError Error = InResult.ClaimError();

			if (Error.GetCode() == EMetaHumanCaptureError::AbortedByUser)
			{
				if (!OutTaskInfo.bHasErrors)
				{
					OutTaskInfo.bCanceled = true;
				}
			}
			else
			{
				// There is an error, cancel all tasks for specific take
				TakeIngestStopTokens[InTaskTakeId].RequestStop();

				// Make sure we don't overwrite a previously notified error
				if (!OutTaskInfo.bHasErrors)
				{
					FText ErrorMessagePrefix = FText::Format(LOCTEXT("IngestError_ConversionMessagePrefix", "Conversion of data for take {0} failed"), FText::FromString(OutTaskInfo.TakeInfo.GetTakeName()));

					OutTaskInfo.bHasErrors = true;
					OutTaskInfo.ErrorText = FText::Format(LOCTEXT("IngestError_ConversionMessage", "{0}: {1}"), ErrorMessagePrefix, FText::FromString(Error.GetMessage()));
				}
			}
		});

	FLiveLinkFaceTakeDataConverter::FConvertParams ConvertParams;
	ConvertParams.TakeInfo = OutTaskInfo.TakeInfo;
	ConvertParams.TargetIngestDirectory = TargetIngestDirectory;
	ConvertParams.TargetIngestPackagePath = TargetIngestPackagePath;

	bool bInitialized = TakeDataConverter.Initialize(ConvertParams); // TODO handle errors
	OutTaskInfo.Result = TakeDataConverter.Convert(TakeIngestStopTokens[InTaskTakeId]);
}

bool FLiveLinkFaceIngestBase::IsMetaHumanAnimatorTake(const FString& Directory, const FLiveLinkFaceTakeInfo& TakeInfo)
{		
	for (const FString& ExpectedFile : TakeInfo.TakeMetadata.GetMHAFileNames())
	{
		const FString& FilePath = Directory / ExpectedFile;
		if (!IFileManager::Get().FileExists(*FilePath))
		{
			return false;
		}
	}

	return true;
}

void FLiveLinkFaceIngestBase::GetTakesProcessing(const TArray<TakeId>& InTakeIdList, TPerTakeCallback<void> InCallback, const FStopToken& InStopToken)
{
	TPerTakeCallback<void> PerTakeCallbackInner = TPerTakeCallback<void>([this, UserPerTakeCallback = MoveTemp(InCallback)](TPerTakeResult<void> InResult)
		{
			if (InResult.Value.IsError())
			{
				DeleteDataForTake(InResult.Key);
				RemoveTakeFromIngestCache(InResult.Key);
			}

			UserPerTakeCallback(MoveTemp(InResult));
		}, EDelegateExecutionThread::InternalThread);
	
	const int32 TakesToProcess = InTakeIdList.Num();



	// One entry for each take conversion task
	TArray<FTakeConversionTaskInfo> TaskInfoList;
	TaskInfoList.SetNum(TakesToProcess);

	for (TakeId TakeId : InTakeIdList)
	{
		TakeIngestStopTokens.Add(TakeId, FStopToken());
		TakeProgress[TakeId].store(0.0f, std::memory_order_relaxed);

		{
			FScopeLock lock(&TakeProcessNameMutex);
			TakeProcessName[TakeId] = LOCTEXT("ProgressBarPendingCaption", "Pending...");
		}
	}

	MHA_CPUPROFILER_EVENT_SCOPE(FLiveLinkFaceIngestBase::GetTakes);

	EParallelForFlags ParallelForFlags = EParallelForFlags::Unbalanced;

	// Check to see if we should force ingestion to run in a single thread
	if (GetMutableDefault<UMetaHumanEditorSettings>()->bForceSerialIngestion)
	{
		ParallelForFlags |= EParallelForFlags::ForceSingleThread;
	}

	const int32 BatchSize = CalculateBatchSize(TakesToProcess);
	ParallelFor(TEXT("MetaHuman.CaptureSource.GetTakesAsync"), TakesToProcess, BatchSize, [this, &InTakeIdList, &InStopToken, &TaskInfoList, TakesToProcess](int32 TaskIndex)
		{
			const TakeId TaskTakeId = InTakeIdList[TaskIndex];
			{
				FScopeLock lock(&TakeProcessNameMutex);
				TakeProcessName[TaskTakeId] = LOCTEXT("ProgressBarProcessingCaption", "Processing...");
			}

			// Get a reference to the task info we are processing for this instance of the parallel for
			FTakeConversionTaskInfo& TaskInfo = TaskInfoList[TaskIndex];
			UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Processing take (%d of %d): %s"), TaskIndex + 1, TaskInfoList.Num(), *TaskInfo.TakeInfo.TakeOriginDirectory);

			// convert a single take; this is a virtual method so can be overridden
			GetTake(TaskTakeId, TaskInfo);
		
		}, ParallelForFlags);
	
	// Gather data from the tasks that succeeded and report in the log the ones that didn't
	TArray<FLiveLinkFaceTakeInfo> ConvertedTakes;
	TArray<FLiveLinkFaceTakeDataConverter::FConvertResult> ConvertedResults;
	ConvertedTakes.Reserve(TaskInfoList.Num());
	ConvertedResults.Reserve(TaskInfoList.Num());
	for (const FTakeConversionTaskInfo& TaskInfo : TaskInfoList)
	{
		if (!TaskInfo.bCanceled)
		{
			if (TaskInfo.ErrorText.IsEmpty())
			{
				ConvertedTakes.Add(TaskInfo.TakeInfo);
				ConvertedResults.Add(TaskInfo.Result);
			}
			else
			{
				TPerTakeResult<void> TakeResult = TPerTakeResult<void>(TaskInfo.TakeInfo.Id, FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, TaskInfo.ErrorText.ToString()));
				PerTakeCallbackInner(MoveTemp(TakeResult));
			}
		}
		else
		{
			TResult<void, FMetaHumanCaptureError> TakeResult = ResultOk;
			// Cancellation can come as a conequence of another error being detected, so we need to make sure we
			// don't overwrite the original error if there was one.
			if (!TaskInfo.bHasErrors)
			{
				FText Message = LOCTEXT("IngestError_Cancellation", "The ingest was aborted by the user");
				TakeResult = FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser, Message.ToString());
			}
			else
			{
				TakeResult = FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, TaskInfo.ErrorText.ToString());
			}
			PerTakeCallbackInner(TPerTakeResult<void>(TaskInfo.TakeInfo.Id, MoveTemp(TakeResult)));
		}
	}

	TakeIngestStopTokens.Empty();

	if (!ConvertedTakes.IsEmpty())
	{
		TArray<FMetaHumanTake> Takes;

		switch (Mode)
		{
		case ETakeIngestMode::Async:
		{
			TPromise<TArray<FMetaHumanTake>> TakePromise;

			// Need to run the asset creation function in the game thread
			AsyncTask(ENamedThreads::GameThread, [this, PerTakeCallback = MoveTemp(PerTakeCallbackInner), &TakePromise, &ConvertedTakes, &ConvertedResults]() mutable
				{
					TArray<FCreateAssetsData> CreateAssetsList = PrepareTakeAssets_GameThread(ConvertedResults, ConvertedTakes);

					TArray<FMetaHumanTake> Takes;

					FIngestAssetCreator::CreateAssets_GameThread(CreateAssetsList, Takes, MoveTemp(PerTakeCallback));

					TakePromise.SetValue(Takes);
				});

			Takes = TakePromise.GetFuture().Get();
			break;
		}

		case ETakeIngestMode::Blocking:
		{
			check(IsInGameThread());
			TArray<FCreateAssetsData> CreateAssetsList = PrepareTakeAssets_GameThread(ConvertedResults, ConvertedTakes);

			FIngestAssetCreator::CreateAssets_GameThread(CreateAssetsList, Takes, MoveTemp(PerTakeCallbackInner));
			break;
		}
		default:
			break;
		}

		{
			FScopeLock CurrentIngestedLock(&CurrentIngestedTakeMutex);
			CurrentIngestedTakes = MoveTemp(Takes);
		}
	}
}

void FLiveLinkFaceIngestBase::GetTakes(const TArray<TakeId>& InTakeIdList, TPerTakeCallback<void> InCallback)
{
	ProcessTakes([TakeIdList = InTakeIdList, PerTakeCallback = MoveTemp(InCallback), this](const FStopToken& InStopToken) mutable
		{
			GetTakesProcessing(TakeIdList, PerTakeCallback, InStopToken);
		});
}

int32 FLiveLinkFaceIngestBase::CalculateBatchSize(int32 InTakesToProcess)
{
	const int32 WorkersAvailable = LowLevelTasks::FScheduler::Get().GetNumWorkers();

	const int32 NumOfTakesInParallel = WorkersAvailable / 3; // 3 async tasks per take

	return FMath::CeilToInt((float)InTakesToProcess / NumOfTakesInParallel);
}

FString FLiveLinkFaceIngestBase::GetTakeIngestRelativePath(const FLiveLinkFaceTakeInfo& InTakeInfo) const
{
	const FString& TakesOriginDirectory = GetTakesOriginDirectory();
	const FString TakePath = InTakeInfo.GetTakePath();

	// This line means that the entire subtree relative to the input directory will be recreated
	// This ensures no conflicts will happen due file names that clash, but it also means longer paths
	// which might cause issues when cooking
	FString TakesTargetRelativeDirectory = InTakeInfo.TakeOriginDirectory.Mid(TakesOriginDirectory.Len());
	if (TakesTargetRelativeDirectory.IsEmpty())
	{
		TakesTargetRelativeDirectory = TakePath;
	}

	return TakesTargetRelativeDirectory;
}

void FLiveLinkFaceIngestBase::DeleteDataForTake(TakeId InId)
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	check(TakeInfoCache.Contains(InId));
	const FLiveLinkFaceTakeInfo& TakeToDelete = TakeInfoCache[InId];

	FString TakesTargetRelativeDirectory = GetTakeIngestRelativePath(TakeToDelete);

	const FString PathToDirectory = TargetIngestBaseDirectory / TakesTargetRelativeDirectory;
	const FString PathToAssets = TargetIngestBasePackagePath / TakesTargetRelativeDirectory;

	ExecuteFromGameThread(TEXT("TakeDataDeletion"), [PathToAssets, PathToDirectory]()
	{
		FIngestAssetCreator::RemoveAssetsByPath(PathToAssets);
		IFileManager::Get().DeleteDirectory(*PathToDirectory, true, true);
	});
}

TakeId FLiveLinkFaceIngestBase::AddTakeInfo(FLiveLinkFaceTakeInfo&& InTakeInfo)
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	TakeId NewTakeId = GenerateNewTakeId();
	InTakeInfo.Id = NewTakeId;
	TakeInfoCache.Emplace(NewTakeId, Forward<FLiveLinkFaceTakeInfo>(InTakeInfo));

	TakeProgress.Emplace(NewTakeId, 0);
	TakeProgressFrameCount.Emplace(NewTakeId, 0);
	TakeProgressTotalFrames.Emplace(NewTakeId, 0);

	FScopeLock ProcessNameLock(&TakeProcessNameMutex);
	TakeProcessName.Emplace(NewTakeId, FText());

	return NewTakeId;
}

TakeId FLiveLinkFaceIngestBase::GenerateNewTakeId()
{
	return ++CurrId;
}

int32 FLiveLinkFaceIngestBase::ClearTakeInfoCache()
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	int32 PreviousTakeCount = TakeInfoCache.Num();
	TakeInfoCache.Empty();
	TakeIngestStopTokens.Empty();
	TakeProgress.Empty();
	TakeProgressFrameCount.Empty();
	TakeProgressTotalFrames.Empty();

	FScopeLock ProcessNameLock(&TakeProcessNameMutex);
	TakeProcessName.Empty();

	return PreviousTakeCount;
}

void FLiveLinkFaceIngestBase::RemoveTakeFromTakeCache(TakeId InTakeId)
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	TakeInfoCache.Remove(InTakeId);
	TakeProgress.Remove(InTakeId);
	TakeProgressFrameCount.Remove(InTakeId);
	TakeProgressTotalFrames.Remove(InTakeId);

	FScopeLock ProcessNameLock(&TakeProcessNameMutex);
	TakeProcessName.Remove(InTakeId);
}

void FLiveLinkFaceIngestBase::CancelProcessing(const TArray<TakeId>& InIdList)
{
	if (!InIdList.IsEmpty())
	{
		for (TakeId TakeId : InIdList)
		{
			TakeIngestStopTokens[TakeId].RequestStop();
		}
	}
	else
	{				
		//first set the individual flags to aborted
		for (TPair<TakeId, FStopToken> StopToken: TakeIngestStopTokens)
		{
			StopToken.Value.RequestStop();
		}

		//cancel all processing
		FFootageIngest::CancelProcessing(InIdList);
	}
}

FLiveLinkFaceTakeInfo FLiveLinkFaceIngestBase::GetLiveLinkFaceTakeInfo(TakeId InId) const
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	return TakeInfoCache[InId];
}

/**
 * Creates a FMetaHumanTake from the result of a ILiveLinkFaceTakeDataConverter::Convert call.
 * This function needs to be called from the GameThread, hence the _GameThread suffix
 */
TArray<FCreateAssetsData> FLiveLinkFaceIngestBase::PrepareTakeAssets_GameThread(const TArray<FLiveLinkFaceTakeDataConverter::FConvertResult>& InConvertResultList, const TArray<FLiveLinkFaceTakeInfo>& InTakeInfoList)
{
	check(InConvertResultList.Num() == InTakeInfoList.Num());

	TArray<FCreateAssetsData> CreateAssetDataList;
	for (int32 Index = 0; Index < InConvertResultList.Num(); ++Index)
	{
		const FLiveLinkFaceTakeDataConverter::FConvertResult& ConvertResult = InConvertResultList[Index];
		const FLiveLinkFaceTakeInfo& TakeInfo = InTakeInfoList[Index];

		FCreateAssetsData CreateAssetData;
		FCreateAssetsData::FViewData ViewData;
		FCreateAssetsData::FImageSequenceData ImageSequenceData;
		FCreateAssetsData::FAudioData AudioData;

		CreateAssetData.TakeId = TakeInfo.Id;
		CreateAssetData.PackagePath = ConvertResult.TargetIngestPackagePath;

		const FString TakeName = TakeInfo.GetTakeName();

		ImageSequenceData.FrameRate = TakeInfo.DepthMetadata.FrameRate;
		ImageSequenceData.Name = FString::Format(TEXT("{0}_RGB_MediaSource"), { TakeName });
		ImageSequenceData.SequenceDirectory = ConvertResult.ImageSequenceDirectory;
		ViewData.Video = ImageSequenceData;
		ViewData.Video.bTimecodePresent = ConvertResult.bVideoTimecodePresent;
		ViewData.Video.Timecode = ConvertResult.VideoTimecode;
		ViewData.Video.TimecodeRate= ConvertResult.TimecodeRate;

		ImageSequenceData.Name = FString::Format(TEXT("{0}_Depth_MediaSource"), { TakeName });
		ImageSequenceData.SequenceDirectory = ConvertResult.DepthSequenceDirectory;
		ViewData.Depth = ImageSequenceData;
		ViewData.Depth.bTimecodePresent = ConvertResult.bVideoTimecodePresent;
		ViewData.Depth.Timecode = ConvertResult.VideoTimecode;
		ViewData.Depth.TimecodeRate = ConvertResult.TimecodeRate;

		CreateAssetData.Views.Add(ViewData);

		AudioData.Name = FString::Format(TEXT("{0}_Audio"), { TakeName });
		AudioData.WAVFile = ConvertResult.WAVFilePath;
		AudioData.Timecode = ConvertResult.AudioTimecode;
		AudioData.TimecodeRate = ConvertResult.TimecodeRate;
		CreateAssetData.AudioClips.Add(AudioData);

		CreateAssetData.Calibration.Name = FString::Format(TEXT("{0}_Calibration"), { TakeName });
		CreateAssetData.Calibration.CalibrationFile = TakeInfo.GetCameraCalibrationFilePath();

		CreateAssetData.CaptureExcludedFrames = ConvertResult.CaptureExcludedFrames;

		CreateAssetDataList.Add(MoveTemp(CreateAssetData));
	}

	return CreateAssetDataList;
}

/////////////////////////////////////////////////////
// FLiveLinkFaceArchiveIngest

FLiveLinkFaceArchiveIngest::FLiveLinkFaceArchiveIngest(const FString& InInputDirectory, bool bInShouldCompressDepthFiles)
	: FLiveLinkFaceIngestBase(bInShouldCompressDepthFiles)
	, InputDirectory{ InInputDirectory }
{
	FPaths::NormalizeDirectoryName(InputDirectory);
}

FLiveLinkFaceArchiveIngest::~FLiveLinkFaceArchiveIngest()
{
	Shutdown();

	ProcessTakesAsyncTask = nullptr;
}

void FLiveLinkFaceArchiveIngest::Startup(ETakeIngestMode InMode)
{
	FLiveLinkFaceIngestBase::Startup(InMode);

	FConnectionChangedEvent::EState ConnState = FPaths::DirectoryExists(*InputDirectory) ?
		FConnectionChangedEvent::EState::Connected : FConnectionChangedEvent::EState::Disconnected;

	PublishEvent<FConnectionChangedEvent>(ConnState);
}

void FLiveLinkFaceArchiveIngest::Shutdown()
{
	FLiveLinkFaceIngestBase::Shutdown();

	RefreshTakeListTask ? RefreshTakeListTask->Abort() : void();
}

void FLiveLinkFaceArchiveIngest::RefreshTakeListAsync(IFootageIngestAPI::TCallback<void> InCallback)
{
	int32 PreviousTakeCount = ClearTakeInfoCache();
	
	if (PreviousTakeCount != 0)
	{
		PublishEvent<FTakeListResetEvent>();
	}

	RefreshTakeListTask = MakeUnique<FAbortableAsyncTask>([this, ClientCallback = MoveTemp(InCallback)](const FStopToken& InStopToken) mutable {
		TResult<void, FMetaHumanCaptureError> Result = ReadTakeList(InStopToken);
		ClientCallback(MoveTemp(Result));
		});
	switch (Mode)
	{
	case ETakeIngestMode::Async:
		RefreshTakeListTask->StartAsync();
		break;

	case ETakeIngestMode::Blocking:
		RefreshTakeListTask->StartSync();
		break;

	default:
		break;
	}
}

FLiveLinkFaceTakeInfo FLiveLinkFaceIngestBase::ReadTake(const FString& CurrentDirectory) {
	FLiveLinkFaceTakeInfo TakeInfo;
	
	// All Live Link takes should contain take info, video metadata and audio metadata
	// If we fail to parse any of these for this take directory then abort processing.
	if (!FLiveLinkFaceMetadataParser::ParseTakeInfo(CurrentDirectory, TakeInfo))
	{
		TakeInfo.Issues.Emplace(LOCTEXT("IngestError_TakeInfoParsingFailed", "Failed to parse take info"));
	}

	if (!FLiveLinkFaceMetadataParser::ParseVideoMetadata(CurrentDirectory, TakeInfo.VideoMetadata))
	{
		TakeInfo.Issues.Emplace(LOCTEXT("IngestError_VideoMetadataFailed", "Failed to parse video metadata"));
	}

	if (!FLiveLinkFaceMetadataParser::ParseAudioMetadata(CurrentDirectory, TakeInfo.AudioMetadata))
	{
		TakeInfo.Issues.Emplace(LOCTEXT("IngestError_AudioMetadataParsingFailed", "Failed to parse audio metadata"));
	}

	FLiveLinkFaceMetadataParser::ParseThumbnail(CurrentDirectory, TakeInfo);

	TOptional<FText> Result = TakeDurationExceedsLimit(TakeInfo.GetTakeDurationInSeconds());
	if (Result.IsSet())
	{
		TakeInfo.Issues.Emplace(Result.GetValue());

		UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Allowed limit can be extended using \"au.SoundWaveImportLengthLimitInSeconds\""));
	}

	FString FolderName = FPaths::GetPathLeaf(CurrentDirectory);
	if (MetaHumanStringContainsWhitespace(FolderName))
	{
		TakeInfo.Issues.Emplace(LOCTEXT("IngestError_TakeFolderContainsWhiteSpace", "Take Folder contains white space character(s)"));
	}

	if (MetaHumanStringContainsWhitespace(TakeInfo.TakeMetadata.Subject))
	{
		TakeInfo.Issues.Emplace(LOCTEXT("IngestError_SubjectContainsWhiteSpace", "Subject contains white space character(s)"));
	}

	if (MetaHumanStringContainsWhitespace(TakeInfo.TakeMetadata.SlateName))
	{
		TakeInfo.Issues.Emplace(LOCTEXT("IngestError_SlateNameContainsWhiteSpace", "Slate name contains white space character(s)"));
	}

	if (!FCString::IsPureAnsi(*CurrentDirectory))
	{
		TakeInfo.Issues.Emplace(LOCTEXT("IngestError_UnsupportedCharactersInTakeDirectoryPath", "Take path contains unsupported text characters"));
	}

	const FString& SlateName = TakeInfo.TakeMetadata.SlateName;
	if (!FCString::IsPureAnsi(*SlateName))
	{
		const FText Message = LOCTEXT("IngestError_UnsupportedCharactersInSlateName", "Slate name '{0}' contains unsupported text characters");
		TakeInfo.Issues.Emplace(FText::Format(Message, FText::FromString(SlateName)));
	}

	const FString& Subject = TakeInfo.TakeMetadata.Subject;
	if (!FCString::IsPureAnsi(*Subject))
	{
		const FText Message = LOCTEXT("IngestError_UnsupportedCharactersInSubjectName", "Subject name '{0}' contains unsupported text characters");
		TakeInfo.Issues.Emplace(FText::Format(Message, FText::FromString(Subject)));
	}

	if (IsMetaHumanAnimatorTake(CurrentDirectory, TakeInfo))
	{
		// Only parse depth metadata if this is MHA take.
		// TODO: Evaluate the need to parse the depth metadata here. We rely on importing the calibration directly from the file
		// so parsing this information here might not actually be necessary
		TakeInfo.DepthMetadata.bShouldCompressFiles = bShouldCompressDepthFiles;

		if (!FLiveLinkFaceMetadataParser::ParseDepthMetadata(CurrentDirectory, TakeInfo.DepthMetadata))
		{
			TakeInfo.Issues.Emplace(LOCTEXT("IngestError_DepthMetadataFailed", "Failed to parse depth metadata"));
		}
	} 
	else 
	{
		// If this is not an MHA take we want it to appear in the capture manager even if it cannot be ingested.
		TakeInfo.Issues.Emplace(LOCTEXT("IngestError_UnsupportedTakeFormat", "Unsupported take format"));
	}

	return TakeInfo;
}

TResult<void, FMetaHumanCaptureError> FLiveLinkFaceArchiveIngest::ReadTakeList(const FStopToken& InStopToken)
{
	TResult<void, FMetaHumanCaptureError> Result = ResultOk;

	// Count the number of potential takes available
	TArray<FString> TakeDirectories;
	const bool bIterationResult = IFileManager::Get().IterateDirectoryRecursively(*InputDirectory, [&](const TCHAR* InFileNameOrDirectory, bool bInIsDirectory)
	{
		if (InStopToken.IsStopRequested())
		{
			Result = FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser);
			return false;
		}

		if (!bInIsDirectory)
		{
			const FString CurrentDirectory = FPaths::GetPath(InFileNameOrDirectory);
			const FString CurrentFileName = FPaths::GetCleanFilename(InFileNameOrDirectory);
			if (CurrentFileName == FLiveLinkFaceStaticFileNames::TakeMetadata)
			{
				TakeDirectories.Add(CurrentDirectory);
			}
		}

		return true;
	});

	if (Result.IsError())
	{
		return Result;
	}
	else if (!bIterationResult)
	{
		return FMetaHumanCaptureError(EMetaHumanCaptureError::InvalidArguments, "Invalid path to the takes directory.");
	}

	// Start parsing takes
	const int32 NumTakeDirectories = TakeDirectories.Num();
	for (int32 DirectoryIndex = 0; DirectoryIndex < NumTakeDirectories; ++DirectoryIndex)
	{
		if (InStopToken.IsStopRequested())
		{
			return FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser);
		}

		const FString& CurrentDirectory = TakeDirectories[DirectoryIndex];

		UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Parsing recording in folder (%d of %d): %s"), DirectoryIndex + 1, NumTakeDirectories, *CurrentDirectory);

		FLiveLinkFaceTakeInfo TakeInfo = ReadTake(CurrentDirectory);

		TakeId NewTakeId = AddTakeInfo(MoveTemp(TakeInfo));
		PublishEvent<FNewTakesAddedEvent>(NewTakeId);
	}

	return ResultOk;
}

const FString& FLiveLinkFaceArchiveIngest::GetTakesOriginDirectory() const
{
	return InputDirectory;
}

#undef LOCTEXT_NAMESPACE
