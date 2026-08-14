// Copyright Epic Games, Inc. All Rights Reserved.

#include "FileFootageIngest.h"
#include "MetaHumanCaptureSourceLog.h"
#include "MetaHumanCaptureEvents.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#include "Async/Async.h"
#include "Algo/Transform.h"
#include "Templates/Atomic.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

#define LOCTEXT_NAMESPACE "FootageIngest"

static constexpr float WaitOnFutureTimeout = 0.3; // seconds

FFileFootageIngest::FFileFootageIngest(const FString& InInputDirectory)
	: FFootageIngest()
	, InputDirectory{ InInputDirectory }
{
	FPaths::NormalizeDirectoryName(InputDirectory);
}

FFileFootageIngest::~FFileFootageIngest()
{
	Shutdown();

	ProcessTakesAsyncTask = nullptr;
	RefreshTakeListTask = nullptr;
}

void FFileFootageIngest::Startup(ETakeIngestMode InMode)
{
	FFootageIngest::Startup(InMode);

	FConnectionChangedEvent::EState ConnState = FPaths::DirectoryExists(*InputDirectory) ?
		FConnectionChangedEvent::EState::Connected : FConnectionChangedEvent::EState::Disconnected;

	PublishEvent<FConnectionChangedEvent>(ConnState);
}

void FFileFootageIngest::RefreshTakeListAsync(IFootageIngestAPI::TCallback<void> InCallback)
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

void FFileFootageIngest::Shutdown()
{
	// Cancel the startup in case it is running
	TArray<int32> EmptyList;
	CancelProcessing(EmptyList); //cancels all the takes for the capture source

	RefreshTakeListTask ? RefreshTakeListTask->Abort() : void();
}

int32 FFileFootageIngest::GetNumTakes() const
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	return TakeInfoCache.Num();
}

TArray<TakeId> FFileFootageIngest::GetTakeIds() const
{
	TArray<TakeId> TakeIds;

	FScopeLock Lock(&TakeInfoCacheMutex);
	TakeInfoCache.GetKeys(TakeIds);
	return TakeIds;
}

FMetaHumanTakeInfo FFileFootageIngest::GetTakeInfo(TakeId InId) const
{
	FScopeLock Lock(&TakeInfoCacheMutex);

	check(TakeInfoCache.Contains(InId));
	return TakeInfoCache[InId];
}

