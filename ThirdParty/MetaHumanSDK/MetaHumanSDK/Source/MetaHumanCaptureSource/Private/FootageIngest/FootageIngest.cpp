// Copyright Epic Games, Inc. All Rights Reserved.

#include "FootageIngest.h"

#include "MetaHumanCaptureSourceLog.h"
#include "CameraCalibration.h"

#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"
#include "Serialization/JsonSerializer.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ImgMediaSource.h"
#include "Sound/SoundWave.h"
#include "AssetImportTask.h"
#include "ObjectTools.h"
#include "LensFile.h"
#include "ImageSequenceUtils.h"

#include "HAL/IConsoleManager.h"

#include "Templates/IsIntegral.h"

#define LOCTEXT_NAMESPACE "FootageIngest"

FFootageIngest::FFootageIngest()
{
	RegisterEvent(FTakeListResetEvent::Name);
	RegisterEvent(FNewTakesAddedEvent::Name);
	RegisterEvent(FThumbnailChangedEvent::Name);
	RegisterEvent(FConnectionChangedEvent::Name);
	RegisterEvent(FRecordingStatusChangedEvent::Name);
	RegisterEvent(FTakesRemovedEvent::Name);
}

FFootageIngest::~FFootageIngest() = default;

void FFootageIngest::Startup(ETakeIngestMode InMode)
{
	Mode = InMode;
}

void FFootageIngest::SetTargetPath(const FString& InTargetIngestDirectory, const FString& InTargetPackagePath)
{
	TargetIngestBaseDirectory = InTargetIngestDirectory;
	TargetIngestBasePackagePath = InTargetPackagePath;

	FPaths::NormalizeDirectoryName(TargetIngestBaseDirectory);
}

bool FFootageIngest::IsProcessing() const
{
	return ProcessTakesAsyncTask.IsValid() && !ProcessTakesAsyncTask->IsDone();
}

void FFootageIngest::CancelProcessing(const TArray<TakeId>&)
{
	//this is the base function, inherited by LiveLinkFaceFootageIngest;
	//it cancels all the takes for the capture source and is called only
	//if CancelProcessing in subclasses receive an empty list (which reads as "Cancel All")
	if (IsProcessing())
	{
		ProcessTakesAsyncTask->Abort();
		bCancelAllRequested = true;
	}
}

bool FFootageIngest::IsCancelling() const
{
	return bCancelAllRequested;
}

float FFootageIngest::GetTaskProgress(TakeId InId) const
{
	if (IsProcessing())
	{
		return TakeProgress[InId].load(std::memory_order_relaxed);
	}

	return 0.0f;
}

FText FFootageIngest::GetTaskName(TakeId InId) const
{
	if (IsProcessing())
	{
		FScopeLock Lock(&TakeProcessNameMutex);
		return TakeProcessName[InId];
	}

	return FText();
}

bool FFootageIngest::Tick(float InDeltaTime)
{
	ProcessTakesFinished();
	return true;
}

IFootageIngestAPI::FOnGetTakesFinished& FFootageIngest::OnGetTakesFinished()
{
	return OnGetTakesFinishedDelegate;
}

TOptional<FText> FFootageIngest::TakeDurationExceedsLimit(const float InDurationInSeconds)
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.SoundWaveImportLengthLimitInSeconds"));
	if (CVar)
	{
		static constexpr float Unlimited = -1.f;
		float Limit = CVar->GetFloat();

		if (!FMath::IsNearlyEqual(Limit, Unlimited) && FMath::IsNegativeOrNegativeZero(Limit - InDurationInSeconds))
		{
			const FText Message = LOCTEXT("IngestError_TakeDurationExceedsLimit", "Take duration ({0} seconds) exceeds allowed limit ({1} seconds).");

			FNumberFormattingOptions Options;
			Options.MaximumFractionalDigits = 2;
			Options.MinimumFractionalDigits = 2;

			return FText::Format(Message, FText::AsNumber(InDurationInSeconds, &Options), FText::AsNumber(Limit, &Options));
		}
	}

	return {};
}

void FFootageIngest::ProcessTakes(FAbortableAsyncTask::FTaskFunction InProcessTakesFunction)
{
	ProcessTakesAsyncTask = MakeUnique<FAbortableAsyncTask>(InProcessTakesFunction);

	switch (Mode)
	{
	case ETakeIngestMode::Async:
		ProcessTakesAsyncTask->StartAsync();
		break;

	case ETakeIngestMode::Blocking:
		ProcessTakesAsyncTask->StartSync();
		ProcessTakesFinished();
		break;

	default:
		break;
	}
}

void FFootageIngest::ProcessTakesFinished()
{
	if (ProcessTakesAsyncTask.IsValid() && ProcessTakesAsyncTask->IsDone())
	{
		ProcessTakesAsyncTask.Reset();

		TArray<FMetaHumanTake> IngestedTakes;

		if (!bCancelAllRequested)
		{
			FScopeLock Lock(&CurrentIngestedTakeMutex);
			IngestedTakes = MoveTemp(CurrentIngestedTakes);
		}

		OnGetTakesFinishedDelegate.ExecuteIfBound(IngestedTakes);
		ClearTakesFromIngestCache();

		// Reset the cancel requested flag as the task is now done or cancelled
		if (bCancelAllRequested)
		{
			bCancelAllRequested = false;
		}
	}
}

void FFootageIngest::RemoveTakeFromIngestCache(const TakeId InId)
{
	FScopeLock Lock(&CurrentIngestedTakeMutex);

	const int32 TakeToRemove = CurrentIngestedTakes.IndexOfByPredicate([InId](const FMetaHumanTake& InTake)
	{
		return InId == InTake.TakeId;
	});

	if (TakeToRemove != INDEX_NONE)
	{
		CurrentIngestedTakes.RemoveAt(TakeToRemove);
	}
}

void FFootageIngest::ClearTakesFromIngestCache()
{
	FScopeLock Lock(&CurrentIngestedTakeMutex);
	CurrentIngestedTakes.Empty();
}

#undef LOCTEXT_NAMESPACE