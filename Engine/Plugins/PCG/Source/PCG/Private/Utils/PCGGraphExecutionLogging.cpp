// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphExecutionLogging.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "Graph/PCGGraphExecutor.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

namespace PCGGraphExecutionLogging
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	static TAutoConsoleVariable<bool> CVarGraphExecutionLoggingEnable(
		TEXT("pcg.GraphExecution.EnableLogging"),
		false,
		TEXT("Enables fine grained log of graph execution"));

	static TAutoConsoleVariable<bool> CVarGraphExecutionCullingLoggingEnable(
		TEXT("pcg.GraphExecution.EnableCullingLogging"),
		false,
		TEXT("Enables fine grained log of dynamic task culling during graph execution"));
#endif

	bool LogEnabled()
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		return CVarGraphExecutionLoggingEnable.GetValueOnAnyThread();
#else
		return false;
#endif
	}

	bool CullingLogEnabled()
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		return CVarGraphExecutionCullingLoggingEnable.GetValueOnAnyThread();
#else
		return false;
#endif
	}

	void LogGraphTask(FPCGTaskId TaskId, const FPCGGraphTask& Task, const TSet<FPCGTaskId>* SuccessorIds)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		auto GenerateInputsString = [](const TArray<FPCGGraphTaskInput>& Inputs)
		{
			FString InputString;
			bool bFirstInput = true;

			for (const FPCGGraphTaskInput& Input : Inputs)
			{
				if (!bFirstInput)
				{
					InputString += TEXT(",");
				}
				bFirstInput = false;

				InputString += FString::Printf(
					TEXT("%" UINT64_FMT "->'%s'"),
					Input.TaskId,
					Input.DownstreamPin.IsSet() ? *Input.DownstreamPin.GetValue().Label.ToString() : TEXT("NoPin"));
			}

			return InputString;
		};

		FString SuccessorsString;
		if (SuccessorIds)
		{
			bool bFirstSuccessor = true;
			for (const FPCGTaskId& SuccessorId : *SuccessorIds)
			{
				SuccessorsString += bFirstSuccessor ? FString::Printf(TEXT("%" UINT64_FMT), SuccessorId) : FString::Printf(TEXT(",%" UINT64_FMT), SuccessorId);
				bFirstSuccessor = false;
			}
		}

		const FString PinDependencyString =
#if WITH_EDITOR
			Task.PinDependency.ToString();
#else
			TEXT("MISSINGPINDEPS");
