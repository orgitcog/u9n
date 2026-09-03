// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"
#include "UObject/WeakObjectPtr.h"

/**
 * Predictive solvers configuration.
 */
struct FPredictiveSolversTaskConfig
{
	FString TemplateDescriptionJson;
	FString ConfigurationJson;
	TWeakObjectPtr<class UDNAAsset> DNAAsset;
	TArray<uint8> PredictiveSolverGlobalTeethTrainingData;
	TArray<uint8> PredictiveSolverTrainingData;
	bool bTrainPreviewSolvers = true;
};

/**
 * Predictive solvers training result.
 */
struct FPredictiveSolversResult
{
	TArray<uint8> PredictiveWithoutTeethSolver;
	TArray<uint8> PredictiveSolvers;
	bool bSuccess = false;
};

/**
 * Predictive solvers worker that actually does calculations.
 */
class FPredictiveSolversWorker : public FNonAbandonableTask
{
public:
	using SolverProgressFunc = TFunction<void(float)>;
	using SolverCompletedFunc = TFunction<void(void)>;

	FPredictiveSolversWorker(bool bInIsAsync, const FPredictiveSolversTaskConfig& InConfig, SolverProgressFunc InOnProgress, SolverCompletedFunc InOnCompleted, std::atomic<bool>& bInIsCancelled, std::atomic<float>& InProgress);

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPredictiveSolversWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork();

private:
	friend class FPredictiveSolversTask;
	void RunTraining();

	bool bIsAsync = true;
	FPredictiveSolversTaskConfig Config;
	SolverProgressFunc OnProgress;
	SolverCompletedFunc OnCompleted;
	std::atomic<bool>& bIsCancelled;
	std::atomic<float>& Progress;
	float LastProgress = 0.0f;
	std::atomic<bool> bIsDone = false;
	FPredictiveSolversResult Result;
};

DECLARE_DELEGATE_OneParam(FOnPredictiveSolversCompleted, const FPredictiveSolversResult&);
DECLARE_DELEGATE_OneParam(FOnPredictiveSolversProgress, float);

/**
 * Predictive solver task that creates new worker for predictive solver calculations.
 */
class METAHUMANIDENTITY_API FPredictiveSolversTask
{
public:
	FPredictiveSolversTask(const FPredictiveSolversTaskConfig& InConfig);

	FPredictiveSolversResult StartSync();
	void StartAsync();

	/** Only triggered when task is executed asynchronously */
	FOnPredictiveSolversCompleted& OnCompletedCallback();
	FOnPredictiveSolversProgress& OnProgressCallback();

	bool IsDone() const;
	bool WasCancelled() const;

	void Cancel();
	void Stop();

	/** Obtains current training progress in range [0..1]. Return true if it was successful (task is still active), false otherwise */
	bool PollProgress(float& OutProgress) const;

private:
	FPredictiveSolversTaskConfig Config;
	TUniquePtr<FAsyncTask<class FPredictiveSolversWorker>> Task;
	std::atomic<bool> bCancelled = false;
	std::atomic<float> Progress = 0.0f;
	std::atomic<bool> bSkipCallback = false;
	FOnPredictiveSolversCompleted OnCompletedDelegate;
	FOnPredictiveSolversProgress OnProgressDelegate;

	void OnProgress_Thread(float InProgress);
	void OnCompleted_Thread();
};

/**
 * Singleton responsible for managing and owning predictive solver tasks.
 */
class METAHUMANIDENTITY_API FPredictiveSolversTaskManager
{
public:
	static FPredictiveSolversTaskManager& Get();

	FPredictiveSolversTaskManager() = default;
	~FPredictiveSolversTaskManager() = default;

	FPredictiveSolversTaskManager(FPredictiveSolversTaskManager const&) = delete;
	FPredictiveSolversTaskManager& operator=(const FPredictiveSolversTaskManager&) = delete;

	/** Creates new solver tasks and adds to the list of active tasks */
	FPredictiveSolversTask* New(const FPredictiveSolversTaskConfig& InConfig);

	/** Removes given task from the list and nullifies the reference */
	bool Remove(FPredictiveSolversTask*& InOutTask);

	/** Stops all active tasks */
	void StopAll();

private:
	TArray<TUniquePtr<FPredictiveSolversTask>> Tasks;
};
