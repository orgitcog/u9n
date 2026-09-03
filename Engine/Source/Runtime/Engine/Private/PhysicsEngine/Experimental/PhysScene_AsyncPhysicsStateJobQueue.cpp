// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/Experimental/PhysScene_AsyncPhysicsStateJobQueue.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"

namespace Chaos
{
	static float GAsyncPhysicsStateTaskTimeBudgetMS = 0.0f;
	static FAutoConsoleVariableRef CVarAsyncPhysicsStateTaskTimeBudgetMS(
		TEXT("p.Chaos.AsyncPhysicsStateTask.TimeBudgetMS"),
		GAsyncPhysicsStateTaskTimeBudgetMS,
		TEXT("Maximum time budget in milliseconds for the async physics state task (0 = no time limit)"),
		ECVF_Default);
}

FPhysScene_AsyncPhysicsStateJobQueue::FPhysScene_AsyncPhysicsStateJobQueue(FPhysScene* InPhysicScene)
	: PhysScene(InPhysicScene)
{
	PhysScene->GetOwningWorld()->OnAllLevelsChanged().AddRaw(this, &FPhysScene_AsyncPhysicsStateJobQueue::OnUpdateLevelStreamingDone);
}

FPhysScene_AsyncPhysicsStateJobQueue::~FPhysScene_AsyncPhysicsStateJobQueue()
{
	PhysScene->GetOwningWorld()->OnAllLevelsChanged().RemoveAll(this);

	if (!IsCompleted())
	{
		Tick(true);
	}
	check(IsCompleted());
}

void FPhysScene_AsyncPhysicsStateJobQueue::AddJob(const FJob& Job)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPhysScene_AsyncPhysicsStateJobQueue::AddJob);
	check(IsInGameThread());

	Job.OnPreExecute_GameThread();
	FWriteScopeLock Lock(JobsLock);
	JobsToExecute.Add(Job);
}

void FPhysScene_AsyncPhysicsStateJobQueue::RemoveJob(const FJob& Job)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPhysScene_AsyncPhysicsStateJobQueue::RemoveJob);
	check(IsInGameThread());
	check(Job.IsValid());

	bool bWaitForJobToComplete = false;
	bool bFoundJob = false;
	{
		FWriteScopeLock Lock(JobsLock);
		// First test in CompletedJobs, if found, leave JobsToExecute 
		// as it will be cleaned up by the async task.
		if (CompletedJobs.Remove(Job))
		{
			bFoundJob = true;
		}
		// If the job is executing, wait for it to complete
		else if (ExecutingJob == Job)
		{
			bFoundJob = true;
			bWaitForJobToComplete = true;
		}
		// Else it's safe to remove it from JobsToExecute
		else if (JobsToExecute.Remove(Job))
		{
			bFoundJob = true;
		}
	}

	if (bFoundJob)
	{
		if (bWaitForJobToComplete)
		{
			// Wait for the async task
			AsyncJobTask.Wait();
			FWriteScopeLock Lock(JobsLock);
			// If job is completed, needs to be removed from CompletedJobs
			if (!CompletedJobs.Remove(Job))
			{
				// Else if it's still the executing job, finish executing it
				if (ExecutingJob == Job)
				{
					UE::FTimeout NeverTimeout(UE::FTimeout::Never());
					ExecutingJob->Execute(NeverTimeout);
					ExecutingJob.Reset();
					JobsToExecute.Remove(Job);
				}
			}
			check(!ExecutingJob.IsSet());
		}
		Job.OnPostExecute_GameThread();
	}
}

bool FPhysScene_AsyncPhysicsStateJobQueue::IsCompleted() const
{
	FReadScopeLock Lock(JobsLock);
	return AsyncJobTask.IsCompleted() && JobsToExecute.IsEmpty() && !ExecutingJob.IsSet() && CompletedJobs.IsEmpty();
}

void FPhysScene_AsyncPhysicsStateJobQueue::OnUpdateLevelStreamingDone()
{
	++GameThreadEpoch;

	LaunchAsyncJobTask();
}

void FPhysScene_AsyncPhysicsStateJobQueue::LaunchAsyncJobTask()
{
	if (!AsyncJobTask.IsCompleted())
	{
		return;
	}

	{
		FReadScopeLock Lock(JobsLock);
		if (JobsToExecute.IsEmpty())
		{
			return;
		}
	}

	const double TimeBudgetSeconds = Chaos::GAsyncPhysicsStateTaskTimeBudgetMS > 0 ? Chaos::GAsyncPhysicsStateTaskTimeBudgetMS / 1000 : MAX_dbl;
	AsyncJobTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, TimeBudgetSeconds]() { ExecuteJobsAsync(TimeBudgetSeconds); }, UE::Tasks::ETaskPriority::BackgroundHigh);
}

