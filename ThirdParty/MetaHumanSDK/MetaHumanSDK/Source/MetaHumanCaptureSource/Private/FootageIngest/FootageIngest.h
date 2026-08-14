// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFootageIngestAPI.h"
#include "CameraCalibration.h"
#include "Pipeline/Pipeline.h"
#include "Pipeline/PipelineData.h"
#include "Async/EventSourceUtils.h"
#include "Async/Task.h"
#include "AssetImportTask.h"

#include "Containers/Ticker.h"
#include "AssetRegistry/AssetData.h"

class FFootageIngest
	: public IFootageIngestAPI
	, public FTSTickerObjectBase
{
public:
	FFootageIngest();
	virtual ~FFootageIngest();

	//~ IFootageIngestAPI interface
	virtual void Startup(ETakeIngestMode InMode) override;
	virtual bool IsProcessing() const override;
	virtual void CancelProcessing(const TArray<TakeId>& InIdList) override;
	virtual bool IsCancelling() const override;
	virtual float GetTaskProgress(TakeId InId) const;
	virtual FText GetTaskName(TakeId InId) const;
	virtual void SetTargetPath(const FString& InTargetIngestDirectory, const FString& InTargetPackagePath) override;

	//~ FTSTickerObjectBase interface
	virtual bool Tick(float InDeltaTime) override;

	virtual FOnGetTakesFinished& OnGetTakesFinished() override;

protected:

	template<typename Functor>
	void ExecuteFromGameThread(const FString& InName, Functor&& InFunctor)
	{
		if (IsInGameThread())
		{
			InFunctor();
		}
		else
		{
			ExecuteOnGameThread(*InName, MoveTemp(InFunctor));
		}
	}
	/**
	 * @brief Runs a function to process takes.
	 *		This can be called by derived classes to run a function in either blocking or async modes.
	 * @param InTaskType The type of task to be executed. Used to call the appropriate delegates when the task finishes executing
	 * @param InProcessTakesFunction A callable to process takes. It receives an abort flag as a parameter that should be checked
	 *								 by implementations that indicates the user requested the task to be cancelled.
	 */
	void ProcessTakes(FAbortableAsyncTask::FTaskFunction InProcessTakesFunction);
	static TOptional<FText> TakeDurationExceedsLimit(float InDurationInSeconds);
private:

	/**
	 * @brief Called after a process takes task finishes, being it blocking or async.
	 *		Calls delegates to notify observers that the task has finished
	 */
	void ProcessTakesFinished();

protected:
	FString TargetIngestBaseDirectory;
	FString TargetIngestBasePackagePath;
	ETakeIngestMode Mode;

	TUniquePtr<FAbortableAsyncTask> ProcessTakesAsyncTask;

	FOnGetTakesFinished OnGetTakesFinishedDelegate;

	TMap<TakeId, std::atomic<float>> TakeProgress;
	TMap<TakeId, std::atomic<int32>> TakeProgressFrameCount;
	TMap<TakeId, std::atomic<int32>> TakeProgressTotalFrames; // not defaulted to zero to prevent divide by zero - numbers should be set to real values before use

	mutable FCriticalSection TakeProcessNameMutex;
	TMap<TakeId, FText> TakeProcessName;

	bool bCancelAllRequested = false;

	void RemoveTakeFromIngestCache(const TakeId InId);
	void ClearTakesFromIngestCache();


	// The Take cache for the current execution of GetTakesAsync
	mutable FCriticalSection CurrentIngestedTakeMutex;
	TArray<FMetaHumanTake> CurrentIngestedTakes;
};

