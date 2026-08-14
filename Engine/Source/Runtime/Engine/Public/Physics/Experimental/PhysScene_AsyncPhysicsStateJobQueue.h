// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/Pipe.h"
#include "Tasks/Task.h"
#include "Async/Fundamental/Task.h"
#include "UObject/Object.h"
#include "PhysicsInterfaceDeclaresCore.h"
#include "Physics/Experimental/AsyncPhysicsStateProcessorInterface.h"

struct FPhysScene_AsyncPhysicsStateJobQueue
{
	FPhysScene_AsyncPhysicsStateJobQueue(FPhysScene* InPhysicScene);
	~FPhysScene_AsyncPhysicsStateJobQueue();

	enum class EJobType
	{
		CreatePhysicsState,
		DestroyPhysicsState
	};

	struct FJob
	{
		FJob(IAsyncPhysicsStateProcessor* InProcessor, EJobType InType)
			: Processor(InProcessor)
			, Type(InType)
		{}

		bool IsValid() const
		{
			return Processor && ::IsValid(Processor->GetAsyncPhysicsStateObject());
		}

		bool Execute(UE::FTimeout& Timeout) const;
		void OnPreExecute_GameThread() const;
		void OnPostExecute_GameThread() const;

		inline bool operator == (const FJob& Other) const
		{
			return (Processor == Other.Processor) && (Type == Other.Type);
		}

		friend inline uint32 GetTypeHash(const FJob& InJob)
		{
			return GetTypeHash(TPair<IAsyncPhysicsStateProcessor*, EJobType>{InJob.Processor, InJob.Type});
		}

		IAsyncPhysicsStateProcessor* Processor;
		EJobType Type;
	};

	void Tick(bool bWaitForCompletion = false);
	void AddJob(const FJob& Job);
	void RemoveJob(const FJob& Job);
	bool IsCompleted() const;

private:

	void LaunchAsyncJobTask();
	void ExecuteJobsAsync(double TimeBudgetSeconds);
	void OnUpdateLevelStreamingDone();

	// GameThread variables
	FPhysScene* PhysScene = nullptr; // The Physics scene
	UE::Tasks::FTask AsyncJobTask; // The async task

	// Async Task variables
	int32 TaskEpoch = 0; // Last epoch used by the async task
	double UsedAsyncTaskTimeBudgetSec = 0; // Time used in the async task since the last epoch update

	// Variables access/modified by both the GameThread and the async task
	mutable FRWLock JobsLock; // Lock used to protect jobs to execute, executing and completed
	TArray<FJob> JobsToExecute;
	TOptional<FJob> ExecutingJob;
	TArray<FJob> CompletedJobs;

	std::atomic<bool> bIsBlocking = false; // Notifies to the async task that we are block waiting for the task to complete
	std::atomic<int32> GameThreadEpoch = 0; // Epoch updated by the GameThread at every frame and used by the async task to reset UsedAsyncTaskTimeBudgetSec
};