void FPhysScene_AsyncPhysicsStateJobQueue::ExecuteJobsAsync(double TimeBudgetSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPhysScene_AsyncPhysicsStateJobQueue::ExecuteJobsAsync);
	UE::FTimeout Timeout(TimeBudgetSeconds - UsedAsyncTaskTimeBudgetSec);
	bool bWorkRemaining = false;

	int32 CompletedJobsCount = 0;
	int32 Index = 0;
	do
	{
		if (bIsBlocking)
		{
			Timeout = UE::FTimeout::Never();
		}
		else if (TaskEpoch != GameThreadEpoch)
		{
			// Reset Timeout
			TaskEpoch = GameThreadEpoch;
			Timeout = UE::FTimeout(TimeBudgetSeconds);
			UsedAsyncTaskTimeBudgetSec = 0;
		}
		
		{
			FWriteScopeLock Lock(JobsLock);
			if (!JobsToExecute.IsValidIndex(Index))
			{
				break;
			}
			check(!ExecutingJob.IsSet() || (ExecutingJob == JobsToExecute[Index]));
			ExecutingJob = JobsToExecute[Index++];
		}

		if (ExecutingJob->Execute(Timeout))
		{
			FWriteScopeLock Lock(JobsLock);
			CompletedJobs.Add(*ExecutingJob);
			ExecutingJob.Reset();
			++CompletedJobsCount;
		}
		else
		{
			break;
		}

		if (Timeout.IsExpired())
		{
			break;
		}
	} while (true);

	// Remove completed jobs
	{
		FWriteScopeLock Lock(JobsLock);
		JobsToExecute.RemoveAt(0, CompletedJobsCount);
	}

	UsedAsyncTaskTimeBudgetSec += Timeout.GetElapsedSeconds();
}

void FPhysScene_AsyncPhysicsStateJobQueue::Tick(bool bWaitForCompletion)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPhysScene_AsyncPhysicsStateJobQueue::Tick);
	check(IsInGameThread());
	if (IsCompleted())
	{
		return;
	}

	const bool bShouldWaitForCompletion = bWaitForCompletion ||
		PhysScene->GetOwningWorld()->GetIsInBlockTillLevelStreamingCompleted() ||
		PhysScene->GetOwningWorld()->GetShouldForceUnloadStreamingLevels() ||
		PhysScene->GetOwningWorld()->IsBeingCleanedUp();

	// Wait for tasks if the world is inside a blocking load
	if (bShouldWaitForCompletion)
	{
		// Tell the async task that there is no time limit anymore
		bIsBlocking = true;
		do
		{
			LaunchAsyncJobTask();
			AsyncJobTask.Wait();
			{
				FReadScopeLock Lock(JobsLock);
				if (JobsToExecute.IsEmpty())
				{
					break;
				}
			}
		} while (true);
		bIsBlocking = false;
	}

	TArray<FJob> CompletedJobsCopy;
	{
		FWriteScopeLock Lock(JobsLock);
		CompletedJobsCopy = MoveTemp(CompletedJobs);
	}

	if (!CompletedJobsCopy.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPhysScene_AsyncPhysicsStateJobQueue::Tick_OnPostExecute_GameThread);
		for (const FJob& Job : CompletedJobsCopy)
		{
			Job.OnPostExecute_GameThread();
		}
	}
}

void FPhysScene_AsyncPhysicsStateJobQueue::FJob::OnPreExecute_GameThread() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPhysScene_AsyncPhysicsStateJobQueue::OnPreExecute_GameThread);
	switch (Type)
	{
		case EJobType::DestroyPhysicsState:
		{
			Processor->OnAsyncDestroyPhysicsStateBegin_GameThread();
		}
		break;
	}
}

bool FPhysScene_AsyncPhysicsStateJobQueue::FJob::Execute(UE::FTimeout& Timeout) const
{
	if (IsValid())
	{
		switch (Type)
		{
		case EJobType::CreatePhysicsState:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPhysScene_AsyncPhysicsStateJobQueue::CreatePhysicsState);
			return Processor->OnAsyncCreatePhysicsState(Timeout);
		}
		break;
		case EJobType::DestroyPhysicsState:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPhysScene_AsyncPhysicsStateJobQueue::DestroyPhysicsState);
			return Processor->OnAsyncDestroyPhysicsState(Timeout);
		}
		break;
		}
	}
	return true;
}

void FPhysScene_AsyncPhysicsStateJobQueue::FJob::OnPostExecute_GameThread() const
{
	if (IsValid()) 
	{
		switch (Type)
		{
			case EJobType::CreatePhysicsState:
			{
				Processor->OnAsyncCreatePhysicsStateEnd_GameThread();
			}
			break;
			case EJobType::DestroyPhysicsState:
			{
				Processor->OnAsyncDestroyPhysicsStateEnd_GameThread();
			}
			break;
		}
	}
}