#endif

		UE_LOG(LogPCG, Log, TEXT("\t\tID: %u\tParent: %u\tNode: %s\tInputs: %s\tPinDeps: %s\tSuccessors: %s"),
			TaskId,
			Task.ParentId != InvalidPCGTaskId ? Task.ParentId : 0,
			Task.Node ? (*Task.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString()) : TEXT("NULL"),
			*GenerateInputsString(Task.Inputs),
			*PinDependencyString,
			*SuccessorsString
		);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphTasks(const TMap<FPCGTaskId, FPCGGraphTask>& Tasks, const TMap<FPCGTaskId, TSet<FPCGTaskId>>* TaskSuccessors)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		for (const TPair<FPCGTaskId, FPCGGraphTask>& TaskIdAndTask : Tasks)
		{
			PCGGraphExecutionLogging::LogGraphTask(TaskIdAndTask.Key, TaskIdAndTask.Value, TaskSuccessors ? TaskSuccessors->Find(TaskIdAndTask.Value.NodeId) : nullptr);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphTasks(const TArray<FPCGGraphTask>& Tasks)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		for (const FPCGGraphTask& Task : Tasks)
		{
			LogGraphTask(Task.NodeId, Task);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphSchedule(const IPCGGraphExecutionSource* InExecutionSource, const UPCGGraph* InScheduledGraph)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() && !IsRunningCommandlet())
		{
			return;
		}

		UE_LOG(LogPCG, Display, TEXT("[%s/%s] --- SCHEDULE GRAPH %s ---"),
			*PCGLog::GetExecutionSourceName(InExecutionSource),
			(InExecutionSource && InExecutionSource->GetExecutionState().GetGraph()) ? *InExecutionSource->GetExecutionState().GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			InScheduledGraph ? *InScheduledGraph->GetName() : TEXT("MISSINGGRAPH)"));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphScheduleDependency(const IPCGGraphExecutionSource* InExecutionSource, const FPCGStack* InFromStack)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() && !IsRunningCommandlet())
		{
			return;
		}

		FString FromStackString;
		if (InFromStack)
		{
			InFromStack->CreateStackFramePath(FromStackString);
		}

		UE_LOG(LogPCG, Display, TEXT("[%s/%s] --- SCHEDULE GRAPH FOR DEPENDENCY, from stack: %s"),
			*PCGLog::GetExecutionSourceName(InExecutionSource),
			(InExecutionSource && InExecutionSource->GetExecutionState().GetGraph()) ? *InExecutionSource->GetExecutionState().GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			*FromStackString);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphScheduleDependencyFailed(const IPCGGraphExecutionSource* InExecutionSource, const FPCGStack* InFromStack)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() && !IsRunningCommandlet())
		{
			return;
		}

		FString FromStackString;
		if (InFromStack)
		{
			InFromStack->CreateStackFramePath(FromStackString);
		}

		UE_LOG(LogPCG, Warning, TEXT("[%s/%s] Failed to schedule dependency, from stack: %s"),
			*PCGLog::GetExecutionSourceName(InExecutionSource),
			(InExecutionSource && InExecutionSource->GetExecutionState().GetGraph()) ? *InExecutionSource->GetExecutionState().GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			*FromStackString);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}
	
	void LogGraphPostSchedule(const TMap<FPCGTaskId, FPCGGraphTask>& Tasks, const TMap<FPCGTaskId, TSet<FPCGTaskId>>& TaskSuccessors)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!CullingLogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("POST SCHEDULE:"));

		LogGraphTasks(Tasks, &TaskSuccessors);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogPostProcessGraph(const IPCGGraphExecutionSource* InExecutionSource)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() && !IsRunningCommandlet())
		{
			return;
		}

		UE_LOG(LogPCG, Display, TEXT("[%s/%s] IPCGGraphExecutionSource::PostProcessGraph"),
			*PCGLog::GetExecutionSourceName(InExecutionSource),
			(InExecutionSource && InExecutionSource->GetExecutionState().GetGraph()) ? *InExecutionSource->GetExecutionState().GetGraph()->GetName() : TEXT("MISSINGGRAPH"));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogExecutionSourceCancellation(const TSet<IPCGGraphExecutionSource*>& CancelledExecutionSources)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() && !IsRunningCommandlet())
		{
			return;
		}

		for (const IPCGGraphExecutionSource* ExecutionSource : CancelledExecutionSources)
		{
			UE_LOG(LogPCG, Display, TEXT("[%s/%s] ExecutionSource cancelled"),
				*PCGLog::GetExecutionSourceName(ExecutionSource),
				(ExecutionSource && ExecutionSource->GetExecutionState().GetGraph()) ? *ExecutionSource->GetExecutionState().GetGraph()->GetName() : TEXT("MISSINGGRAPH"));
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogChangeOriginIgnoredForComponent(const UObject* InObject, const IPCGGraphExecutionSource* InExecutionSource)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[%s/%s] Change origin ignored: '%s'"),
			*PCGLog::GetExecutionSourceName(InExecutionSource),
			(InExecutionSource && InExecutionSource->GetExecutionState().GetGraph()) ? *InExecutionSource->GetExecutionState().GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			InObject ? *InObject->GetName() : TEXT("MISSINGOBJECT"));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphExecuteFrameFinished()
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("--- FINISH FPCGGRAPHEXECUTOR::EXECUTE ---"));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	FString GetPinsToDeactivateString(const TArray<FPCGPinId>& PinIdsToDeactivate)
	{
		FString PinIdsToDeactivateString;
		bool bFirst = true;

		for (const FPCGPinId& PinId : PinIdsToDeactivate)
		{
			const FPCGTaskId NodeId = PCGPinIdHelpers::GetNodeIdFromPinId(PinId);
			const uint64 PinIndex = PCGPinIdHelpers::GetPinIndexFromPinId(PinId);
			PinIdsToDeactivateString += bFirst ? FString::Printf(TEXT("%" UINT64_FMT "_%" UINT64_FMT), NodeId, PinIndex) : FString::Printf(TEXT(",%" UINT64_FMT "_%" UINT64_FMT), NodeId, PinIndex);
			bFirst = false;
		}

		return PinIdsToDeactivateString;
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING

	void LogTaskExecute(const FPCGGraphTask& Task)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		IPCGGraphExecutionSource* ExecutionSource = Task.ExecutionSource.Get();
		if (!ExecutionSource)
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("         [%s/%s] %s\t\tEXECUTE"),
			*PCGLog::GetExecutionSourceName(ExecutionSource),
			ExecutionSource->GetExecutionState().GetGraph() ? *ExecutionSource->GetExecutionState().GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			*FString::Printf(TEXT("%" UINT64_FMT "'%s'"), Task.NodeId, Task.Node ? *Task.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString() : TEXT("")));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogTaskExecuteCachingDisabled(const FPCGGraphTask& Task)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		IPCGGraphExecutionSource* ExecutionSource = Task.ExecutionSource.Get();
		if (!ExecutionSource)
		{
			return;
		}

		UE_LOG(LogPCG, Warning, TEXT("[%s/%s] %s\t\tCACHING DISABLED"),
			*PCGLog::GetExecutionSourceName(ExecutionSource),
			ExecutionSource->GetExecutionState().GetGraph() ? *ExecutionSource->GetExecutionState().GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			*FString::Printf(TEXT("%" UINT64_FMT "'%s'"), Task.NodeId, Task.Node ? *Task.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString() : TEXT("")));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogTaskCullingBegin(FPCGTaskId CompletedTaskId, uint64 InactiveOutputPinBitmask, const TArray<FPCGPinId>& PinIdsToDeactivate)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!CullingLogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("BEGIN CullInactiveDownstreamNodes, CompletedTaskId: %u, InactiveOutputPinBitmask: %u, Deactivating pin IDs: %s"),
			CompletedTaskId, InactiveOutputPinBitmask, *GetPinsToDeactivateString(PinIdsToDeactivate));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogTaskCullingBeginLoop(FPCGTaskId PinTaskId, uint64 PinIndex, const TArray<FPCGPinId>& PinIdsToDeactivate)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!CullingLogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("LOOP: DEACTIVATE %u_%u, remaining IDs: %s"), PinTaskId, PinIndex, *GetPinsToDeactivateString(PinIdsToDeactivate));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogTaskCullingUpdatedPinDeps(FPCGTaskId TaskId, const FPCGPinDependencyExpression& PinDependency, bool bDependencyExpressionBecameFalse)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!CullingLogEnabled())
		{
			return;
		}

		const FString PinDependencyString =