void FFileFootageIngest::GetTakesProcessing(const TArray<TakeId>& InTakeIdList, TPerTakeCallback<void> InCallback, const FStopToken& InStopToken)
{
	TPerTakeCallback<void> PerTakeCallbackInner = TPerTakeCallback<void>([this, UserPerTakeCallback = MoveTemp(InCallback)](TPerTakeResult<void> InResult)
		{
			if (InResult.Value.IsError() && InResult.Value.GetError().GetCode() != EMetaHumanCaptureError::Warning)
			{
				DeleteDataForTake(InResult.Key);
				RemoveTakeFromIngestCache(InResult.Key);
			}

			UserPerTakeCallback(MoveTemp(InResult));
		}, EDelegateExecutionThread::InternalThread);

	for (TakeId Id : InTakeIdList)
	{
		const FMetaHumanTakeInfo TakeInfo = GetCachedTakeInfo(Id);
		TakeProgressFrameCount[Id].store(0, std::memory_order_relaxed);
		TakeProgressTotalFrames[Id].store(TakeInfo.NumFrames, std::memory_order_relaxed);
		TakeProgress[Id].store(0.0f, std::memory_order_relaxed);

		TakeIngestStopTokens.Add(Id, FStopToken());

		FScopeLock Lock(&TakeProcessNameMutex);
		TakeProcessName[Id] = LOCTEXT("ProgressBarPendingCaption", "Pending...");
	}

	TArray<FCreateAssetsData> CreateAssetsDataList;
	for (int32 TakeEntryIndex = 0; TakeEntryIndex < InTakeIdList.Num(); ++TakeEntryIndex)
	{
		const TakeId TakeId = InTakeIdList[TakeEntryIndex];

		if (InStopToken.IsStopRequested() || TakeIngestStopTokens[TakeId].IsStopRequested())
		{
			FText Message = LOCTEXT("IngestError_Cancellation", "The ingest was aborted by the user");
			TPerTakeResult<void> TakeResult = TPerTakeResult<void>(TakeId, FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser, Message.ToString()));
			PerTakeCallbackInner(MoveTemp(TakeResult));

			continue;
		}

		if (TakeProgressTotalFrames[TakeId] > 0)
		{
			TakeProgress[TakeId].store(float(TakeProgressFrameCount[TakeId]) / TakeProgressTotalFrames[TakeId], std::memory_order_relaxed);
		}
		else
		{
			TakeProgress[TakeId].store(0.0f, std::memory_order_relaxed);
		}

		const FMetaHumanTakeInfo TakeInfo = GetCachedTakeInfo(TakeId);
		const FString& TakeName = TakeInfo.Name;

		UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Processing take (%d of %d): %s"), TakeEntryIndex + 1, InTakeIdList.Num(), *TakeInfo.Name);

		{
			FScopeLock Lock(&TakeProcessNameMutex);
			TakeProcessName[TakeId] = LOCTEXT("ProgressBarProcessingCaption", "Processing...");
		}

		FCreateAssetsData CreateAssetsData;
		TResult<void, FMetaHumanCaptureError> Result = CreateAssets(TakeInfo, TakeIngestStopTokens[TakeId], CreateAssetsData);

		if (Result.IsValid())
		{
			CreateAssetsDataList.Add(CreateAssetsData);
		}
		else
		{
			FMetaHumanCaptureError Error = Result.ClaimError();
			FString Message;
			if (Error.GetCode() == EMetaHumanCaptureError::Warning)
			{
				CreateAssetsDataList.Add(CreateAssetsData);

				Message = FText::Format(LOCTEXT("FileFootageIngest_IngestWarning", "Warning occurred while ingesting take {0}: {1}"), FText::FromString(TakeInfo.Name), FText::FromString(Error.GetMessage())).ToString();
			}
			else
			{
				Message = FText::Format(LOCTEXT("FileFootageIngest_IngestError", "Error occurred while ingesting take {0}: {1}"), FText::FromString(TakeInfo.Name), FText::FromString(Error.GetMessage())).ToString();
			}
			EMetaHumanCaptureError Status;
			if (TakeIngestStopTokens[TakeId].IsStopRequested())
			{
				Status = EMetaHumanCaptureError::AbortedByUser;
			}
			else
			{
				Status = Error.GetCode();
			}

			TPerTakeResult<void> TakeResult = TPerTakeResult<void>(TakeId, FMetaHumanCaptureError(Status, MoveTemp(Message)));
			PerTakeCallbackInner(MoveTemp(TakeResult));
		}
	}

	TakeIngestStopTokens.Empty();

	if (!InStopToken.IsStopRequested())
	{
		TArray<FMetaHumanTake> Takes;

		if (Mode == ETakeIngestMode::Async)
		{
			TPromise<TArray<FMetaHumanTake>> TakePromise;

			// Need to run the asset creation function in the game thread
			AsyncTask(ENamedThreads::GameThread, [PerTakeCallback = MoveTemp(PerTakeCallbackInner), &TakePromise, &CreateAssetsDataList, this]() mutable
				{
					TArray<FMetaHumanTake> Takes;
					FIngestAssetCreator::CreateAssets_GameThread(CreateAssetsDataList, Takes, MoveTemp(PerTakeCallback));

					TakePromise.SetValue(Takes);
				});

			// Wait for the asset creation function to finish in the game thread
			Takes = TakePromise.GetFuture().Get();
		}
		else
		{
			check(IsInGameThread());
			FIngestAssetCreator::CreateAssets_GameThread(CreateAssetsDataList, Takes, MoveTemp(PerTakeCallbackInner));
		}

		{
			FScopeLock CurrentIngestedLock(&CurrentIngestedTakeMutex);
			CurrentIngestedTakes = MoveTemp(Takes);
		}
	}
}

