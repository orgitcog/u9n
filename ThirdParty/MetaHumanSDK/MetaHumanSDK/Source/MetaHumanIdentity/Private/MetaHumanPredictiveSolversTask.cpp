// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPredictiveSolversTask.h"
#include "Async/TaskGraphInterfaces.h"
#include "Async/Async.h"

#include "Features/IModularFeatures.h"
#include "MetaHumanFaceTrackerInterface.h"


/////////////////////////////////////////////////////
// FPredictiveSolversWorker

FPredictiveSolversWorker::FPredictiveSolversWorker(bool bInIsAsync, const FPredictiveSolversTaskConfig& InConfig, SolverProgressFunc InOnProgress, SolverCompletedFunc InOnCompleted, std::atomic<bool>& bInIsCancelled, std::atomic<float>& InProgress)
	: bIsAsync(bInIsAsync)
	, Config(InConfig)
	, OnProgress(InOnProgress)
	, OnCompleted(InOnCompleted)
	, bIsCancelled(bInIsCancelled)
	, Progress(InProgress)
	, LastProgress(0.0f)
	, bIsDone(false)
	, Result({})
{
}

void FPredictiveSolversWorker::DoWork()
{
	RunTraining();

	bIsDone = true;

	if (bIsAsync)
	{
		OnCompleted();
	}
}

void FPredictiveSolversWorker::RunTraining()
{
	Result.bSuccess = false;

	const FName& FeatureName = IPredictiveSolverInterface::GetModularFeatureName();
	if (IModularFeatures::Get().IsModularFeatureAvailable(FeatureName))
	{
		IPredictiveSolverInterface& PredSolverAPI = IModularFeatures::Get().GetModularFeature<IPredictiveSolverInterface>(FeatureName);
		PredSolverAPI.TrainPredictiveSolver(bIsDone, Progress, OnProgress, bIsCancelled, Config, Result);
	}
}

/////////////////////////////////////////////////////
// FPredictiveSolversTask

FPredictiveSolversTask::FPredictiveSolversTask(const FPredictiveSolversTaskConfig& InConfig)
	: Config(InConfig)
{
}

FPredictiveSolversResult FPredictiveSolversTask::StartSync()
{
	FPredictiveSolversWorker::SolverProgressFunc InOnProgress = [this](float InProgress)
	{
		OnProgress_Thread(InProgress);
	};

	Task = MakeUnique<FAsyncTask<FPredictiveSolversWorker>>(false, Config, InOnProgress, nullptr, bCancelled, Progress);
	Task->StartSynchronousTask();

	return MoveTemp(Task->GetTask().Result);
}

void FPredictiveSolversTask::StartAsync()
{
	check(Task == nullptr);
	check(IsInGameThread());
	check(FPlatformProcess::SupportsMultithreading());

	FPredictiveSolversWorker::SolverProgressFunc InOnProgress = [this](float InProgress)
	{
		OnProgress_Thread(InProgress);
	};
	FPredictiveSolversWorker::SolverCompletedFunc InOnCompleted = [this]()
	{
		OnCompleted_Thread();
	};

	Task = MakeUnique<FAsyncTask<FPredictiveSolversWorker>>(true, Config, InOnProgress, InOnCompleted, bCancelled, Progress);
	Task->StartBackgroundTask();
}

FOnPredictiveSolversCompleted& FPredictiveSolversTask::OnCompletedCallback()
{
	return OnCompletedDelegate;
}

FOnPredictiveSolversProgress& FPredictiveSolversTask::OnProgressCallback()
{
	return OnProgressDelegate;
}

bool FPredictiveSolversTask::IsDone() const
{
	return Task.IsValid() && Task->IsDone();
}

bool FPredictiveSolversTask::WasCancelled() const
{
	return bCancelled;
}

void FPredictiveSolversTask::Cancel()
{
	if (!IsDone() && !bCancelled)
	{
		bCancelled = true;
	}
}

void FPredictiveSolversTask::Stop()
{
	// TODO: This can't stop task execution fast enough because
	// the tracker is using resources and threads extensively, so
	// it takes a while to clean everything up
	if (!IsDone())
	{
		bCancelled = true;
		bSkipCallback = true;
		Task->EnsureCompletion();
	}
}

bool FPredictiveSolversTask::PollProgress(float& OutProgress) const
{
	if (!IsDone())
	{
		OutProgress = Progress;
		return true;
	}

	return false;
}

void FPredictiveSolversTask::OnProgress_Thread(float InProgress)
{
	if (OnProgressDelegate.IsBound())
	{
		AsyncTask(ENamedThreads::GameThread, [this, InProgress]
		{
			OnProgressDelegate.ExecuteIfBound(InProgress);
		});
	}
}

void FPredictiveSolversTask::OnCompleted_Thread()
{
	if (bSkipCallback)
	{
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [this]
	{
		if (Task.IsValid())
		{
			Task->EnsureCompletion();

			OnCompletedDelegate.ExecuteIfBound(MoveTemp(Task->GetTask().Result));
		}
	});
}

/////////////////////////////////////////////////////
// FPredictiveSolversTaskManager

FPredictiveSolversTaskManager& FPredictiveSolversTaskManager::Get()
{
	static FPredictiveSolversTaskManager Instance;

	return Instance;
}

FPredictiveSolversTask* FPredictiveSolversTaskManager::New(const FPredictiveSolversTaskConfig& InConfig)
{
	Tasks.Emplace(MakeUnique<FPredictiveSolversTask>(InConfig));

	return Tasks.Last().Get();
}

void FPredictiveSolversTaskManager::StopAll()
{
	if (Tasks.IsEmpty())
	{
		return;
	}

	for (int32 i = 0; i < Tasks.Num(); i++)
	{
		if (Tasks[i].IsValid() && !Tasks[i]->IsDone())
		{
			Tasks[i]->Stop();
		}
	}

	Tasks.Empty();
}

bool FPredictiveSolversTaskManager::Remove(FPredictiveSolversTask*& InOutTask)
{
	if (InOutTask && InOutTask->IsDone())
	{
		for (int32 i = 0; i < Tasks.Num(); i++)
		{
			if (Tasks[i].IsValid() && Tasks[i].Get() == InOutTask)
			{
				Tasks.RemoveAt(i);
				InOutTask = nullptr;

				return true;
			}
		}
	}

	return false;
}
