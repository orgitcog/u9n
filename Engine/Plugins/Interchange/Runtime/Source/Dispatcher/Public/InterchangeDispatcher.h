// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/Thread.h"
#include "InterchangeDispatcherTask.h"
#include "InterchangeWorkerHandler.h"

#define UE_API INTERCHANGEDISPATCHER_API

namespace UE
{
	namespace Interchange
	{

		// Handle a list of tasks, and a set of external workers to consume them.
		class  FInterchangeDispatcher
		{
		public:
			UE_API FInterchangeDispatcher(const FString& InResultFolder);
			~FInterchangeDispatcher() { TerminateProcess(); }

			UE_API int32 AddTask(const FString& JsonDescription);
			UE_API int32 AddTask(const FString& JsonDescription, FInterchangeDispatcherTaskCompleted TaskCompledDelegate);
			UE_API TOptional<FTask> GetNextTask();
			UE_API void SetTaskState(int32 TaskIndex, ETaskState TaskState, const FString& JsonResult, const TArray<FString>& JSonMessages);
			UE_API void GetTaskState(int32 TaskIndex, ETaskState& TaskState, double& TaskRunningStateStartTime);
			UE_API void GetTaskState(int32 TaskIndex, ETaskState& TaskState, FString& JsonResult, TArray<FString>& JSonMessages);

			UE_API void StartProcess();
			UE_API void StopProcess(bool bBlockUntilTerminated);
			UE_API void TerminateProcess();
			UE_API void WaitAllTaskToCompleteExecution();
			UE_API bool IsOver();

			void SetInterchangeWorkerFatalError(FString& ErrorMessage)
			{
				InterchangeWorkerFatalError = MoveTemp(ErrorMessage);
			}

			FString GetInterchangeWorkerFatalError()
			{
				return InterchangeWorkerFatalError;
			}

			UE_API bool IsInterchangeWorkerRunning();
			static UE_API bool IsInterchangeWorkerAvailable();
		private:
			void SpawnHandler();
			bool IsHandlerAlive();
			void CloseHandler();
			void EmptyQueueTasks();

			// Tasks
			FCriticalSection TaskPoolCriticalSection;
			TArray<FTask> TaskPool;
			int32 NextTaskIndex;
			int32 CompletedTaskCount;

			/** Path where the result files are dump */
			FString ResultFolder;

			FString InterchangeWorkerFatalError;

			// Workers
			TUniquePtr<FInterchangeWorkerHandler> WorkerHandler = nullptr;
		};

	} //ns Interchange
}//ns UE

#undef UE_API