void FFileFootageIngest::GetTakes(const TArray<TakeId>& InIdList, TPerTakeCallback<void> InCallback)
{
	ProcessTakes([TakeIdList = InIdList, PerTakeCallback = MoveTemp(InCallback), this](const FStopToken& InStopToken) mutable
	{
		GetTakesProcessing(TakeIdList, PerTakeCallback, InStopToken);
	});
}

void FFileFootageIngest::AddTakeInfo(FMetaHumanTakeInfo&& InTakeInfo)
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	check(!TakeInfoCache.Contains(InTakeInfo.Id));
	TakeInfoCache.Emplace(InTakeInfo.Id, Forward<FMetaHumanTakeInfo>(InTakeInfo));

	TakeProgress.Emplace(InTakeInfo.Id, 0.0f);
	TakeProgressFrameCount.Emplace(InTakeInfo.Id, 0);
	TakeProgressTotalFrames.Emplace(InTakeInfo.Id, 0);

	FScopeLock ProcessNameLock(&TakeProcessNameMutex);
	TakeProcessName.Emplace(InTakeInfo.Id, FText());
}

TakeId FFileFootageIngest::GenerateNewTakeId()
{
	return ++CurrId;
}

FMetaHumanTakeInfo FFileFootageIngest::GetCachedTakeInfo(TakeId InId) const
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	return TakeInfoCache[InId];
}

int32 FFileFootageIngest::ClearTakeInfoCache()
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

void FFileFootageIngest::CancelProcessing(const TArray<TakeId>& InIdList)
{
	bool bShouldCancelPipeline = false;
	if (!InIdList.IsEmpty())
	{
		for (TakeId Id : InIdList)
		{
			TakeIngestStopTokens[Id].RequestStop();

			if (Id == TakeIdInPipeline)
			{
				bShouldCancelPipeline = true;
			}
		}
	}
	else
	{
		for (TPair<TakeId, FStopToken> StopToken : TakeIngestStopTokens)
		{
			StopToken.Value.RequestStop();
		}
		//call the base class with an empty list to cancel all
		FFootageIngest::CancelProcessing(InIdList);

		bShouldCancelPipeline = true;
	}

	if (Pipeline.IsRunning() && bShouldCancelPipeline)
	{
		Pipeline.Cancel();
	}
}

TResult<void, FMetaHumanCaptureError> FFileFootageIngest::RunPipeline(const FStopToken& InStopToken, TakeId InTakeId, bool bInShouldRunMultiThreaded)
{
	TResult<void, FMetaHumanCaptureError> Result = ResultOk;

	UE::MetaHuman::Pipeline::FFrameComplete OnFrameComplete;
	UE::MetaHuman::Pipeline::FProcessComplete OnProcessComplete;

	OnFrameComplete.AddRaw(this, &FFileFootageIngest::FrameComplete, InTakeId);
	OnProcessComplete.AddRaw(this, &FFileFootageIngest::ProcessComplete, TRetainedRef<TResult<void, FMetaHumanCaptureError>>(Result));

	UE::MetaHuman::Pipeline::FPipelineRunParameters PipelineRunParameters;
	if (bInShouldRunMultiThreaded)
	{
		PipelineRunParameters.SetMode(UE::MetaHuman::Pipeline::EPipelineMode::PushSyncNodes);
	}
	else
	{
		PipelineRunParameters.SetMode(UE::MetaHuman::Pipeline::EPipelineMode::PushSync);
	}

	PipelineRunParameters.SetOnFrameComplete(OnFrameComplete);
	PipelineRunParameters.SetOnProcessComplete(OnProcessComplete);
	PipelineRunParameters.SetRestrictStartingToGameThread(false);

	// Blocking function
	TakeIdInPipeline = InTakeId;
	Pipeline.Run(PipelineRunParameters);

	if (InStopToken.IsStopRequested() && Result.IsValid())
	{
		Result = FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser);
	}

	TakeIdInPipeline = INVALID_ID;

	return Result;
}