#if WITH_EDITOR
			PinDependency.ToString();
#else
			TEXT("MISSINGPINDEPS");
#endif

		UE_LOG(LogPCG, Log, TEXT("UPDATED PIN DEP EXPRESSION (task ID %u): %s"), TaskId, *PinDependencyString);

		if (bDependencyExpressionBecameFalse)
		{
			UE_LOG(LogPCG, Log, TEXT("CULL task ID %u"), TaskId);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteStore(const FPCGContext* InContext, EPCGHiGenGrid InGenerationGrid, int32 InFromGridSize, int32 InToGridSize, const FString& InResourcePath, int32 InDataItemCount)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] STORE. GenerationGridSize=%u, FromGridSize=%u, ToGridSize=%u, Path=%s, DataItems=%d"),
			*PCGLog::GetExecutionSourceName(InContext->ExecutionSource.Get(), /*bUseLabel=*/true),
			PCGHiGenGrid::IsValidGrid(InGenerationGrid) ? PCGHiGenGrid::GridToGridSize(InGenerationGrid) : PCGHiGenGrid::UnboundedGridSize(),
			InFromGridSize,
			InToGridSize,
			*InResourcePath,
			InDataItemCount);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieve(const FPCGContext* InContext, EPCGHiGenGrid InGenerationGrid, int32 InFromGridSize, int32 InToGridSize, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE. GenerationGridSize=%u, FromGridSize=%u, ToGridSize=%u, Path=%s"),
			*PCGLog::GetExecutionSourceName(InContext->ExecutionSource.Get(), /*bUseLabel=*/true),
			PCGHiGenGrid::IsValidGrid(InGenerationGrid) ? PCGHiGenGrid::GridToGridSize(InGenerationGrid) : PCGHiGenGrid::UnboundedGridSize(),
			InFromGridSize,
			InToGridSize,
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieveSuccess(const FPCGContext* InContext, const IPCGGraphExecutionSource* InExecutionSource, const FString& InResourcePath, int32 InDataItemCount)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: SUCCESS. Path=%s, DataItems=%d"),
			*PCGLog::GetExecutionSourceName(InContext->ExecutionSource.Get(), /*bUseLabel=*/true),
			*InResourcePath,
			InDataItemCount);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieveScheduleGraph(const FPCGContext* InContext, const IPCGGraphExecutionSource* InScheduledSource, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: SCHEDULE GRAPH. Source=%s, Path=%s"),
			*PCGLog::GetExecutionSourceName(InContext->ExecutionSource.Get(), /*bUseLabel=*/true),
			*PCGLog::GetExecutionSourceName(InScheduledSource, /*bUseLabel=*/true),
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieveWaitOnScheduledGraph(const FPCGContext* InContext, const IPCGGraphExecutionSource* InWaitOnSource, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: WAIT FOR SCHEDULED GRAPH. Source=%s, Path=%s"),
			*PCGLog::GetExecutionSourceName(InContext->ExecutionSource.Get(), /*bUseLabel=*/true),
			*PCGLog::GetExecutionSourceName(InWaitOnSource, /*bUseLabel=*/true),
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}
	
	void LogGridLinkageTaskExecuteRetrieveWakeUp(const FPCGContext* InContext, const IPCGGraphExecutionSource* InWokenBySource)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: WOKEN BY Source=%s"),
			*PCGLog::GetExecutionSourceName(InContext->ExecutionSource.Get(), /*bUseLabel=*/true),
			*PCGLog::GetExecutionSourceName(InWokenBySource, /*bUseLabel=*/true));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieveNoLocalSource(const FPCGContext* InContext, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Warning, TEXT("[GRIDLINKING] [%s] RETRIEVE: FAILED: No overlapping local source found. This may be expected. Path=%s"),
			*PCGLog::GetExecutionSourceName(InContext->ExecutionSource.Get(), /*bUseLabel=*/true),
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieveNoData(const FPCGContext* InContext, const IPCGGraphExecutionSource* InExecutionSource, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Warning, TEXT("[GRIDLINKING] [%s] RETRIEVE: FAILED: No data found on local source. Source=%s, Path=%s"),
			*PCGLog::GetExecutionSourceName(InContext->ExecutionSource.Get(), /*bUseLabel=*/true),
			*PCGLog::GetExecutionSourceName(InExecutionSource, /*bUseLabel=*/true),
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}
}