void FFileFootageIngest::FrameComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData, TakeId InTakeId)
{
	TakeProgressFrameCount[InTakeId]++;

	if (TakeProgressTotalFrames[InTakeId] > 0)
	{
		TakeProgress[InTakeId].store(float(TakeProgressFrameCount[InTakeId]) / TakeProgressTotalFrames[InTakeId], std::memory_order_relaxed);
	}
	else
	{
		TakeProgress[InTakeId].store(0.0f, std::memory_order_relaxed);
	}
}

void FFileFootageIngest::ProcessComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData, TRetainedRef<TResult<void, FMetaHumanCaptureError>> OutResult)
{
	TResult<void, FMetaHumanCaptureError>& Result = OutResult.Get();

	if (InPipelineData->GetExitStatus() != UE::MetaHuman::Pipeline::EPipelineExitStatus::Ok &&
		InPipelineData->GetExitStatus() != UE::MetaHuman::Pipeline::EPipelineExitStatus::Aborted)
	{
		FText Message = FText::Format(LOCTEXT("IngestError_PipelineError", "An error occurred in the ingest pipeline: {0}"), FText::FromString(InPipelineData->GetErrorMessage()));
		Result = FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError, Message.ToString());

		return;
	}

	if (InPipelineData->GetExitStatus() == UE::MetaHuman::Pipeline::EPipelineExitStatus::Aborted)
	{
		Result = FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser);
		return;
	}

	Result = ResultOk;
}

void FFileFootageIngest::DeleteDataForTake(TakeId InId)
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	check(TakeInfoCache.Contains(InId));

	FMetaHumanTakeInfo& TakeInfo = TakeInfoCache[InId];

	const FString PathToDirectory = TargetIngestBaseDirectory / TakeInfo.OutputDirectory;
	const FString PathToAssets = TargetIngestBasePackagePath / TakeInfo.OutputDirectory;

	ExecuteFromGameThread(TEXT("TakeDataDeletion"), [PathToAssets, PathToDirectory]()
	{
		FIngestAssetCreator::RemoveAssetsByPath(PathToAssets);
		IFileManager::Get().DeleteDirectory(*PathToDirectory, true, true);
	});
}

TResult<void, FMetaHumanCaptureError> FFileFootageIngest::ReadTakeList(const FStopToken& InStopToken)
{
	TResult<void, FMetaHumanCaptureError> Result = ResultOk;
	TakeId NextTakeId = GenerateNewTakeId();
	bool bIterationResult = IFileManager::Get().IterateDirectoryRecursively(*InputDirectory, [&](const TCHAR* InFileNameOrDirectory, bool bInIsDirectory)
		{
			if (InStopToken.IsStopRequested())
			{
				Result = FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser);
				return false;
			}

			if (!bInIsDirectory)
			{
				FString FullFilePath(InFileNameOrDirectory);

				if (FPaths::GetPathLeaf(FullFilePath).Equals(TEXT("take.json")))
				{
					FMetaHumanTakeInfo TakeInfo = ReadTake(FullFilePath, InStopToken, NextTakeId);
					AddTakeInfo(MoveTemp(TakeInfo));
					PublishEvent<FNewTakesAddedEvent>(NextTakeId);
					NextTakeId = GenerateNewTakeId();
				}
			}

			return true;
		});

	if (Result.IsError())
	{
		return Result;
	}
	else if (bIterationResult)
	{
		return FMetaHumanCaptureError(EMetaHumanCaptureError::InvalidArguments, "Invalid path to the takes directory.");
	}

	return ResultOk;
}

#undef LOCTEXT_NAMESPACE