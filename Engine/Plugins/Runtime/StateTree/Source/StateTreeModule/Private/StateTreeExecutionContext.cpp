// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreeDelegate.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeReference.h"
#include "AutoRTFM.h"
#include "Containers/StaticArray.h"
#include "Debugger/StateTreeDebug.h"
#include "Debugger/StateTreeTrace.h"
#include "Debugger/StateTreeTraceTypes.h"
#include "Misc/ScopeExit.h"
#include "VisualLogger/VisualLogger.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Logging/LogScopedVerbosityOverride.h"

#define STATETREE_LOG(Verbosity, Format, ...) UE_VLOG_UELOG(GetOwner(), LogStateTree, Verbosity, TEXT("%s: ") Format, *GetInstanceDescriptionInternal(), ##__VA_ARGS__)
#define STATETREE_CLOG(Condition, Verbosity, Format, ...) UE_CVLOG_UELOG((Condition), GetOwner(), LogStateTree, Verbosity, TEXT("%s: ") Format, *GetInstanceDescriptionInternal(), ##__VA_ARGS__)

namespace UE::StateTree::Debug
{
	constexpr int32 IndentSize = 2;	// Debug printing indent for hierarchical data.
	static std::atomic<uint32> InstanceSerialNumber = 0;
}; // namespace

namespace UE::StateTree::ExecutionContext
{
namespace Private
{
	bool bCopyBoundPropertiesOnNonTickedTask = false;
	FAutoConsoleVariableRef CVarCopyBoundPropertiesOnNonTickedTask(
		TEXT("StateTree.CopyBoundPropertiesOnNonTickedTask"),
		bCopyBoundPropertiesOnNonTickedTask,
		TEXT("When ticking the tasks, copy the bindings when the tasks are not ticked because it disabled ticking or completed. The bindings are not updated on failure.")
	);

	bool bTickGlobalNodesFollowingTreeHierarchy = true;
	FAutoConsoleVariableRef CVarTickGlobalNodesFollowingTreeHierarchy(
		TEXT("StateTree.TickGlobalNodesFollowingTreeHierarchy"),
		bTickGlobalNodesFollowingTreeHierarchy,
		TEXT("If true, then the global evaluators and global tasks are ticked following the asset hierarchy.\n")
		TEXT("The order is (1)root evaluators, (2)root global tasks, (3)state tasks, (4)linked asset evaluators, (5)linked asset global tasks, (6)linked asset state tasks.\n")
		TEXT("If false (5.5 behavior), then all the global nodes, from all linked assets, are evaluated, then the state tasks are ticked.\n")
		TEXT("You should upgrade your asset. This option is to help migrate to the new behavior.")
	);

	bool bGlobalTasksCompleteOwningFrame = true;
	FAutoConsoleVariableRef CVarGlobalTasksCompleteFrame(
		TEXT("StateTree.GlobalTasksCompleteOwningFrame"),
		bGlobalTasksCompleteOwningFrame,
		TEXT("If true, the global tasks complete the tree they are part of. The root or the linked state. 5.6 behavior.\n")
		TEXT("If false, the global tasks complete the root tree execution (even for linked assets). 5.5 behavior.")
		TEXT("You should upgrade your asset. This option is to help migrate to the new behavior.")
	);

	constexpr uint32 NumEStateTreeRunStatus()
	{
#ifdef UE_STATETREE_ESTATETREERUNSTATUS_PRIVATE
#error UE_STATETREE_ESTATETREERUNSTATUS_PRIVATE_ALREADY_DEFINED
#endif

#define UE_STATETREE_ESTATETREERUNSTATUS_PRIVATE(EnumValue) ++Number;

		int32 Number = 0;
		FOREACH_ENUM_ESTATETREERUNSTATUS(UE_STATETREE_ESTATETREERUNSTATUS_PRIVATE)

#undef UE_STATETREE_ESTATETREERUNSTATUS_PRIVATE
			return Number;
	}
	constexpr uint32 NumEStateTreeFinishTaskType()
	{
#ifdef UE_STATETREE_ESTATETREEFINISHTASKTYPE_PRIVATE
#error UE_STATETREE_ESTATETREEFINISHTASKTYPE_PRIVATE_ALREADY_DEFINED
#endif

#define UE_STATETREE_ESTATETREEFINISHTASKTYPE_PRIVATE(EnumValue) ++Number;

		int32 Number = 0;
		FOREACH_ENUM_ESTATETREEFINISHTASKTYPE(UE_STATETREE_ESTATETREEFINISHTASKTYPE_PRIVATE)

#undef UE_STATETREE_ESTATETREEFINISHTASKTYPE_PRIVATE
			return Number;
	}
}

	bool MarkDelegateAsBroadcasted(FStateTreeDelegateDispatcher Dispatcher, const FStateTreeExecutionFrame& CurrentFrame, FStateTreeInstanceStorage& Storage)
	{
		const UStateTree* StateTree = CurrentFrame.StateTree;
		check(StateTree);

		for (FStateTreeStateHandle ActiveState : CurrentFrame.ActiveStates)
		{
			const FCompactStateTreeState* State = StateTree->GetStateFromHandle(ActiveState);
			check(State);

			if (!State->bHasDelegateTriggerTransitions)
			{
				continue;
			}

			const int32 TransitionEnd = State->TransitionsBegin + State->TransitionsNum;
			for (int32 TransitionIndex = State->TransitionsBegin; TransitionIndex <TransitionEnd; ++TransitionIndex)
			{
				const FCompactStateTransition* Transition = StateTree->GetTransitionFromIndex(FStateTreeIndex16(TransitionIndex));
				check(Transition);
				if (Transition->RequiredDelegateDispatcher == Dispatcher)
				{
					ensureMsgf(EnumHasAnyFlags(Transition->Trigger, EStateTreeTransitionTrigger::OnDelegate), TEXT("The transition should have both (a valid dispatcher and the OnDelegate flag) or none."));
					Storage.MarkDelegateAsBroadcasted(Dispatcher);
					return true;
				}
			}
		}

		return false;
	}

	/** @return in order {Failed, Succeeded, Stopped, Running, Unset} */
	EStateTreeRunStatus GetPriorityRunStatus(EStateTreeRunStatus A, EStateTreeRunStatus B)
	{
		static_assert((int32)EStateTreeRunStatus::Running == 0);
		static_assert((int32)EStateTreeRunStatus::Stopped == 1);
		static_assert((int32)EStateTreeRunStatus::Succeeded == 2);
		static_assert((int32)EStateTreeRunStatus::Failed == 3);
		static_assert((int32)EStateTreeRunStatus::Unset == 4);
		static_assert(Private::NumEStateTreeRunStatus() == 5, "The number of entries in EStateTreeRunStatus changed. Update GetPriorityRunStatus.");

		static constexpr int32 PriorityMatrix[] = { 1, 2, 3, 4, 0 };
		return PriorityMatrix[(uint8)A] > PriorityMatrix[(uint8)B] ? A : B;
	}

	UE::StateTree::ETaskCompletionStatus CastToTaskStatus(EStateTreeFinishTaskType FinishTask)
	{
		static_assert(Private::NumEStateTreeFinishTaskType() == 2, "The number of entries in EStateTreeFinishTaskType changed. Update CastToTaskStatus.");

		return FinishTask == EStateTreeFinishTaskType::Succeeded ? UE::StateTree::ETaskCompletionStatus::Succeeded : UE::StateTree::ETaskCompletionStatus::Failed;
	}

	EStateTreeRunStatus CastToRunStatus(EStateTreeFinishTaskType FinishTask)
	{
		static_assert(Private::NumEStateTreeFinishTaskType() == 2, "The number of entries in EStateTreeFinishTaskType changed. Update CastToRunStatus.");

		return FinishTask == EStateTreeFinishTaskType::Succeeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
	}

	UE::StateTree::ETaskCompletionStatus CastToTaskStatus(EStateTreeRunStatus InStatus)
	{
		static_assert((int32)EStateTreeRunStatus::Running == (int32)UE::StateTree::ETaskCompletionStatus::Running);
		static_assert((int32)EStateTreeRunStatus::Stopped == (int32)UE::StateTree::ETaskCompletionStatus::Stopped);
		static_assert((int32)EStateTreeRunStatus::Succeeded == (int32)UE::StateTree::ETaskCompletionStatus::Succeeded);
		static_assert((int32)EStateTreeRunStatus::Failed == (int32)UE::StateTree::ETaskCompletionStatus::Failed);
		static_assert(Private::NumEStateTreeRunStatus() == 5, "The number of entries in EStateTreeRunStatus changed. Update CastToTaskStatus.");

		return InStatus != EStateTreeRunStatus::Unset ? (UE::StateTree::ETaskCompletionStatus)InStatus : UE::StateTree::ETaskCompletionStatus::Running;
	}

	EStateTreeRunStatus CastToRunStatus(UE::StateTree::ETaskCompletionStatus InStatus)
	{
		static_assert((int32)EStateTreeRunStatus::Running == (int32)UE::StateTree::ETaskCompletionStatus::Running);
		static_assert((int32)EStateTreeRunStatus::Stopped == (int32)UE::StateTree::ETaskCompletionStatus::Stopped);
		static_assert((int32)EStateTreeRunStatus::Succeeded == (int32)UE::StateTree::ETaskCompletionStatus::Succeeded);
		static_assert((int32)EStateTreeRunStatus::Failed == (int32)UE::StateTree::ETaskCompletionStatus::Failed);
		static_assert(UE::StateTree::NumberOfTaskStatus == 4, "The number of entries in EStateTreeRunStatus changed. Update CastToRunStatus.");

		return (EStateTreeRunStatus)InStatus;
	}
}

/**
 * FStateTreeReadOnlyExecutionContext implementation
 */
FStateTreeReadOnlyExecutionContext::FStateTreeReadOnlyExecutionContext(TNotNull<UObject*> InOwner, TNotNull<const UStateTree*> InStateTree, FStateTreeInstanceData& InInstanceData)
	: FStateTreeReadOnlyExecutionContext(InOwner, InStateTree, InInstanceData.GetMutableStorage())
{
}

FStateTreeReadOnlyExecutionContext::FStateTreeReadOnlyExecutionContext(TNotNull<UObject*> InOwner, TNotNull<const UStateTree*> InStateTree, FStateTreeInstanceStorage& InStorage)
	: Owner(*InOwner)
	, RootStateTree(*InStateTree)
	, Storage(InStorage)
{
	Storage.AcquireReadAccess();
	if (IsValid())
	{
		Storage.GetRuntimeValidation().SetContext(&Owner, &RootStateTree);
	}
}

FStateTreeReadOnlyExecutionContext::~FStateTreeReadOnlyExecutionContext()
{
	Storage.ReleaseReadAccess();
}

FStateTreeScheduledTick FStateTreeReadOnlyExecutionContext::GetNextScheduledTick() const
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return FStateTreeScheduledTick::MakeSleep();
	}

	const FStateTreeExecutionState& Exec = Storage.GetExecutionState();
	if (Exec.TreeRunStatus != EStateTreeRunStatus::Running)
	{
		return FStateTreeScheduledTick::MakeSleep();
	}

	// USchema::IsScheduleTickAllowed.
	//Used the state tree cached value to prevent runtime changes that could affect the behavior.
	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
	{
		const FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		if (!CurrentFrame.StateTree->IsScheduledTickAllowed())
		{
			return FStateTreeScheduledTick::MakeEveryFrames();
		}
	}

	const FStateTreeEventQueue& EventQueue = Storage.GetEventQueue();
	const bool bHasEvents = EventQueue.HasEvents();
	const bool bHasBroadcastedDelegates = Storage.HasBroadcastedDelegates();

	// We wish to return in order: EveryFrames, then NextFrame, then CustomTickRate, then Sleep.
	// Do we have a state that requires a tick or is waiting for an event.
	TOptional<float> CustomTickRate;
	{
		bool bHasTaskWithEveryFramesTick = false;
		for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
		{
			const FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
			const UStateTree* CurrentStateTree = CurrentFrame.StateTree;

			// Test global tasks.
			if (CurrentStateTree->DoesRequestTickGlobalTasks(bHasEvents))
			{
				bHasTaskWithEveryFramesTick = true;
			}

			// Test active states tasks.
			for (int32 StateIndex = 0; StateIndex < CurrentFrame.ActiveStates.Num(); ++StateIndex)
			{
				const FStateTreeStateHandle CurrentHandle = CurrentFrame.ActiveStates[StateIndex];
				const FCompactStateTreeState& State = CurrentStateTree->States[CurrentHandle.Index];
				if (State.bEnabled)
				{
					if (State.bHasCustomTickRate)
					{
						CustomTickRate = CustomTickRate.IsSet() ? FMath::Min(CustomTickRate.GetValue(), State.CustomTickRate) : State.CustomTickRate;
					}
					else if (!CustomTickRate.IsSet())
					{
						if (State.DoesRequestTickTasks(bHasEvents)
							|| State.ShouldTickTransitions(bHasEvents, bHasBroadcastedDelegates)
							)
						{
							// todo: ShouldTickTransitions has onevent or ontick. both can be already triggered and we are waiting for the delay
							bHasTaskWithEveryFramesTick = true;
						}
					}
				}
			}
		}

		if (!CustomTickRate.IsSet() && bHasTaskWithEveryFramesTick)
		{
			return FStateTreeScheduledTick::MakeEveryFrames();
		}

		// If one state has a custom tick rate, then it overrides the tick rate for all states.
		//Only return the CustomTickRate if it's > than NextFrame, the custom tick rate will be processed at the end.
		if (CustomTickRate.IsSet() && CustomTickRate.GetValue() <= 0.0f)
		{
			// A state might override the custom tick rate with > 0, then another state overrides it again with 0 to tick back every frame.
			return FStateTreeScheduledTick::MakeEveryFrames();
		}
	}

	// Requests
	if (Exec.HasScheduledTickRequests())
	{
		// The ScheduledTickRequests loop value is cached. Returns every frame or next frame. CustomTime needs to wait after the other tests.
		const FStateTreeScheduledTick ScheduledTickRequest = Exec.GetScheduledTickRequest();
		if (ScheduledTickRequest.ShouldTickEveryFrames() || ScheduledTickRequest.ShouldTickOnceNextFrame())
		{
			return ScheduledTickRequest;
		}
		const float CachedTickRate = ScheduledTickRequest.GetTickRate();
		CustomTickRate = CustomTickRate.IsSet() ? FMath::Min(CustomTickRate.GetValue(), CachedTickRate) : CachedTickRate;
	}

	// Transitions
	if (Storage.GetTransitionRequests().Num() > 0)
	{
		return FStateTreeScheduledTick::MakeNextFrame();
	}

	// Events are cleared every tick.
	if (bHasEvents && Storage.IsOwningEventQueue())
	{
		return FStateTreeScheduledTick::MakeNextFrame();
	}

	// Completed task. For EnterState or for user that only called TickTasks and not TickTransitions.
	if (Exec.bHasPendingCompletedState)
	{
		return FStateTreeScheduledTick::MakeNextFrame();
	}

	// Min of all delayed transitions.
	if (Exec.DelayedTransitions.Num() > 0)
	{
		for (const FStateTreeTransitionDelayedState& Transition : Exec.DelayedTransitions)
		{
			CustomTickRate = CustomTickRate.IsSet() ? FMath::Min(CustomTickRate.GetValue(), Transition.TimeLeft) : Transition.TimeLeft;
		}
	}

	// Custom tick rate for tasks and transitions.
	if (CustomTickRate.IsSet())
	{
		return FStateTreeScheduledTick::MakeCustomTickRate(CustomTickRate.GetValue());
	}

	return FStateTreeScheduledTick::MakeSleep();
}

EStateTreeRunStatus FStateTreeReadOnlyExecutionContext::GetStateTreeRunStatus() const
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return EStateTreeRunStatus::Failed;
	}

	return Storage.GetExecutionState().TreeRunStatus;
}

EStateTreeRunStatus FStateTreeReadOnlyExecutionContext::GetLastTickStatus() const
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return EStateTreeRunStatus::Failed;
	}

	const FStateTreeExecutionState& Exec = Storage.GetExecutionState();
	return Exec.LastTickStatus;
}

TConstArrayView<FStateTreeExecutionFrame> FStateTreeReadOnlyExecutionContext::GetActiveFrames() const
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return TConstArrayView<FStateTreeExecutionFrame>();
	}

	const FStateTreeExecutionState& Exec = Storage.GetExecutionState();
	return Exec.ActiveFrames;
}

FString FStateTreeReadOnlyExecutionContext::GetActiveStateName() const
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return FString();
	}

	const FStateTreeExecutionState& Exec = Storage.GetExecutionState();

	TStringBuilder<1024> FullStateName;

	const UStateTree* LastStateTree = &RootStateTree;
	int32 Indent = 0;

	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); FrameIndex++)
	{
		const FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UStateTree* CurrentStateTree = CurrentFrame.StateTree;

		// Append linked state marker at the end of the previous line.
		if (Indent > 0)
		{
			FullStateName << TEXT(" >");
		}
		// If tree has changed, append that too.
		if (CurrentFrame.StateTree != LastStateTree)
		{
			FullStateName << TEXT(" [");
			FullStateName << CurrentFrame.StateTree.GetFName();
			FullStateName << TEXT(']');

			LastStateTree = CurrentFrame.StateTree;
		}

		for (int32 Index = 0; Index < CurrentFrame.ActiveStates.Num(); Index++)
		{
			const FStateTreeStateHandle Handle = CurrentFrame.ActiveStates[Index];
			if (Handle.IsValid())
			{
				const FCompactStateTreeState& State = CurrentStateTree->States[Handle.Index];
				if (Indent > 0)
				{
					FullStateName += TEXT("\n");
				}
				FullStateName.Appendf(TEXT("%*s-"), Indent * 3, TEXT("")); // Indent
				FullStateName << State.Name;
				Indent++;
			}
		}
	}

	switch (Exec.TreeRunStatus)
	{
	case EStateTreeRunStatus::Failed:
		FullStateName << TEXT(" FAILED\n");
		break;
	case EStateTreeRunStatus::Succeeded:
		FullStateName << TEXT(" SUCCEEDED\n");
		break;
	case EStateTreeRunStatus::Running:
		// Empty
		break;
	default:
		FullStateName << TEXT("--\n");
	}

	return FullStateName.ToString();
}

TArray<FName> FStateTreeReadOnlyExecutionContext::GetActiveStateNames() const
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return TArray<FName>();
	}

	TArray<FName> Result;
	const FStateTreeExecutionState& Exec = Storage.GetExecutionState();

	// Active States
	for (const FStateTreeExecutionFrame& CurrentFrame : Exec.ActiveFrames)
	{
		const UStateTree* CurrentStateTree = CurrentFrame.StateTree;
		for (int32 Index = 0; Index < CurrentFrame.ActiveStates.Num(); Index++)
		{
			const FStateTreeStateHandle Handle = CurrentFrame.ActiveStates[Index];
			if (Handle.IsValid())
			{
				const FCompactStateTreeState& State = CurrentStateTree->States[Handle.Index];
				Result.Add(State.Name);
			}
		}
	}

	return Result;
}

#if WITH_GAMEPLAY_DEBUGGER
FString FStateTreeReadOnlyExecutionContext::GetDebugInfoString() const
{
	TStringBuilder<2048> DebugString;
	DebugString << TEXT("StateTree (asset: '");
	RootStateTree.GetFullName(DebugString);
	DebugString << TEXT("')");

	if (IsValid())
	{
		const FStateTreeExecutionState& Exec = Storage.GetExecutionState();

		DebugString << TEXT("Status: ");
		DebugString << UEnum::GetDisplayValueAsText(Exec.TreeRunStatus).ToString();
		DebugString << TEXT("\n");

		// Active States
		DebugString << TEXT("Current State:\n");
		for (const FStateTreeExecutionFrame& CurrentFrame : Exec.ActiveFrames)
		{
			const UStateTree* CurrentStateTree = CurrentFrame.StateTree;

			if (CurrentFrame.bIsGlobalFrame)
			{
				DebugString.Appendf(TEXT("\nEvaluators\n  [ %-30s | %8s | %15s ]\n"),
					TEXT("Name"), TEXT("Bindings"), TEXT("Data Handle"));
				for (int32 EvalIndex = CurrentStateTree->EvaluatorsBegin; EvalIndex < (CurrentStateTree->EvaluatorsBegin + CurrentStateTree->EvaluatorsNum); EvalIndex++)
				{
					const FStateTreeEvaluatorBase& Eval = CurrentStateTree->Nodes[EvalIndex].Get<const FStateTreeEvaluatorBase>();
					DebugString.Appendf(TEXT("| %-30s | %8d | %15s |\n"),
						*Eval.Name.ToString(), Eval.BindingsBatch.Get(), *Eval.InstanceDataHandle.Describe());
				}

				DebugString << TEXT("\nGlobal Tasks\n");
				for (int32 TaskIndex = CurrentStateTree->GlobalTasksBegin; TaskIndex < (CurrentStateTree->GlobalTasksBegin + CurrentStateTree->GlobalTasksNum); TaskIndex++)
				{
					const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
					if (Task.bTaskEnabled)
					{
						DebugString << Task.GetDebugInfo(*this);

					}
				}
			}

			for (int32 Index = 0; Index < CurrentFrame.ActiveStates.Num(); Index++)
			{
				FStateTreeStateHandle Handle = CurrentFrame.ActiveStates[Index];
				if (Handle.IsValid())
				{
					const FCompactStateTreeState& State = CurrentStateTree->States[Handle.Index];
					DebugString << TEXT('[');
					DebugString << State.Name;
					DebugString << TEXT("]\n");

					if (State.TasksNum > 0)
					{
						DebugString += TEXT("\nTasks:\n");
						for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
						{
							const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
							if (Task.bTaskEnabled)
							{
								DebugString << Task.GetDebugInfo(*this);
							}
						}
					}
				}
			}
		}
	}
	else
	{
		DebugString << TEXT("StateTree context is not initialized properly.");
	}

	return DebugString.ToString();
}
#endif // WITH_GAMEPLAY_DEBUGGER

#if WITH_STATETREE_DEBUG
int32 FStateTreeReadOnlyExecutionContext::GetStateChangeCount() const
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return 0;
	}

	const FStateTreeExecutionState& Exec = Storage.GetExecutionState();
	return Exec.StateChangeCount;
}

void FStateTreeReadOnlyExecutionContext::DebugPrintInternalLayout()
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogStateTree, ELogVerbosity::Log);
	UE_LOG(LogStateTree, Log, TEXT("%s"), *RootStateTree.DebugInternalLayoutAsString());
}
#endif // WITH_STATETREE_DEBUG

FString FStateTreeReadOnlyExecutionContext::GetInstanceDescriptionInternal() const
{
	const TInstancedStruct<FStateTreeExecutionExtension>& ExecutionExtension = Storage.GetExecutionState().ExecutionExtension;
	return ExecutionExtension.IsValid()
		? ExecutionExtension.Get().GetInstanceDescription(FStateTreeExecutionExtension::FContextParameters(Owner, RootStateTree, Storage))
		: Owner.GetName();
}

#if WITH_STATETREE_TRACE
UE_AUTORTFM_ALWAYS_OPEN
static uint32 GetNextInstanceSerialNumber()
{
	// The instance serial number is only used to synthesize unique instance debug IDs; rollback isn't needed.
	return ++UE::StateTree::Debug::InstanceSerialNumber;
}

FStateTreeInstanceDebugId FStateTreeReadOnlyExecutionContext::GetInstanceDebugId() const
{
	FStateTreeInstanceDebugId& InstanceDebugId = Storage.GetMutableExecutionState().InstanceDebugId;
	if (!InstanceDebugId.IsValid())
	{
		InstanceDebugId = FStateTreeInstanceDebugId(GetTypeHash(GetInstanceDescriptionInternal()), GetNextInstanceSerialNumber());
	}
	return InstanceDebugId;
}
#endif // WITH_STATETREE_TRACE

/**
 * FStateTreeMinimalExecutionContext implementation
 */
// Deprecated
FStateTreeMinimalExecutionContext::FStateTreeMinimalExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData)
	: FStateTreeMinimalExecutionContext(&InOwner, &InStateTree, InInstanceData.GetMutableStorage())
{
}

// Deprecated
FStateTreeMinimalExecutionContext::FStateTreeMinimalExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceStorage& InStorage)
	: FStateTreeMinimalExecutionContext(&InOwner, &InStateTree, InStorage)
{
}

FStateTreeMinimalExecutionContext::FStateTreeMinimalExecutionContext(TNotNull<UObject*> InOwner, TNotNull<const UStateTree*> InStateTree, FStateTreeInstanceData& InInstanceData)
	: FStateTreeMinimalExecutionContext(InOwner, InStateTree, InInstanceData.GetMutableStorage())
{
}

FStateTreeMinimalExecutionContext::FStateTreeMinimalExecutionContext(TNotNull<UObject*> InOwner, TNotNull<const UStateTree*> InStateTree, FStateTreeInstanceStorage& InStorage)
	: FStateTreeReadOnlyExecutionContext(InOwner, InStateTree, InStorage)
{
	Storage.AcquireWriteAccess();
}

FStateTreeMinimalExecutionContext::~FStateTreeMinimalExecutionContext()
{
	Storage.ReleaseWriteAccess();
}

UE::StateTree::FScheduledTickHandle FStateTreeMinimalExecutionContext::AddScheduledTickRequest(FStateTreeScheduledTick ScheduledTick)
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return UE::StateTree::FScheduledTickHandle();
	}

	UE::StateTree::FScheduledTickHandle Result = Storage.GetMutableExecutionState().AddScheduledTickRequest(ScheduledTick);
	ScheduleNextTick();
	return Result;
}

void FStateTreeMinimalExecutionContext::UpdateScheduledTickRequest(UE::StateTree::FScheduledTickHandle Handle, FStateTreeScheduledTick ScheduledTick)
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return;
	}

	if (Storage.GetMutableExecutionState().UpdateScheduledTickRequest(Handle, ScheduledTick))
	{
		ScheduleNextTick();
	}
}

void FStateTreeMinimalExecutionContext::RemoveScheduledTickRequest(UE::StateTree::FScheduledTickHandle Handle)
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return;
	}

	if (Storage.GetMutableExecutionState().RemoveScheduledTickRequest(Handle))
	{
		ScheduleNextTick();
	}
}

void FStateTreeMinimalExecutionContext::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_SendEvent);

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return;
	}

	STATETREE_LOG(VeryVerbose, TEXT("Send Event '%s'"), *Tag.ToString());
	UE_STATETREE_DEBUG_LOG_EVENT(this, Log, TEXT("Send Event '%s'"), *Tag.ToString());

	FStateTreeEventQueue& LocalEventQueue = Storage.GetMutableEventQueue();
	LocalEventQueue.SendEvent(&Owner, Tag, Payload, Origin);
	ScheduleNextTick();

	UE_STATETREE_DEBUG_SEND_EVENT(this, &RootStateTree, Tag, Payload, Origin);
}

void FStateTreeMinimalExecutionContext::ScheduleNextTick()
{
	TInstancedStruct<FStateTreeExecutionExtension>& ExecutionExtension = Storage.GetMutableExecutionState().ExecutionExtension;
	if (bAllowedToScheduleNextTick && RootStateTree.IsScheduledTickAllowed() && ExecutionExtension.IsValid())
	{
		ExecutionExtension.GetMutable().ScheduleNextTick(FStateTreeExecutionExtension::FContextParameters(Owner, RootStateTree, Storage));
	}
}

/**
 * FStateTreeExecutionContext::FCurrentlyProcessedFrameScope implementation
 */
FStateTreeExecutionContext::FCurrentlyProcessedFrameScope::FCurrentlyProcessedFrameScope(FStateTreeExecutionContext& InContext, const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame): Context(InContext)
{
	check(CurrentFrame.StateTree);
	FStateTreeInstanceStorage* SharedInstanceDataStorage = &CurrentFrame.StateTree->GetSharedInstanceData()->GetMutableStorage();

	SavedFrame = Context.CurrentlyProcessedFrame;
	SavedParentFrame = Context.CurrentlyProcessedParentFrame;
	SavedSharedInstanceDataStorage = Context.CurrentlyProcessedSharedInstanceStorage;
	Context.CurrentlyProcessedFrame = &CurrentFrame;
	Context.CurrentlyProcessedParentFrame = CurrentParentFrame;
	Context.CurrentlyProcessedSharedInstanceStorage = SharedInstanceDataStorage;
	
	UE_STATETREE_DEBUG_INSTANCE_FRAME_EVENT(&Context, Context.CurrentlyProcessedFrame);
}

FStateTreeExecutionContext::FCurrentlyProcessedFrameScope::~FCurrentlyProcessedFrameScope()
{
	Context.CurrentlyProcessedFrame = SavedFrame;
	Context.CurrentlyProcessedParentFrame = SavedParentFrame;
	Context.CurrentlyProcessedSharedInstanceStorage = SavedSharedInstanceDataStorage;

	if (Context.CurrentlyProcessedFrame)
	{
		UE_STATETREE_DEBUG_INSTANCE_FRAME_EVENT(&Context, Context.CurrentlyProcessedFrame);
	}
}

/**
 * FStateTreeExecutionContext::FNodeInstanceDataScope implementation
 */
FStateTreeExecutionContext::FNodeInstanceDataScope::FNodeInstanceDataScope(FStateTreeExecutionContext& InContext, const FStateTreeNodeBase* InNode, const int32 InNodeIndex, const FStateTreeDataHandle InNodeDataHandle, const FStateTreeDataView InNodeInstanceData)
	: Context(InContext)
{
	SavedNode = Context.CurrentNode;
	SavedNodeIndex = Context.CurrentNodeIndex;
	SavedNodeDataHandle = Context.CurrentNodeDataHandle;
	SavedNodeInstanceData = Context.CurrentNodeInstanceData;
	Context.CurrentNode = InNode;
	Context.CurrentNodeIndex = InNodeIndex;
	Context.CurrentNodeDataHandle = InNodeDataHandle;
	Context.CurrentNodeInstanceData = InNodeInstanceData;
}

FStateTreeExecutionContext::FNodeInstanceDataScope::~FNodeInstanceDataScope()
{
	Context.CurrentNodeDataHandle = SavedNodeDataHandle;
	Context.CurrentNodeInstanceData = SavedNodeInstanceData;
	Context.CurrentNodeIndex = SavedNodeIndex;
	Context.CurrentNode = SavedNode;
}

/**
 * FStateTreeExecutionContext implementation
 */
FStateTreeExecutionContext::FStateTreeExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData, const FOnCollectStateTreeExternalData& InCollectExternalDataDelegate, const EStateTreeRecordTransitions RecordTransitions)
	: FStateTreeExecutionContext(&InOwner, &InStateTree, InInstanceData, InCollectExternalDataDelegate, RecordTransitions)
{
}

FStateTreeExecutionContext::FStateTreeExecutionContext(TNotNull<UObject*> InOwner, TNotNull<const UStateTree*> InStateTree, FStateTreeInstanceData& InInstanceData, const FOnCollectStateTreeExternalData& InCollectExternalDataDelegate, const EStateTreeRecordTransitions RecordTransitions)
	: FStateTreeMinimalExecutionContext(InOwner, InStateTree, InInstanceData)
	, InstanceData(InInstanceData)
	, CollectExternalDataDelegate(InCollectExternalDataDelegate)
{
	if (IsValid())
	{
		// Initialize data views for all possible items.
		ContextAndExternalDataViews.SetNum(RootStateTree.GetNumContextDataViews());
		EventQueue = InstanceData.GetSharedMutableEventQueue();
		bRecordTransitions = RecordTransitions == EStateTreeRecordTransitions::Yes;
	}
	else
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree asset is not valid ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
	}
}

FStateTreeExecutionContext::FStateTreeExecutionContext(const FStateTreeExecutionContext& InContextToCopy, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData)
	: FStateTreeExecutionContext(InContextToCopy, &InStateTree, InInstanceData)
{
}

FStateTreeExecutionContext::FStateTreeExecutionContext(const FStateTreeExecutionContext& InContextToCopy, TNotNull<const UStateTree*> InStateTree, FStateTreeInstanceData& InInstanceData)
	: FStateTreeExecutionContext(&InContextToCopy.Owner, InStateTree, InInstanceData, InContextToCopy.CollectExternalDataDelegate)
{
	SetLinkedStateTreeOverrides(InContextToCopy.LinkedAssetStateTreeOverrides);
	const bool bIsSameSchema = RootStateTree.GetSchema()->GetClass() == InContextToCopy.GetStateTree()->GetSchema()->GetClass();
	if (bIsSameSchema)
	{
		for (const FStateTreeExternalDataDesc& TargetDataDesc : GetContextDataDescs())
		{
			const int32 TargetIndex = TargetDataDesc.Handle.DataHandle.GetIndex();
			ContextAndExternalDataViews[TargetIndex] = InContextToCopy.ContextAndExternalDataViews[TargetIndex];
		}
	}
	else
	{
		STATETREE_LOG(Error, TEXT("%hs: '%s' using StateTree '%s' trying to run subtree '%s' but their schemas don't match"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(InContextToCopy.GetStateTree()), *GetFullNameSafe(&RootStateTree));
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FStateTreeExecutionContext::~FStateTreeExecutionContext()
{
	// Mark external data indices as invalid
	FStateTreeExecutionState& Exec = InstanceData.GetMutableStorage().GetMutableExecutionState();
	for (FStateTreeExecutionFrame& Frame : Exec.ActiveFrames)
	{
		Frame.ExternalDataBaseIndex = {};
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FStateTreeExecutionContext::SetCollectExternalDataCallback(const FOnCollectStateTreeExternalData& Callback)
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return;
	}

	FStateTreeExecutionState& Exec = GetExecState();
	if (!ensureMsgf(Exec.CurrentPhase == EStateTreeUpdatePhase::Unset, TEXT("%hs can't be called while already in %s ('%s' using StateTree '%s')."),
		__FUNCTION__, *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString(), *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree)))
	{
		return;
	}

	CollectExternalDataDelegate = Callback;
}

void FStateTreeExecutionContext::SetLinkedStateTreeOverrides(const FStateTreeReferenceOverrides* InLinkedStateTreeOverrides)
{
	if (InLinkedStateTreeOverrides)
	{
		SetLinkedStateTreeOverrides(*InLinkedStateTreeOverrides);
	}
	else
	{
		SetLinkedStateTreeOverrides(FStateTreeReferenceOverrides());
	}
}

void FStateTreeExecutionContext::SetLinkedStateTreeOverrides(FStateTreeReferenceOverrides InLinkedStateTreeOverrides)
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return;
	}

	FStateTreeExecutionState& Exec = GetExecState();
	if (!ensureMsgf(Exec.CurrentPhase == EStateTreeUpdatePhase::Unset, TEXT("%hs can't be called while already in %s ('%s' using StateTree '%s')."),
		__FUNCTION__, *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString(), *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree)))
	{
		return;
	}

	bool bValid = true;

	// Confirms that the overrides schema matches.
	const TConstArrayView<FStateTreeReferenceOverrideItem> InOverrideItems = InLinkedStateTreeOverrides.GetOverrideItems();
	for (const FStateTreeReferenceOverrideItem& Item : InOverrideItems)
	{
		if (const UStateTree* ItemStateTree = Item.GetStateTreeReference().GetStateTree())
		{
			if (!ItemStateTree->IsReadyToRun())
			{
				STATETREE_LOG(Error, TEXT("%hs: '%s' using StateTree '%s' trying to set override '%s' but the tree is not initialized properly."),
					__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(GetStateTree()), *GetFullNameSafe(ItemStateTree));
				bValid = false;
				break;
			}

			if (!RootStateTree.HasCompatibleContextData(*ItemStateTree))
			{
				STATETREE_LOG(Error, TEXT("%hs: '%s' using StateTree '%s' trying to set override '%s' but the tree context data is not compatible."),
					__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(GetStateTree()), *GetFullNameSafe(ItemStateTree));
				bValid = false;
				break;
			}

			const UStateTreeSchema* OverrideSchema = ItemStateTree->GetSchema();
			if (ItemStateTree->GetSchema() == nullptr)
			{
				STATETREE_LOG(Error, TEXT("%hs: '%s' using StateTree '%s' trying to set override '%s' but the tree does not have a schema."),
					__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(GetStateTree()), *GetFullNameSafe(ItemStateTree));
				bValid = false;
				break;
			}
				
			const bool bIsSameSchema = RootStateTree.GetSchema()->GetClass() == OverrideSchema->GetClass();
			if (!bIsSameSchema)
			{
				STATETREE_LOG(Error, TEXT("%hs: '%s' using StateTree '%s' trying to set override '%s' but their schemas don't match."),
					__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(GetStateTree()), *GetFullNameSafe(Item.GetStateTreeReference().GetStateTree()));
				bValid = false;
				break;
			}
		}
	}

	bool bChanged = false;
	if (bValid)
	{
		LinkedAssetStateTreeOverrides = MoveTemp(InLinkedStateTreeOverrides);
		bChanged = LinkedAssetStateTreeOverrides.GetOverrideItems().Num() > 0;
	}
	else if (LinkedAssetStateTreeOverrides.GetOverrideItems().Num() > 0)
	{
		LinkedAssetStateTreeOverrides.Reset();
		bChanged = true;
	}
	
	if (bChanged)
	{
		TInstancedStruct<FStateTreeExecutionExtension>& ExecutionExtension = Storage.GetMutableExecutionState().ExecutionExtension;
		if (ExecutionExtension.IsValid())
		{
			ExecutionExtension.GetMutable().OnLinkedStateTreeOverridesSet(FStateTreeExecutionExtension::FContextParameters(Owner, RootStateTree, Storage), LinkedAssetStateTreeOverrides);
		}
	}
}

const FStateTreeReference* FStateTreeExecutionContext::GetLinkedStateTreeOverrideForTag(const FGameplayTag StateTag) const
{
	for (const FStateTreeReferenceOverrideItem& Item : LinkedAssetStateTreeOverrides.GetOverrideItems())
	{
		if (StateTag.MatchesTag(Item.GetStateTag()))
		{
			return &Item.GetStateTreeReference();
		}
	}

	return nullptr;
}

bool FStateTreeExecutionContext::FExternalGlobalParameters::Add(const FPropertyBindingCopyInfo& Copy, uint8* InParameterMemory)
{
	const int32 TypeHash = HashCombine(GetTypeHash(Copy.SourceLeafProperty), GetTypeHash(Copy.SourceIndirection));
	const int32 NumMappings = Mappings.Num();
	Mappings.Add(TypeHash, InParameterMemory);
	return Mappings.Num() > NumMappings;			
}

uint8* FStateTreeExecutionContext::FExternalGlobalParameters::Find(const FPropertyBindingCopyInfo& Copy) const
{
	const int32 TypeHash = HashCombine(GetTypeHash(Copy.SourceLeafProperty), GetTypeHash(Copy.SourceIndirection));
	if(uint8* const* MappingPtr = Mappings.Find(TypeHash))
	{
		return *MappingPtr;
	}
		
	checkf(false, TEXT("Missing external parameter data"));
	return nullptr;
}

void FStateTreeExecutionContext::FExternalGlobalParameters::Reset()
{
	Mappings.Reset();
}

void FStateTreeExecutionContext::SetExternalGlobalParameters(const FExternalGlobalParameters* Parameters)
{
	ExternalGlobalParameters = Parameters;
}

bool FStateTreeExecutionContext::AreContextDataViewsValid() const
{
	if (!IsValid())
	{
		return false;
	}
	
	bool bResult = true;
	
	for (const FStateTreeExternalDataDesc& DataDesc : RootStateTree.GetContextDataDescs())
	{
		const FStateTreeDataView& DataView = ContextAndExternalDataViews[DataDesc.Handle.DataHandle.GetIndex()];

		// Required items must have valid pointer of the expected type.  
		if (DataDesc.Requirement == EStateTreeExternalDataRequirement::Required)
		{
			if (!DataView.IsValid() || !DataDesc.IsCompatibleWith(DataView))
			{
				bResult = false;
				break;
			}
		}
		else // Optional items must have the expected type if they are set.
		{
			if (DataView.IsValid() && !DataDesc.IsCompatibleWith(DataView))
			{
				bResult = false;
				break;
			}
		}
	}
	return bResult;
}

bool FStateTreeExecutionContext::SetContextDataByName(const FName Name, FStateTreeDataView DataView)
{
	const FStateTreeExternalDataDesc* Desc = RootStateTree.GetContextDataDescs().FindByPredicate([&Name](const FStateTreeExternalDataDesc& Desc)
	{
		return Desc.Name == Name;
	});
	if (Desc)
	{
		ContextAndExternalDataViews[Desc->Handle.DataHandle.GetIndex()] = DataView;
		return true;
	}
	return false;
}

FStateTreeDataView FStateTreeExecutionContext::GetContextDataByName(const FName Name) const
{
	const FStateTreeExternalDataDesc* Desc = RootStateTree.GetContextDataDescs().FindByPredicate([&Name](const FStateTreeExternalDataDesc& Desc)
		{
			return Desc.Name == Name;
		});
	if (Desc)
	{
		return ContextAndExternalDataViews[Desc->Handle.DataHandle.GetIndex()];
	}
	return FStateTreeDataView();
}

FStateTreeWeakExecutionContext FStateTreeExecutionContext::MakeWeakExecutionContext() const
{
	return FStateTreeWeakExecutionContext(*this);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FStateTreeWeakTaskRef FStateTreeExecutionContext::MakeWeakTaskRef(const FStateTreeTaskBase& Node) const
{
	// This function has been deprecated
	check(CurrentNode == &Node);
	return MakeWeakTaskRefInternal();
}

FStateTreeWeakTaskRef FStateTreeExecutionContext::MakeWeakTaskRefInternal() const
{
	// This function has been deprecated
	FStateTreeWeakTaskRef Result;
	if (const FStateTreeExecutionFrame* Frame = GetCurrentlyProcessedFrame())
	{
		if (Frame->StateTree->Nodes.IsValidIndex(CurrentNodeIndex)
			&& Frame->StateTree->Nodes[CurrentNodeIndex].GetPtr<const FStateTreeTaskBase>() != nullptr)
		{
			Result = FStateTreeWeakTaskRef(Frame->StateTree, FStateTreeIndex16(CurrentNodeIndex));
		}
	}
	return Result;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

EStateTreeRunStatus FStateTreeExecutionContext::Start(const FInstancedPropertyBag* InitialParameters, int32 RandomSeed)
{
	const TOptional<int32> ParamRandomSeed = RandomSeed == -1 ? TOptional<int32>() : RandomSeed;
	return Start(FStartParameters{.GlobalParameters = InitialParameters, .RandomSeed = ParamRandomSeed });
}

void FStateTreeExecutionContext::SetUpdatePhaseInExecutionState(FStateTreeExecutionState& ExecutionState, const EStateTreeUpdatePhase UpdatePhase) const
{
	if (ExecutionState.CurrentPhase == UpdatePhase)
	{
		return;
	}

	if (ExecutionState.CurrentPhase != EStateTreeUpdatePhase::Unset)
	{
		UE_STATETREE_DEBUG_EXIT_PHASE(this, ExecutionState.CurrentPhase);
	}

	ExecutionState.CurrentPhase = UpdatePhase;

	if (ExecutionState.CurrentPhase != EStateTreeUpdatePhase::Unset)
	{
		UE_STATETREE_DEBUG_ENTER_PHASE(this, ExecutionState.CurrentPhase);
	}
}

EStateTreeRunStatus FStateTreeExecutionContext::Start(FStartParameters Parameters)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Start);

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeExecutionState& Exec = GetExecState();
	if (!ensureMsgf(Exec.CurrentPhase == EStateTreeUpdatePhase::Unset, TEXT("%hs can't be called while already in %s ('%s' using StateTree '%s')."),
			__FUNCTION__, *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString(), *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree)))
	{
		return EStateTreeRunStatus::Failed;
	}

	// Stop if still running previous state.
	if (Exec.TreeRunStatus == EStateTreeRunStatus::Running)
	{
		Stop();
	}

	// Initialize instance data. No active states yet, so we'll initialize the evals and global tasks.
	InstanceData.Reset();

	Storage.GetRuntimeValidation().SetContext(&Owner, &RootStateTree);
	Exec.ExecutionExtension = MoveTemp(Parameters.ExecutionExtension);
	if (Parameters.SharedEventQueue)
	{
		InstanceData.SetSharedEventQueue(Parameters.SharedEventQueue.ToSharedRef());
	}

#if WITH_STATETREE_TRACE
	// Make sure the debug id is valid. We want to construct it with the current GetInstanceDescriptionInternal
	GetInstanceDebugId();
#endif

	if (!Parameters.GlobalParameters || !SetGlobalParameters(*Parameters.GlobalParameters))
	{
		SetGlobalParameters(RootStateTree.GetDefaultParameters());
	}

	TGuardValue<bool> ScheduledNextTickScope(bAllowedToScheduleNextTick, false);

	// Initialize for the init frame.
	ensure(Exec.ActiveFrames.Num() == 0);
	FStateTreeExecutionFrame& InitFrame = Exec.ActiveFrames.AddDefaulted_GetRef();
	InitFrame.FrameID = UE::StateTree::FActiveFrameID(Storage.GenerateUniqueId());
	InitFrame.StateTree = &RootStateTree;
	InitFrame.RootState = FStateTreeStateHandle::Root;
	InitFrame.ActiveStates = {};
	InitFrame.bIsGlobalFrame = true;

	const FCompactStateTreeFrame* FrameInfo = RootStateTree.GetFrameFromHandle(FStateTreeStateHandle::Root);
	ensureAlwaysMsgf(FrameInfo, TEXT("The compiled data is invalid. It should contains the information for the root frame."));
	InitFrame.ActiveTasksStatus = FrameInfo ? FStateTreeTasksCompletionStatus(*FrameInfo) : FStateTreeTasksCompletionStatus();
	
	UpdateInstanceData({}, Exec.ActiveFrames);
	Exec.RandomStream.Initialize(Parameters.RandomSeed.IsSet() ? Parameters.RandomSeed.GetValue() : FPlatformTime::Cycles());

	if (!CollectActiveExternalData())
	{
		STATETREE_LOG(Warning, TEXT("%hs: Failed to collect external data ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return EStateTreeRunStatus::Failed;
	}

	// Must sent instance creation event first 
	UE_STATETREE_DEBUG_INSTANCE_EVENT(this, EStateTreeTraceEventType::Push);

	STATETREE_LOG(VeryVerbose, TEXT("%hs: Starting State Tree %s on owner '%s'."),
		__FUNCTION__, *GetFullNameSafe(&RootStateTree), *GetNameSafe(&Owner));

	// From this point any calls to Stop should be deferred.
	SetUpdatePhaseInExecutionState(Exec, EStateTreeUpdatePhase::StartTree);

	// Start evaluators and global tasks. Fail the execution if any global task fails.
	FStateTreeIndex16 LastInitializedTaskIndex;
	const EStateTreeRunStatus GlobalTasksRunStatus = StartEvaluatorsAndGlobalTasks(LastInitializedTaskIndex);
	if (GlobalTasksRunStatus == EStateTreeRunStatus::Running)
	{
		// First tick.
		// Tasks are not ticked here, since their behavior is that EnterState() (called above) is treated as a tick.
		//@todo Check the result of TickEvaluatorsAndGlobalTasks and early exit if it is not running
		constexpr bool bTickGlobalTasks = false;
		TickEvaluatorsAndGlobalTasks(0.0f, bTickGlobalTasks);

		// Initialize to unset running state.
		Exec.TreeRunStatus = EStateTreeRunStatus::Running;
		Exec.LastTickStatus = EStateTreeRunStatus::Unset;

		static const FStateTreeStateHandle RootState = FStateTreeStateHandle::Root;

		FStateSelectionResult StateSelectionResult;
		if (SelectState(InitFrame, RootState, StateSelectionResult))
		{
			check(StateSelectionResult.ContainsFrames());
			if (StateSelectionResult.GetSelectedFrames().Last().ActiveStates.Last().IsCompletionState())
			{
				// Transition to a terminal state (succeeded/failed).
				STATETREE_LOG(Warning, TEXT("%hs: Tree %s at StateTree start on '%s' using StateTree '%s'."),
					__FUNCTION__, StateSelectionResult.GetSelectedFrames().Last().ActiveStates.Last() == FStateTreeStateHandle::Succeeded ? TEXT("succeeded") : TEXT("failed"), *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
				Exec.TreeRunStatus = StateSelectionResult.GetSelectedFrames().Last().ActiveStates.Last().ToCompletionStatus();
			}
			else
			{
				// Enter state tasks can fail/succeed, treat it same as tick.
				FStateTreeTransitionResult Transition;
				Transition.TargetState = RootState;
				Transition.CurrentRunStatus = Exec.LastTickStatus;
				Transition.NextActiveFrames = StateSelectionResult.GetSelectedFrames(); // Enter state will update Exec.ActiveFrames.
				Transition.NextActiveFrameEvents = StateSelectionResult.GetFramesStateSelectionEvents();
				const EStateTreeRunStatus LastTickStatus = EnterState(Transition);

				Exec.LastTickStatus = LastTickStatus;

				// Report state completed immediately.
				if (Exec.LastTickStatus != EStateTreeRunStatus::Running)
				{
					StateCompleted();
				}
			}
		}

		InstanceData.ResetTemporaryInstances();

		if (Exec.LastTickStatus == EStateTreeRunStatus::Unset)
		{
			// Should not happen. This may happen if initial state could not be selected.
			STATETREE_LOG(Error, TEXT("%hs: Failed to select initial state on '%s' using StateTree '%s'. This should not happen, check that the StateTree logic can always select a state at start."),
				__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
			Exec.TreeRunStatus = EStateTreeRunStatus::Failed;
		}
	}
	else
	{
		StopEvaluatorsAndGlobalTasks(GlobalTasksRunStatus, LastInitializedTaskIndex);

		STATETREE_LOG(VeryVerbose, TEXT("%hs: Global tasks completed the StateTree %s on start in status '%s'."),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree), *UEnum::GetDisplayValueAsText(GlobalTasksRunStatus).ToString());

		// No active states or global tasks anymore, reset frames.
		Exec.ActiveFrames.Reset();
		
		RemoveAllDelegateListeners();

		// We are not considered as running yet so we only set the status without requiring a stop.
		Exec.TreeRunStatus = GlobalTasksRunStatus;
	}

	// Reset phase since we are now safe to stop and before potentially stopping the instance.
	SetUpdatePhaseInExecutionState(Exec, EStateTreeUpdatePhase::Unset);

	// Use local for resulting run state since Stop will reset the instance data.
	EStateTreeRunStatus Result = Exec.TreeRunStatus;
	
	if (Exec.RequestedStop != EStateTreeRunStatus::Unset)
	{
		STATETREE_LOG(VeryVerbose, TEXT("Processing Deferred Stop"));
		UE_STATETREE_DEBUG_LOG_EVENT(this, Log, TEXT("Processing Deferred Stop"));
		Result = Stop(Exec.RequestedStop);
	}
	
	return Result;
}

EStateTreeRunStatus FStateTreeExecutionContext::Stop(EStateTreeRunStatus CompletionStatus)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Stop);

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return EStateTreeRunStatus::Failed;
	}

	if (!CollectActiveExternalData())
	{
		STATETREE_LOG(Warning, TEXT("%hs: Failed to collect external data ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return EStateTreeRunStatus::Failed;
	}

	TGuardValue<bool> ScheduledNextTickScope(bAllowedToScheduleNextTick, false);

	// Make sure that we return a valid completion status (i.e. Succeeded, Failed or Stopped)
	if (CompletionStatus == EStateTreeRunStatus::Unset
		|| CompletionStatus == EStateTreeRunStatus::Running)
	{
		CompletionStatus = EStateTreeRunStatus::Stopped;
	}

	FStateTreeExecutionState& Exec = GetExecState();

	// A reentrant call to Stop or a call from Start or Tick must be deferred.
	if (Exec.CurrentPhase != EStateTreeUpdatePhase::Unset)
	{
		STATETREE_LOG(VeryVerbose, TEXT("Deferring Stop at end of %s"), *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString());
		UE_STATETREE_DEBUG_LOG_EVENT(this, Log, TEXT("Deferring Stop at end of %s"), *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString());

		Exec.RequestedStop = CompletionStatus;
		return EStateTreeRunStatus::Running;
	}

	// No need to clear on exit since we reset all the instance data before leaving the function.
	SetUpdatePhaseInExecutionState(Exec, EStateTreeUpdatePhase::StopTree);

	EStateTreeRunStatus Result = Exec.TreeRunStatus;
	
	// Exit states if still in some valid state.
	if (Exec.TreeRunStatus == EStateTreeRunStatus::Running)
	{
		// Transition to Succeeded state.
		FStateTreeTransitionResult Transition;
		Transition.TargetState = FStateTreeStateHandle::FromCompletionStatus(CompletionStatus);
		Transition.CurrentRunStatus = CompletionStatus;	
		ExitState(Transition);

		// No active states or global tasks anymore, reset frames.
		Exec.ActiveFrames.Reset();

		Result = CompletionStatus;
	}

	// Trace before resetting the instance data since it is required to provide all the event information
	UE_STATETREE_DEBUG_ACTIVE_STATES_EVENT(this, {});
	UE_STATETREE_DEBUG_EXIT_PHASE(this, EStateTreeUpdatePhase::StopTree);
	UE_STATETREE_DEBUG_INSTANCE_EVENT(this, EStateTreeTraceEventType::Pop);

	// Destruct all allocated instance data (does not shrink the buffer). This will invalidate Exec too.
	InstanceData.Reset();

	// External data needs to be recollected if this exec context is reused.
	bActiveExternalDataCollected = false;

	return Result;
}

EStateTreeRunStatus FStateTreeExecutionContext::TickPrelude()
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return EStateTreeRunStatus::Failed;
	}

	if (!CollectActiveExternalData())
	{
		STATETREE_LOG(Warning, TEXT("%hs: Failed to collect external data ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeExecutionState& Exec = GetExecState();

	// No ticking if the tree is done or stopped.
	if (Exec.TreeRunStatus != EStateTreeRunStatus::Running)
	{
		return Exec.TreeRunStatus;
	}

	if (!ensureMsgf(Exec.CurrentPhase == EStateTreeUpdatePhase::Unset, TEXT("%hs can't be called while already in %s ('%s' using StateTree '%s')."),
			__FUNCTION__, *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString(), *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree)))
	{
		return EStateTreeRunStatus::Failed;
	}

	// From this point any calls to Stop should be deferred.
	SetUpdatePhaseInExecutionState(Exec, EStateTreeUpdatePhase::TickStateTree);

	return EStateTreeRunStatus::Running;
}


EStateTreeRunStatus FStateTreeExecutionContext::TickPostlude()
{
	FStateTreeExecutionState& Exec = GetExecState();

	// Reset phase since we are now safe to stop.
	SetUpdatePhaseInExecutionState(Exec, EStateTreeUpdatePhase::Unset);

	// Use local for resulting run state since Stop will reset the instance data.
	EStateTreeRunStatus Result = Exec.TreeRunStatus;
	
	if (Exec.RequestedStop != EStateTreeRunStatus::Unset)
	{
		STATETREE_LOG(VeryVerbose, TEXT("Processing Deferred Stop"));
		UE_STATETREE_DEBUG_LOG_EVENT(this, Log, TEXT("Processing Deferred Stop"));

		Result = Stop(Exec.RequestedStop);
	}

	return Result;
}

EStateTreeRunStatus FStateTreeExecutionContext::Tick(const float DeltaTime)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Tick);

	TGuardValue<bool> ScheduledNextTickScope(bAllowedToScheduleNextTick, false);

	const EStateTreeRunStatus PreludeResult = TickPrelude();
	if (PreludeResult != EStateTreeRunStatus::Running)
	{
		return PreludeResult;
	}

	TickUpdateTasksInternal(DeltaTime);
	TickTriggerTransitionsInternal();

	return TickPostlude();
}

EStateTreeRunStatus FStateTreeExecutionContext::TickUpdateTasks(const float DeltaTime)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Tick);

	TGuardValue<bool> ScheduledNextTickScope(bAllowedToScheduleNextTick, false);

	const EStateTreeRunStatus PreludeResult = TickPrelude();
	if (PreludeResult != EStateTreeRunStatus::Running)
	{
		return PreludeResult;
	}

	TickUpdateTasksInternal(DeltaTime);
	
	return TickPostlude();
}
	
EStateTreeRunStatus FStateTreeExecutionContext::TickTriggerTransitions()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Tick);

	TGuardValue<bool> ScheduledNextTickScope(bAllowedToScheduleNextTick, false);

	const EStateTreeRunStatus PreludeResult = TickPrelude();
	if (PreludeResult != EStateTreeRunStatus::Running)
	{
		return PreludeResult;
	}

	TickTriggerTransitionsInternal();

	return TickPostlude();
}

void FStateTreeExecutionContext::TickUpdateTasksInternal(float DeltaTime)
{
	FStateTreeExecutionState& Exec = GetExecState();

	// If stop is requested, do not try to tick tasks.
	if (Exec.RequestedStop != EStateTreeRunStatus::Unset)
	{
		return;
	}
	
	// Prevent wrong user input.
	DeltaTime = FMath::Max(0.f, DeltaTime);

	// Update the delayed transitions.
	for (FStateTreeTransitionDelayedState& DelayedState : Exec.DelayedTransitions)
	{
		DelayedState.TimeLeft -= DeltaTime;
	}

	const EStateTreeRunStatus PreviousTickStatus = Exec.LastTickStatus;
	auto LogRequestStop = [&Exec, this]()
		{
			if (Exec.RequestedStop != EStateTreeRunStatus::Unset) // -V547
			{
				UE_STATETREE_DEBUG_LOG_EVENT(this, Log, TEXT("Global tasks completed (%s), stopping the tree"), *UEnum::GetDisplayValueAsText(Exec.RequestedStop).ToString());
				STATETREE_LOG(Log, TEXT("Global tasks completed (%s), stopping the tree"), *UEnum::GetDisplayValueAsText(Exec.RequestedStop).ToString());
			}
		};
	auto TickTaskLogic = [&Exec, &LogRequestStop, PreviousTickStatus, this](float DeltaTime)
		{
			// Tick tasks on active states.
			Exec.LastTickStatus = TickTasks(DeltaTime);
			// Report state completed immediately (and no global task completes)
			if (Exec.LastTickStatus != EStateTreeRunStatus::Running && Exec.RequestedStop == EStateTreeRunStatus::Unset && PreviousTickStatus == EStateTreeRunStatus::Running)
			{
				StateCompleted();
			}

			LogRequestStop();
		};

	if (UE::StateTree::ExecutionContext::Private::bTickGlobalNodesFollowingTreeHierarchy)
	{
		TickTaskLogic(DeltaTime);
	}
	else
	{
		// Tick global evaluators and tasks.
		const bool bTickGlobalTasks = true;
		const EStateTreeRunStatus EvalAndGlobalTaskStatus = TickEvaluatorsAndGlobalTasks(DeltaTime, bTickGlobalTasks);
		if (EvalAndGlobalTaskStatus == EStateTreeRunStatus::Running)
		{
			if (Exec.LastTickStatus == EStateTreeRunStatus::Running)
			{
				TickTaskLogic(DeltaTime);
			}
		}
		else
		{
			using namespace UE::StateTree;
			if (ExecutionContext::Private::bGlobalTasksCompleteOwningFrame)
			{
				// Only set RequestStop if it's the first frame (root)
				check(Exec.ActiveFrames.Num() > 0);
				const UStateTree* StateTree = Exec.ActiveFrames[0].StateTree;
				check(StateTree == &RootStateTree);
				const ETaskCompletionStatus GlobalTaskStatus = Exec.ActiveFrames[0].ActiveTasksStatus.GetStatus(StateTree).GetCompletionStatus();
				const EStateTreeRunStatus GlobalRunStatus = ExecutionContext::CastToRunStatus(GlobalTaskStatus);
				if (GlobalRunStatus != EStateTreeRunStatus::Running)
				{
					// Note. Exec.RequestedStop default value is Unset.
					Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, GlobalRunStatus);
					LogRequestStop();
				}
			}
			else
			{
				// Any completion stops the tree execution.
				Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, EvalAndGlobalTaskStatus);
				LogRequestStop();
			}
		}
	}
}

void FStateTreeExecutionContext::TickTriggerTransitionsInternal()
{
	UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::TickTransitions);

	FStateTreeExecutionState& Exec = GetExecState();

	// If stop is requested, do not try to trigger transitions.
	if (Exec.RequestedStop != EStateTreeRunStatus::Unset)
	{
		return;
	}

	// Reset the completed subframe counter (for unit-test that do not recreate an execution context between each tick)
	TriggerTransitionsFromFrameIndex.Reset();

	// The state selection is repeated up to MaxIteration time. This allows failed EnterState() to potentially find a new state immediately.
	// This helps event driven StateTrees to not require another event/tick to find a suitable state.
	static constexpr int32 MaxIterations = 5;
	for (int32 Iter = 0; Iter < MaxIterations; Iter++)
	{
		ON_SCOPE_EXIT{ InstanceData.ResetTemporaryInstances(); };

		// Trigger conditional transitions or state succeed/failed transitions. First tick transition is handled here too.
		if (TriggerTransitions())
		{
			UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::ApplyTransitions);
			UE_STATETREE_DEBUG_TRANSITION_EVENT(this, NextTransitionSource, EStateTreeTraceEventType::OnTransition);
			NextTransitionSource.Reset();

			ExitState(NextTransition);

			// Tree succeeded or failed.
			if (NextTransition.TargetState.IsCompletionState())
			{
				// Transition to a terminal state (succeeded/failed), or default transition failed.
				Exec.TreeRunStatus = NextTransition.TargetState.ToCompletionStatus();

				// Stop evaluators and global tasks.
				StopEvaluatorsAndGlobalTasks(Exec.TreeRunStatus);

				// No active states or global tasks anymore, reset frames.
				Exec.ActiveFrames.Reset();

				RemoveAllDelegateListeners();

				break;
			}

			// Enter state tasks can fail/succeed, treat it same as tick.
			const EStateTreeRunStatus LastTickStatus = EnterState(NextTransition);

			NextTransition = FStateTreeTransitionResult();

			Exec.LastTickStatus = LastTickStatus;

			// Report state completed immediately.
			if (Exec.LastTickStatus != EStateTreeRunStatus::Running)
			{
				StateCompleted();
			}
		}

		// Stop as soon as have found a running state.
		if (Exec.LastTickStatus == EStateTreeRunStatus::Running)
		{
			break;
		}
	}
}

void FStateTreeExecutionContext::BroadcastDelegate(const FStateTreeDelegateDispatcher& Dispatcher)
{
	if (!Dispatcher.IsValid())
	{
		return;
	}

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return;
	}

	const FStateTreeExecutionFrame* CurrentFrame = GetCurrentlyProcessedFrame();
	check(CurrentFrame);

	GetExecState().DelegateActiveListeners.BroadcastDelegate(Dispatcher, GetExecState());
	if (UE::StateTree::ExecutionContext::MarkDelegateAsBroadcasted(Dispatcher, *CurrentFrame, GetMutableInstanceData()->GetMutableStorage()))
	{
		ScheduleNextTick();
	}
}

// Deprecated
bool FStateTreeExecutionContext::AddDelegateListener(const FStateTreeDelegateListener& Listener, FSimpleDelegate Delegate)
{
	BindDelegate(Listener, MoveTemp(Delegate));
	return true;
}

void FStateTreeExecutionContext::BindDelegate(const FStateTreeDelegateListener& Listener, FSimpleDelegate Delegate)
{
	if (!Listener.IsValid())
	{
		// The listener is not bound to a dispatcher. It will never trigger the delegate.
		return;
	}

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return;
	}

	const FStateTreeExecutionFrame* CurrentFrame = GetCurrentlyProcessedFrame();
	if (CurrentFrame == nullptr)
	{
		return;
	}

	const UE::StateTree::FActiveStateID CurrentlyProcessedStateID = CurrentFrame->ActiveStates.FindStateID(CurrentlyProcessedState);
	GetExecState().DelegateActiveListeners.Add(Listener, MoveTemp(Delegate), CurrentFrame->FrameID, CurrentlyProcessedStateID, FStateTreeIndex16(CurrentNodeIndex));
}

// Deprecated
void FStateTreeExecutionContext::RemoveDelegateListener(const FStateTreeDelegateListener& Listener)
{
	UnbindDelegate(Listener);
}

void FStateTreeExecutionContext::UnbindDelegate(const FStateTreeDelegateListener& Listener)
{
	if (!Listener.IsValid())
	{
		return;
	}

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return;
	}

	GetExecState().DelegateActiveListeners.Remove(Listener);
}

void FStateTreeExecutionContext::RequestTransition(const FStateTreeTransitionRequest& Request)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_RequestTransition);

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return;
	}

	FStateTreeExecutionState& Exec = GetExecState();

	if (bAllowDirectTransitions)
	{
		checkf(CurrentlyProcessedFrame, TEXT("Expecting CurrentlyProcessedFrame to be valid when called during TriggerTransitions()."));
		
		STATETREE_LOG(Verbose, TEXT("Request transition to '%s' at priority %s"), *GetSafeStateName(*CurrentlyProcessedFrame, Request.TargetState), *UEnum::GetDisplayValueAsText(Request.Priority).ToString());

		if (RequestTransition(*CurrentlyProcessedFrame, Request.TargetState, Request.Priority, /*TransitionEvent*/nullptr, Request.Fallback))
		{
			NextTransitionSource = FStateTreeTransitionSource(CurrentlyProcessedFrame->StateTree, EStateTreeTransitionSourceType::ExternalRequest, Request.TargetState, Request.Priority);
		}
	}
	else
	{
		const FStateTreeExecutionFrame* RootFrame = &Exec.ActiveFrames[0];
		if (CurrentlyProcessedFrame)
		{
			RootFrame = CurrentlyProcessedFrame;
		}

		if (!RootFrame)
		{
			STATETREE_LOG(Warning, TEXT("%hs: RequestTransition called on %s using StateTree %s without active state. Start() must be called before requesting transition."),
				__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
			return;
		}
		
		STATETREE_LOG(Verbose, TEXT("Request transition to '%s' at priority %s"), *GetSafeStateName(*RootFrame, Request.TargetState), *UEnum::GetDisplayValueAsText(Request.Priority).ToString());

		FStateTreeTransitionRequest RequestWithSource = Request;
		RequestWithSource.SourceFrameID = RootFrame->FrameID;
		const int32 ActiveStateIndex = RootFrame->ActiveStates.IndexOfReverse(CurrentlyProcessedState);
		RequestWithSource.SourceStateID = ActiveStateIndex != INDEX_NONE ? RootFrame->ActiveStates.StateIDs[ActiveStateIndex] : UE::StateTree::FActiveStateID::Invalid;

		InstanceData.AddTransitionRequest(&Owner, RequestWithSource);
	}

	ScheduleNextTick();
}

void FStateTreeExecutionContext::RequestTransition(FStateTreeStateHandle InTargetState, EStateTreeTransitionPriority InPriority, EStateTreeSelectionFallback InFallback)
{
	RequestTransition(FStateTreeTransitionRequest(InTargetState, InPriority, InFallback));
}

void FStateTreeExecutionContext::FinishTask(const FStateTreeTaskBase& Task, EStateTreeFinishTaskType FinishType)
{
	using namespace UE::StateTree;

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return;
	}

	// Like GetInstanceData, only accept task if we are currently processing.
	if (!ensure(CurrentNode == &Task))
	{
		return;
	}
	check(CurrentlyProcessedFrame);
	check(CurrentNodeIndex >= 0);

	FStateTreeExecutionState& Exec = GetExecState();

	const UStateTree* CurrentStateTree = CurrentlyProcessedFrame->StateTree;
	const ETaskCompletionStatus TaskStatus = ExecutionContext::CastToTaskStatus(FinishType);

	if (CurrentlyProcessedState.IsValid())
	{
		check(CurrentStateTree->States.IsValidIndex(CurrentlyProcessedState.Index));
		const FCompactStateTreeState& State = CurrentStateTree->States[CurrentlyProcessedState.Index];

		const int32 ActiveStateIndex = CurrentlyProcessedFrame->ActiveStates.IndexOfReverse(CurrentlyProcessedState);
		check(ActiveStateIndex != INDEX_NONE);
		
		const int32 StateTaskIndex = CurrentNodeIndex - State.TasksBegin;
		check(StateTaskIndex >= 0);

		FTasksCompletionStatus StateTasksStatus = const_cast<FStateTreeExecutionFrame*>(CurrentlyProcessedFrame)->ActiveTasksStatus.GetStatus(State);
		StateTasksStatus.SetStatusWithPriority(StateTaskIndex, TaskStatus);
		Exec.bHasPendingCompletedState = Exec.bHasPendingCompletedState || StateTasksStatus.IsCompleted();
	}
	else
	{
		// global frame
		const int32 FrameTaskIndex = CurrentNodeIndex - CurrentStateTree->GlobalTasksBegin;
		check(FrameTaskIndex >= 0);
		FTasksCompletionStatus GlobalTasksStatus = const_cast<FStateTreeExecutionFrame*>(CurrentlyProcessedFrame)->ActiveTasksStatus.GetStatus(CurrentStateTree);
		GlobalTasksStatus.SetStatusWithPriority(FrameTaskIndex, TaskStatus);
		Exec.bHasPendingCompletedState = Exec.bHasPendingCompletedState || GlobalTasksStatus.IsCompleted();
	}
}

// Deprecated
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FStateTreeExecutionContext::FinishTask(const UE::StateTree::FFinishedTask& Task, EStateTreeFinishTaskType FinishType)
{
	FStateTreeExecutionState& Exec = GetExecState();
	FStateTreeExecutionFrame* Frame = Exec.FindActiveFrame(Task.FrameID);
	if (Frame == nullptr)
	{
		return;
	}

	using namespace UE::StateTree;

	const UE::StateTree::ETaskCompletionStatus Status = ExecutionContext::CastToTaskStatus(Task.RunStatus);
	if (Task.Reason == FFinishedTask::EReasonType::GlobalTask)
	{
		if (Frame->bIsGlobalFrame)
		{
			Frame->ActiveTasksStatus.GetStatus(Frame->StateTree).SetStatusWithPriority(Task.TaskIndex.AsInt32(), Status);
		}
	}
	else
	{
		const int32 FoundIndex = Frame->ActiveStates.IndexOfReverse(Task.StateID);
		if (FoundIndex != INDEX_NONE)
		{
			const FStateTreeStateHandle StateHandle = Frame->ActiveStates[FoundIndex];
			const FCompactStateTreeState* State = Frame->StateTree->GetStateFromHandle(StateHandle);
			if (State != nullptr)
			{
				if (Task.Reason == FFinishedTask::EReasonType::InternalTransition)
				{
					Frame->ActiveTasksStatus.GetStatus(*State).SetCompletionStatus(Status);
				}
				else
				{
					check(Task.Reason == FFinishedTask::EReasonType::StateTask);
					Frame->ActiveTasksStatus.GetStatus(*State).SetStatusWithPriority(Task.TaskIndex.AsInt32(), Status);
				}
			}
		}
	}
}

// Deprecated
bool FStateTreeExecutionContext::IsFinishedTaskValid(const UE::StateTree::FFinishedTask& Task) const
{
	return false;
}

// Deprecated
void FStateTreeExecutionContext::UpdateCompletedStateList()
{
}

// Deprecated
void FStateTreeExecutionContext::MarkStateCompleted(UE::StateTree::FFinishedTask& NewFinishedTask)
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FStateTreeExecutionContext::UpdateInstanceData(TConstArrayView<FStateTreeExecutionFrame> CurrentActiveFrames, TArrayView<FStateTreeExecutionFrame> NextActiveFrames)
{
	// Estimate how many new instance data items we might have.
	int32 EstimatedNumStructs = 0;
	for (int32 FrameIndex = 0; FrameIndex < NextActiveFrames.Num(); FrameIndex++)
	{
		const FStateTreeExecutionFrame& NextFrame = NextActiveFrames[FrameIndex];
		if (NextFrame.bIsGlobalFrame)
		{
			EstimatedNumStructs += NextFrame.StateTree->NumGlobalInstanceData;
		}
		// States
		for (int32 StateIndex = 0; StateIndex < NextFrame.ActiveStates.Num(); StateIndex++)
		{
			const FStateTreeStateHandle StateHandle = NextFrame.ActiveStates[StateIndex];
			const FCompactStateTreeState& State = NextFrame.StateTree->States[StateHandle.Index];
			EstimatedNumStructs += State.InstanceDataNum;
		}
	}
	
	TArray<FConstStructView, FNonconcurrentLinearArrayAllocator> InstanceStructs;
	InstanceStructs.Reserve(EstimatedNumStructs);

	TArray<FInstancedStruct*, FNonconcurrentLinearArrayAllocator> TempInstanceStructs;
	TempInstanceStructs.Reserve(EstimatedNumStructs);

	TArray<FCompactStateTreeParameters, TFixedAllocator<FStateSelectionResult::MaxExecutionFrames>> TempParams;

	TArrayView<FStateTreeTemporaryInstanceData> TempInstances = Storage.GetMutableTemporaryInstances();
	auto FindInstanceTempData = [&TempInstances](const FStateTreeExecutionFrame& Frame, FStateTreeDataHandle DataHandle)
	{
		FStateTreeTemporaryInstanceData* TempData = TempInstances.FindByPredicate([&Frame, &DataHandle](const FStateTreeTemporaryInstanceData& Data)
		{
			return Data.FrameID == Frame.FrameID && Data.DataHandle == DataHandle;
		});
		return TempData ? &TempData->Instance : nullptr;
	};
	
	// Find next instance data sources and find common/existing section of instance data at start.
	int32 CurrentGlobalInstanceIndexBase = 0;
	int32 NumCommonInstanceData = 0;

	const UStruct* NextStateParameterDataStruct = nullptr;
	FStateTreeDataHandle NextStateParameterDataHandle = FStateTreeDataHandle::Invalid;
	
	FStateTreeDataHandle CurrentGlobalParameterDataHandle = FStateTreeDataHandle(EStateTreeDataSourceType::GlobalParameterData);

	bool bAreCommon = true;
	for (int32 FrameIndex = 0; FrameIndex < NextActiveFrames.Num(); FrameIndex++)
	{
		const bool bIsCurrentFrameValid = CurrentActiveFrames.IsValidIndex(FrameIndex)
						&& CurrentActiveFrames[FrameIndex].IsSameFrame(NextActiveFrames[FrameIndex]);

		bAreCommon &= bIsCurrentFrameValid;

		const FStateTreeExecutionFrame* CurrentFrame = bIsCurrentFrameValid ? &CurrentActiveFrames[FrameIndex] : nullptr;
		FStateTreeExecutionFrame& NextFrame = NextActiveFrames[FrameIndex];

		check(NextFrame.StateTree);

		if (NextFrame.bIsGlobalFrame)
		{
			// Handle global tree parameters
			if (NextStateParameterDataHandle.IsValid())
			{
				// Point to the parameter block set by linked state.
				check(NextStateParameterDataStruct == NextFrame.StateTree->GetDefaultParameters().GetPropertyBagStruct());
				CurrentGlobalParameterDataHandle = NextStateParameterDataHandle;
				NextStateParameterDataHandle = FStateTreeDataHandle::Invalid; // Mark as used.
			}
			
			// Global Evals
			const int32 BaseIndex = InstanceStructs.Num();
			CurrentGlobalInstanceIndexBase = BaseIndex;
			
			InstanceStructs.AddDefaulted(NextFrame.StateTree->NumGlobalInstanceData);
			TempInstanceStructs.AddZeroed(NextFrame.StateTree->NumGlobalInstanceData);
			
			for (int32 EvalIndex = NextFrame.StateTree->EvaluatorsBegin; EvalIndex < (NextFrame.StateTree->EvaluatorsBegin + NextFrame.StateTree->EvaluatorsNum); EvalIndex++)
			{
				const FStateTreeEvaluatorBase& Eval =  NextFrame.StateTree->Nodes[EvalIndex].Get<const FStateTreeEvaluatorBase>();
				const FConstStructView EvalInstanceData = NextFrame.StateTree->DefaultInstanceData.GetStruct(Eval.InstanceTemplateIndex.Get());
				InstanceStructs[BaseIndex + Eval.InstanceDataHandle.GetIndex()] = EvalInstanceData;
				if (!bAreCommon)
				{
					TempInstanceStructs[BaseIndex + Eval.InstanceDataHandle.GetIndex()] = FindInstanceTempData(NextFrame, Eval.InstanceDataHandle);
				}
			}

			// Global tasks
			for (int32 TaskIndex = NextFrame.StateTree->GlobalTasksBegin; TaskIndex < (NextFrame.StateTree->GlobalTasksBegin + NextFrame.StateTree->GlobalTasksNum); TaskIndex++)
			{
				const FStateTreeTaskBase& Task =  NextFrame.StateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
				const FConstStructView TaskInstanceData = NextFrame.StateTree->DefaultInstanceData.GetStruct(Task.InstanceTemplateIndex.Get());
				InstanceStructs[BaseIndex + Task.InstanceDataHandle.GetIndex()] = TaskInstanceData;
				if (!bAreCommon)
				{
					TempInstanceStructs[BaseIndex + Task.InstanceDataHandle.GetIndex()] = FindInstanceTempData(NextFrame, Task.InstanceDataHandle);
				}
			}

			if (bAreCommon)
			{
				NumCommonInstanceData = InstanceStructs.Num();
			}
		}

		// States
		const int32 BaseIndex = InstanceStructs.Num();

		NextFrame.GlobalParameterDataHandle = CurrentGlobalParameterDataHandle;
		NextFrame.GlobalInstanceIndexBase = FStateTreeIndex16(CurrentGlobalInstanceIndexBase);
		NextFrame.ActiveInstanceIndexBase = FStateTreeIndex16(BaseIndex);

		for (int32 StateIndex = 0; StateIndex < NextFrame.ActiveStates.Num(); StateIndex++)
		{
			// Check if the next state is still same as current state, GetStateSafe() will return invalid state if passed out of bounds index.
			bAreCommon = bAreCommon && (CurrentFrame && CurrentFrame->ActiveStates.GetStateSafe(StateIndex) == NextFrame.ActiveStates[StateIndex]);

			const FStateTreeStateHandle StateHandle = NextFrame.ActiveStates[StateIndex];
			const FCompactStateTreeState& State = NextFrame.StateTree->States[StateHandle.Index];

			InstanceStructs.AddDefaulted(State.InstanceDataNum);
			TempInstanceStructs.AddZeroed(State.InstanceDataNum);

			bool bCanHaveTempData = false;
			
			if (State.Type == EStateTreeStateType::Subtree)
			{
				check(State.ParameterDataHandle.IsValid());
				check(State.ParameterTemplateIndex.IsValid());
				const FConstStructView ParamsInstanceData = NextFrame.StateTree->DefaultInstanceData.GetStruct(State.ParameterTemplateIndex.Get());
				if (!NextStateParameterDataHandle.IsValid())
				{
					// Parameters are not set by a linked state, create instance data.
					InstanceStructs[BaseIndex + State.ParameterDataHandle.GetIndex()] = ParamsInstanceData;
					NextFrame.StateParameterDataHandle = State.ParameterDataHandle;
					bCanHaveTempData = true;
				}
				else
				{
					// Point to the parameter block set by linked state.
					const FCompactStateTreeParameters* Params = ParamsInstanceData.GetPtr<const FCompactStateTreeParameters>();
					const UStruct* StateParameterDataStruct = Params ? Params->Parameters.GetPropertyBagStruct() : nullptr;
					check(NextStateParameterDataStruct == StateParameterDataStruct);
					
					NextFrame.StateParameterDataHandle = NextStateParameterDataHandle;
					NextStateParameterDataHandle = FStateTreeDataHandle::Invalid; // Mark as used.

					// This state will not instantiate parameter data, so we don't care about the temp data either.
					bCanHaveTempData = false;
				}
			}
			else
			{
				if (State.ParameterTemplateIndex.IsValid())
				{
					// Linked state's instance data is the parameters.
					check(State.ParameterDataHandle.IsValid());

					const FCompactStateTreeParameters* Params = nullptr;
					if (FInstancedStruct* TempParamsInstanceData = FindInstanceTempData(NextFrame, State.ParameterDataHandle))
					{
						// If we have temp data for the parameters, then setup the instance data with just a type, so that we can steal the temp data below (TempInstanceStructs).
						// We expect overridden linked assets to hit this code path. 
						InstanceStructs[BaseIndex + State.ParameterDataHandle.GetIndex()] = FConstStructView(TempParamsInstanceData->GetScriptStruct());
						Params = TempParamsInstanceData->GetPtr<const FCompactStateTreeParameters>();
						bCanHaveTempData = true;
					}
					else
					{
						// If not temp data, use the states or linked assets default values.
						FConstStructView ParamsInstanceData;
						if (State.Type == EStateTreeStateType::LinkedAsset)
						{
							// This state is a container for the linked state tree.Its instance data matches the linked state tree parameters.The linked state tree asset is the next frame.
							const bool bIsLastFrame = FrameIndex == NextActiveFrames.Num() - 1;
							if (!bIsLastFrame)
							{
								FStateTreeExecutionFrame& FollowingNextFrame = NextActiveFrames[FrameIndex + 1];
								ParamsInstanceData = FConstStructView::Make(TempParams.Emplace_GetRef(FollowingNextFrame.StateTree->GetDefaultParameters()));
							}
						}
						if (!ParamsInstanceData.IsValid())
						{
							ParamsInstanceData = NextFrame.StateTree->DefaultInstanceData.GetStruct(State.ParameterTemplateIndex.Get());
						}
						InstanceStructs[BaseIndex + State.ParameterDataHandle.GetIndex()] = ParamsInstanceData;
						Params = ParamsInstanceData.GetPtr<const FCompactStateTreeParameters>();
						bCanHaveTempData = true;
					}

					if (State.Type == EStateTreeStateType::Linked
						|| State.Type == EStateTreeStateType::LinkedAsset)
					{
						// Store the index of the parameter data, so that we can point the linked state to it.
						check(State.ParameterDataHandle.GetSource() == EStateTreeDataSourceType::StateParameterData);
						checkf(!NextStateParameterDataHandle.IsValid(), TEXT("NextStateParameterDataIndex not should be set yet when we encounter a linked state."));
						NextStateParameterDataHandle = State.ParameterDataHandle;
						NextStateParameterDataStruct = Params ? Params->Parameters.GetPropertyBagStruct() : nullptr;
					}
				}
			}
			
			if (!bAreCommon && bCanHaveTempData)
			{
				TempInstanceStructs[BaseIndex + State.ParameterDataHandle.GetIndex()] = FindInstanceTempData(NextFrame, State.ParameterDataHandle);
			}

			if (State.EventDataIndex.IsValid())
			{
				InstanceStructs[BaseIndex + State.EventDataIndex.Get()] = FConstStructView(FStateTreeSharedEvent::StaticStruct());
			}

			for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
			{
				const FStateTreeTaskBase& Task = NextFrame.StateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
				const FConstStructView TaskInstanceData = NextFrame.StateTree->DefaultInstanceData.GetStruct(Task.InstanceTemplateIndex.Get());
				InstanceStructs[BaseIndex + Task.InstanceDataHandle.GetIndex()] = TaskInstanceData;
				if (!bAreCommon)
				{
					TempInstanceStructs[BaseIndex + Task.InstanceDataHandle.GetIndex()] = FindInstanceTempData(NextFrame, Task.InstanceDataHandle);
				}
			}

			if (bAreCommon)
			{
				NumCommonInstanceData = InstanceStructs.Num();
			}
		}
	}
	
	// Common section should match.
#if WITH_STATETREE_DEBUG
	for (int32 Index = 0; Index < NumCommonInstanceData; Index++)
	{
		check(Index < InstanceData.Num());

		FConstStructView ExistingInstanceDataView = InstanceData.GetStruct(Index);
		FConstStructView NewInstanceDataView = InstanceStructs[Index]; 

		check(NewInstanceDataView.GetScriptStruct() == ExistingInstanceDataView.GetScriptStruct());

		const FStateTreeInstanceObjectWrapper* ExistingWrapper = ExistingInstanceDataView.GetPtr<const FStateTreeInstanceObjectWrapper>();
		const FStateTreeInstanceObjectWrapper* NewWrapper = ExistingInstanceDataView.GetPtr<const FStateTreeInstanceObjectWrapper>();
		if (ExistingWrapper && NewWrapper)
		{
			check(ExistingWrapper->InstanceObject && NewWrapper->InstanceObject);
			check(ExistingWrapper->InstanceObject->GetClass() == NewWrapper->InstanceObject->GetClass());
		}
	}
#endif

	// Remove instance data that was not common.
	InstanceData.ShrinkTo(NumCommonInstanceData);

	// Add new instance data.
	InstanceData.Append(Owner,
		MakeArrayView(InstanceStructs.GetData() + NumCommonInstanceData, InstanceStructs.Num() - NumCommonInstanceData),
		MakeArrayView(TempInstanceStructs.GetData() + NumCommonInstanceData, TempInstanceStructs.Num() - NumCommonInstanceData));

	InstanceData.ResetTemporaryInstances();
}

FStateTreeDataView FStateTreeExecutionContext::GetDataView(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle)
{
	switch (Handle.GetSource())
	{
	case EStateTreeDataSourceType::ContextData:
		check(!ContextAndExternalDataViews.IsEmpty())
		return ContextAndExternalDataViews[Handle.GetIndex()];

	case EStateTreeDataSourceType::ExternalData:
		check(!ContextAndExternalDataViews.IsEmpty())
		return ContextAndExternalDataViews[CurrentFrame.ExternalDataBaseIndex.Get() + Handle.GetIndex()];

	case EStateTreeDataSourceType::TransitionEvent:
		{
			if (CurrentlyProcessedTransitionEvent)
			{
				// const_cast because events are read only, but we cannot express that in FStateTreeDataView.
				return FStateTreeDataView(FStructView::Make(*const_cast<FStateTreeEvent*>(CurrentlyProcessedTransitionEvent)));
			}

			return nullptr;
		}

	case EStateTreeDataSourceType::StateEvent:
		{
			// If state selection is going, return FStateTreeEvent of the event currently captured by the state selection.
			if (CurrentlyProcessedStateSelectionEvents)
			{
				if (const FCompactStateTreeState* State = CurrentFrame.StateTree->GetStateFromHandle(Handle.GetState()))
				{
					// Events are read only, but we cannot express that in FStateTreeDataView.
					if (FStateTreeEvent* Event = CurrentlyProcessedStateSelectionEvents->Events[State->Depth].GetMutable())
					{
						return FStateTreeDataView(FStructView::Make(*Event));
					}
				}

				return {};
			}

			return UE::StateTree::InstanceData::GetDataView(Storage, CurrentlyProcessedSharedInstanceStorage, ParentFrame, CurrentFrame, Handle);
		}

	case EStateTreeDataSourceType::ExternalGlobalParameterData:
		{
			checkf(false, TEXT("External global parameter data currently not supported for linked state-trees"));
			break;
		}

	default:
		return UE::StateTree::InstanceData::GetDataView(Storage, CurrentlyProcessedSharedInstanceStorage, ParentFrame, CurrentFrame, Handle);
	}

	return {};
}

FStateTreeDataView FStateTreeExecutionContext::GetDataView(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo)
{	
	const FStateTreeDataHandle Handle = CopyInfo.SourceDataHandle.Get<FStateTreeDataHandle>();
	if (Handle.GetSource() == EStateTreeDataSourceType::ExternalGlobalParameterData)
	{
		return GetDataViewOrTemporary(ParentFrame, CurrentFrame, CopyInfo);
	}

	return GetDataView(ParentFrame, CurrentFrame, Handle);
}

EStateTreeRunStatus FStateTreeExecutionContext::ForceTransition(const FRecordedStateTreeTransitionResult& Transition)
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%hs: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return EStateTreeRunStatus::Failed;
	}

	// A reentrant call to ForceTransition or a call from Start, Tick or Stop must be deferred.
	if (GetExecState().CurrentPhase != EStateTreeUpdatePhase::Unset)
	{
		UE_STATETREE_DEBUG_LOG_EVENT(this, Warning, TEXT("Can't force a transition while %s"), *UEnum::GetDisplayValueAsText(GetExecState().CurrentPhase).ToString());
		return EStateTreeRunStatus::Unset;
	}

	TOptional<FStateTreeTransitionResult> TransitionResult = MakeTransitionResult(Transition);
	if (!TransitionResult.IsSet())
	{
		return EStateTreeRunStatus::Unset;
	}

	ExitState(TransitionResult.GetValue());
	return EnterState(TransitionResult.GetValue());
}

const FStateTreeExecutionFrame* FStateTreeExecutionContext::FindFrame(const UStateTree* StateTree, FStateTreeStateHandle RootState, TConstArrayView<FStateTreeExecutionFrame> Frames, const FStateTreeExecutionFrame*& OutParentFrame)
{
	const int32 FrameIndex = Frames.IndexOfByPredicate([&StateTree, RootState](const FStateTreeExecutionFrame& Frame)
	{
		return Frame.StateTree == StateTree && Frame.RootState == RootState;
	});

	if (FrameIndex == INDEX_NONE)
	{
		OutParentFrame = nullptr;
		return nullptr;
	}

	if (FrameIndex > 0)
	{
		OutParentFrame = &Frames[FrameIndex - 1];
	}

	return &Frames[FrameIndex];
}

bool FStateTreeExecutionContext::IsHandleSourceValid(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle) const
{
	switch (Handle.GetSource())
	{
	case EStateTreeDataSourceType::None:
		return true;

	case EStateTreeDataSourceType::ContextData:
		return true;

	case EStateTreeDataSourceType::ExternalData:
		return CurrentFrame.ExternalDataBaseIndex.IsValid()
			&& ContextAndExternalDataViews.IsValidIndex(CurrentFrame.ExternalDataBaseIndex.Get() + Handle.GetIndex());

	case EStateTreeDataSourceType::TransitionEvent:
		return CurrentlyProcessedTransitionEvent != nullptr;

	case EStateTreeDataSourceType::StateEvent:
		return CurrentlyProcessedStateSelectionEvents != nullptr
			|| (CurrentFrame.ActiveInstanceIndexBase.IsValid()
			&& CurrentFrame.ActiveStates.Contains(Handle.GetState())
			&& Storage.IsValidIndex(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex()));

	case EStateTreeDataSourceType::ExternalGlobalParameterData:
	{
		checkf(false, TEXT("External global parameter data currently not supported for linked state-trees"));
		break;
	}

	default:
		return UE::StateTree::InstanceData::Internal::IsHandleSourceValid(Storage, ParentFrame, CurrentFrame, Handle);
	}

	return false;
}

bool FStateTreeExecutionContext::IsHandleSourceValid(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo) const
{
	const FStateTreeDataHandle Handle = CopyInfo.SourceDataHandle.Get<FStateTreeDataHandle>();
	if (Handle.GetSource() == EStateTreeDataSourceType::ExternalGlobalParameterData)
	{
		return ExternalGlobalParameters ? ExternalGlobalParameters->Find(CopyInfo) != nullptr : false;
	}

	return IsHandleSourceValid(ParentFrame, CurrentFrame, Handle);
}

FStateTreeDataView FStateTreeExecutionContext::GetDataViewOrTemporary(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle)
{
	if (IsHandleSourceValid(ParentFrame, CurrentFrame, Handle))
	{
		return GetDataView(ParentFrame, CurrentFrame, Handle);
	}
	
	return GetTemporaryDataView(ParentFrame, CurrentFrame, Handle);
}

FStateTreeDataView FStateTreeExecutionContext::GetDataViewOrTemporary(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo)
{
	const FStateTreeDataHandle Handle = CopyInfo.SourceDataHandle.Get<FStateTreeDataHandle>();
	if (Handle.GetSource() == EStateTreeDataSourceType::ExternalGlobalParameterData)
	{
		uint8* MemoryPtr = ExternalGlobalParameters->Find(CopyInfo);
		return FStateTreeDataView(CopyInfo.SourceStructType, MemoryPtr);	
	}

	return GetDataViewOrTemporary(ParentFrame, CurrentFrame, Handle);	
}

FStateTreeDataView FStateTreeExecutionContext::GetTemporaryDataView(const FStateTreeExecutionFrame* ParentFrame,
	const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle)
{
	switch (Handle.GetSource())
	{
	case EStateTreeDataSourceType::ExternalGlobalParameterData:
		checkf(false, TEXT("External global parameter data currently not supported for linked state-trees"));
		return {};

	default:
		return UE::StateTree::InstanceData::Internal::GetTemporaryDataView(Storage, ParentFrame, CurrentFrame, Handle);
	}

}

FStateTreeDataView FStateTreeExecutionContext::AddTemporaryInstance(const FStateTreeExecutionFrame& Frame, const FStateTreeIndex16 OwnerNodeIndex, const FStateTreeDataHandle DataHandle, FConstStructView NewInstanceData)
{
	const FStructView NewInstance = Storage.AddTemporaryInstance(Owner, Frame, OwnerNodeIndex, DataHandle, NewInstanceData);
	if (FStateTreeInstanceObjectWrapper* Wrapper = NewInstance.GetPtr<FStateTreeInstanceObjectWrapper>())
	{
		return FStateTreeDataView(Wrapper->InstanceObject);
	}
	return NewInstance;
}

bool FStateTreeExecutionContext::CopyBatchOnActiveInstances(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataView TargetView, const FStateTreeIndex16 BindingsBatch)
{
	const FPropertyBindingCopyInfoBatch& Batch = CurrentFrame.StateTree->PropertyBindings.Super::GetBatch(BindingsBatch);
	check(TargetView.GetStruct() == Batch.TargetStruct.Get().Struct);

	if (Batch.PropertyFunctionsBegin != Batch.PropertyFunctionsEnd)
	{
		check(Batch.PropertyFunctionsBegin.IsValid() && Batch.PropertyFunctionsEnd.IsValid());
		EvaluatePropertyFunctionsOnActiveInstances(ParentFrame, CurrentFrame, FStateTreeIndex16(Batch.PropertyFunctionsBegin), Batch.PropertyFunctionsEnd.Get() - Batch.PropertyFunctionsBegin.Get());
	}

	bool bSucceed = true;
	for (const FPropertyBindingCopyInfo& Copy : CurrentFrame.StateTree->PropertyBindings.Super::GetBatchCopies(Batch))
	{
		const FStateTreeDataView SourceView = GetDataView(ParentFrame, CurrentFrame, Copy);
		bSucceed &= CurrentFrame.StateTree->PropertyBindings.Super::CopyProperty(Copy, SourceView, TargetView);
	}
	return bSucceed;
}

bool FStateTreeExecutionContext::CopyBatchWithValidation(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataView TargetView, const FStateTreeIndex16 BindingsBatch)
{
	const FPropertyBindingCopyInfoBatch& Batch = CurrentFrame.StateTree->PropertyBindings.Super::GetBatch(BindingsBatch);
	check(TargetView.GetStruct() == Batch.TargetStruct.Get().Struct);

	if (Batch.PropertyFunctionsBegin != Batch.PropertyFunctionsEnd)
	{
		check(Batch.PropertyFunctionsBegin.IsValid() && Batch.PropertyFunctionsEnd.IsValid());
		EvaluatePropertyFunctionsWithValidation(ParentFrame, CurrentFrame, FStateTreeIndex16(Batch.PropertyFunctionsBegin), Batch.PropertyFunctionsEnd.Get() - Batch.PropertyFunctionsBegin.Get());
	}

	bool bSucceed = true;
	for (const FPropertyBindingCopyInfo& Copy : CurrentFrame.StateTree->PropertyBindings.Super::GetBatchCopies(Batch))
	{
		const FStateTreeDataView SourceView = GetDataViewOrTemporary(ParentFrame, CurrentFrame, Copy);
		if (!SourceView.IsValid())
		{
			bSucceed = false;
			break;
		}
		
		bSucceed &= CurrentFrame.StateTree->PropertyBindings.Super::CopyProperty(Copy, SourceView, TargetView);
	}
	return bSucceed;
}


bool FStateTreeExecutionContext::CollectActiveExternalData()
{
	if (bActiveExternalDataCollected)
	{
		return true;
	}

	bool bAllExternalDataValid = true;
	FStateTreeExecutionState& Exec = GetExecState();
	const FStateTreeExecutionFrame* PrevFrame = nullptr;
	
	for (FStateTreeExecutionFrame& Frame : Exec.ActiveFrames)
	{
		if (PrevFrame && PrevFrame->StateTree == Frame.StateTree)
		{
			Frame.ExternalDataBaseIndex = PrevFrame->ExternalDataBaseIndex;
		}
		else
		{
			Frame.ExternalDataBaseIndex = CollectExternalData(Frame.StateTree);
		}

		if (!Frame.ExternalDataBaseIndex.IsValid())
		{
			bAllExternalDataValid = false;
		}
		
		PrevFrame = &Frame;
	}

	if (bAllExternalDataValid)
	{
		bActiveExternalDataCollected = true;
	}
	
	return bAllExternalDataValid;
}

FStateTreeIndex16 FStateTreeExecutionContext::CollectExternalData(const UStateTree* StateTree)
{
	if (!StateTree)
	{
		return FStateTreeIndex16::Invalid;
	}

	// If one of the active states share the same state tree, get the external data from there.
	for (const FCollectedExternalDataCache& Cache : CollectedExternalCache)
	{
		if (Cache.StateTree == StateTree)
		{
			return Cache.BaseIndex;
		}
	}
	
	const TConstArrayView<FStateTreeExternalDataDesc> ExternalDataDescs = StateTree->GetExternalDataDescs();
	const int32 BaseIndex = ContextAndExternalDataViews.Num();
	const int32 NumDescs = ExternalDataDescs.Num();
	FStateTreeIndex16 Result(BaseIndex);

	if (NumDescs > 0)
	{
		ContextAndExternalDataViews.AddDefaulted(NumDescs);
		const TArrayView<FStateTreeDataView> DataViews = MakeArrayView(ContextAndExternalDataViews.GetData() + BaseIndex, NumDescs);  

		if (ensureMsgf(CollectExternalDataDelegate.IsBound(), TEXT("The StateTree asset has external data, expecting CollectExternalData delegate to be provided.")))
		{
			if (!CollectExternalDataDelegate.Execute(*this, StateTree, StateTree->GetExternalDataDescs(), DataViews))
			{
				// The caller is responsible for error reporting. 
				return FStateTreeIndex16::Invalid;
			}
		}

		// Check that the data is valid and present.
		for (int32 Index = 0; Index < NumDescs; Index++)
		{
			const FStateTreeExternalDataDesc& DataDesc = ExternalDataDescs[Index];
			const FStateTreeDataView& DataView = ContextAndExternalDataViews[BaseIndex + Index];

			if (DataDesc.Requirement == EStateTreeExternalDataRequirement::Required)
			{
				// Required items must have valid pointer of the expected type.  
				if (!DataView.IsValid() || !DataDesc.IsCompatibleWith(DataView))
				{
					Result = FStateTreeIndex16::Invalid;
					break;
				}
			}
			else
			{
				// Optional items must have same type if they are set.
				if (DataView.IsValid() && !DataDesc.IsCompatibleWith(DataView))
				{
					Result = FStateTreeIndex16::Invalid;
					break;
				}
			}
		}
	}

	if (!Result.IsValid())
	{
		// Rollback
		ContextAndExternalDataViews.SetNum(BaseIndex);
	}

	// Cached both succeeded and failed attempts.
	CollectedExternalCache.Add({ StateTree, Result });

	return FStateTreeIndex16(Result);
}

bool FStateTreeExecutionContext::SetGlobalParameters(const FInstancedPropertyBag& Parameters)
{
	if (ensureMsgf(RootStateTree.GetDefaultParameters().GetPropertyBagStruct() == Parameters.GetPropertyBagStruct(),
		TEXT("Parameters must be of the same struct type. Make sure to migrate the provided parameters to the same type as the StateTree default parameters.")))
	{
		Storage.SetGlobalParameters(Parameters);
		return true;
	}

	return false;
}

void FStateTreeExecutionContext::CaptureNewStateEvents(TConstArrayView<FStateTreeExecutionFrame> PrevFrames, TConstArrayView<FStateTreeExecutionFrame> NewFrames, TArrayView<FStateTreeFrameStateSelectionEvents> FramesStateSelectionEvents)
{
	// Mark the events from delayed transitions as in use, so that each State will receive unique copy of the event struct. 
	TArray<FStateTreeSharedEvent, TInlineAllocator<16>> EventsInUse;
	for (const FStateTreeTransitionDelayedState& DelayedTransition : GetExecState().DelayedTransitions)
	{
		if (DelayedTransition.CapturedEvent.IsValid())
		{
			EventsInUse.Add(DelayedTransition.CapturedEvent);
		}
	}

	for (int32 FrameIndex = 0; FrameIndex < NewFrames.Num(); ++FrameIndex)
	{
		const FStateTreeExecutionFrame& NewFrame = NewFrames[FrameIndex];

		// Find states that are unique to the new frame.
		TConstArrayView<FStateTreeStateHandle> UniqueStates = NewFrame.ActiveStates;
		if (PrevFrames.IsValidIndex(FrameIndex))
		{
			const FStateTreeExecutionFrame& PrevFrame = PrevFrames[FrameIndex];

			if (PrevFrame.FrameID == NewFrame.FrameID)
			{
			checkf(PrevFrame.RootState == NewFrame.RootState && PrevFrame.StateTree == NewFrame.StateTree, TEXT("If the Id matches, then the root and the tree must also match."));

			for (int32 StateIndex = 0; StateIndex < NewFrame.ActiveStates.Num(); ++StateIndex)
			{
				if (!PrevFrame.ActiveStates.IsValidIndex(StateIndex) || PrevFrame.ActiveStates.StateIDs[StateIndex] != NewFrame.ActiveStates.StateIDs[StateIndex])
				{
					UniqueStates = TConstArrayView<FStateTreeStateHandle>(&NewFrame.ActiveStates[StateIndex], NewFrame.ActiveStates.Num() - StateIndex);
					break;
				}
			}
		}
		}

		// Capture events for the new states.
		for (const FStateTreeStateHandle StateHandle : UniqueStates)
		{
			if (const FCompactStateTreeState* State = NewFrame.StateTree->GetStateFromHandle(StateHandle))
			{
				if (State->EventDataIndex.IsValid())
				{
					FStateTreeSharedEvent& StateTreeEvent = Storage.GetMutableStruct(NewFrame.ActiveInstanceIndexBase.Get() + State->EventDataIndex.Get()).Get<FStateTreeSharedEvent>();
					
					const FStateTreeSharedEvent& EventToCapture = FramesStateSelectionEvents[FrameIndex].Events[State->Depth];
					if (EventsInUse.Contains(EventToCapture))
					{
						// Event is already spoken for, make a copy.
						StateTreeEvent = FStateTreeSharedEvent(*EventToCapture);
					}
					else
					{
						// Event not in use, steal it.
						StateTreeEvent = FramesStateSelectionEvents[FrameIndex].Events[State->Depth];
						EventsInUse.Add(EventToCapture);
					}
				}
			}
		}
	}
}

EStateTreeRunStatus FStateTreeExecutionContext::EnterState(FStateTreeTransitionResult& Transition)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_EnterState);

	if (Transition.NextActiveFrames.IsEmpty())
	{
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeExecutionState& Exec = GetExecState();

	if (bRecordTransitions)
	{
		RecordedTransitions.Add(MakeRecordedTransitionResult(Transition));
	}

	// Allocate new tasks.
	UpdateInstanceData(Exec.ActiveFrames, Transition.NextActiveFrames);

	CaptureNewStateEvents(Exec.ActiveFrames, Transition.NextActiveFrames, Transition.NextActiveFrameEvents);

	Exec.StateChangeCount++;
	Exec.EnterStateFailedFrameIndex = FStateTreeIndex16::Invalid; // This will make all tasks to be accepted.
	Exec.EnterStateFailedTaskIndex = FStateTreeIndex16::Invalid; // This will make all tasks to be accepted.
	
	// On target branch means that the state is the target of current transition or child of it.
	// States which were active before and will remain active, but are not on target branch will not get
	// EnterState called. That is, a transition is handled as "replan from this state".
	bool bOnTargetBranch = false;
	FStateTreeTransitionResult CurrentTransition = Transition;
	EStateTreeRunStatus Result = EStateTreeRunStatus::Running;

	STATETREE_LOG(Log, TEXT("Enter state '%s' (%d)"), *DebugGetStatePath(Transition.NextActiveFrames), Exec.StateChangeCount);
	UE_STATETREE_DEBUG_ENTER_PHASE(this, EStateTreeUpdatePhase::EnterStates);

	// The previous active frames are needed for state enter logic.
	TArray<FStateTreeExecutionFrame, FNonconcurrentLinearArrayAllocator> PreviousActiveFrames;
	PreviousActiveFrames = Exec.ActiveFrames;

	// Reset the current active frames, new ones are added one by one.
	Exec.ActiveFrames.Reset();

	// Track any changed state (i.e., not sustained) to prevent reused subtrees from being considered as sustained states since the whole
	// tree is reused and should be considered as a new instance.
	bool bAnyParentStateChanged = false;

	for (int32 FrameIndex = 0; FrameIndex < Transition.NextActiveFrames.Num() && Result != EStateTreeRunStatus::Failed; FrameIndex++)
	{
		const FStateTreeExecutionFrame& NextFrame = Transition.NextActiveFrames[FrameIndex];
		
		FStateTreeExecutionFrame* CurrentParentFrame = !Exec.ActiveFrames.IsEmpty() ? &Exec.ActiveFrames.Last() : nullptr;
		FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames.Add_GetRef(NextFrame);
		const UStateTree* CurrentStateTree = NextFrame.StateTree;
		const UE::StateTree::FActiveFrameID CurrentFrameID = NextFrame.FrameID;
		
		// We'll add new states one by one, so that active states contain only the states which have EnterState called.
		CurrentFrame.ActiveStates.Reset();

		if (!ensureMsgf(CurrentFrame.ActiveTasksStatus.IsValid(), TEXT("Frame is not formed correct.")))
		{
			// Create the status. We lost the previous tasks' completion status.
			const FCompactStateTreeFrame* FrameInfo = NextFrame.StateTree->GetFrameFromHandle(NextFrame.RootState);
			ensureAlwaysMsgf(FrameInfo, TEXT("The compiled data is invalid. It should contains the information for the root frame."));
			CurrentFrame.ActiveTasksStatus = FrameInfo ? FStateTreeTasksCompletionStatus(*FrameInfo) : FStateTreeTasksCompletionStatus();
		}

		// Get previous active states, they are used to calculate transition type.
		FStateTreeActiveStates PreviousActiveStates;
		if (PreviousActiveFrames.IsValidIndex(FrameIndex)
			&& PreviousActiveFrames[FrameIndex].IsSameFrame(NextFrame))
		{
			PreviousActiveStates = PreviousActiveFrames[FrameIndex].ActiveStates;
		}

		FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);

		for (int32 StateIndex = 0; StateIndex < NextFrame.ActiveStates.Num() && Result != EStateTreeRunStatus::Failed; StateIndex++)
		{
			const FStateTreeStateHandle CurrentHandle = NextFrame.ActiveStates[StateIndex];
			const UE::StateTree::FActiveStateID CurrentStateID = NextFrame.ActiveStates.StateIDs[StateIndex];
			const FStateTreeStateHandle PreviousHandle = PreviousActiveStates.GetStateSafe(StateIndex);
			const FCompactStateTreeState& State = CurrentStateTree->States[CurrentHandle.Index];

			// Add only enabled States to the list of active States
			if (State.bEnabled && !CurrentFrame.ActiveStates.Push(CurrentHandle, NextFrame.ActiveStates.StateIDs[StateIndex]))
			{
				STATETREE_LOG(Error, TEXT("%hs: Reached max execution depth when trying to enter state '%s'.  '%s' using StateTree '%s'."),
					__FUNCTION__, *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
				break;
			}
			//@todo push the same state as previously handle
			CurrentFrame.ActiveTasksStatus.Push(State);

			UE::StateTree::FTasksCompletionStatus CurrentStateTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(State);
			CurrentFrame.NumCurrentlyActiveStates = static_cast<uint8>(CurrentFrame.ActiveStates.Num());

			FCurrentlyProcessedStateScope StateScope(*this, CurrentHandle);

			if (State.Type == EStateTreeStateType::Linked
				|| State.Type == EStateTreeStateType::LinkedAsset)
			{
				if (State.ParameterDataHandle.IsValid()
					&& State.ParameterBindingsBatch.IsValid())
				{
					const FStateTreeDataView StateParamsDataView = GetDataView(CurrentParentFrame, CurrentFrame, State.ParameterDataHandle);
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, StateParamsDataView, State.ParameterBindingsBatch);
				}
			}

			if (CurrentFrame.FrameID == Transition.SourceFrameID
				&& CurrentHandle == Transition.TargetState)
			{
				bOnTargetBranch = true;
			}

			const bool bWasActive = (PreviousHandle == CurrentHandle) && !bAnyParentStateChanged;

			const EStateTreeStateChangeType ChangeType = bWasActive ? EStateTreeStateChangeType::Sustained : EStateTreeStateChangeType::Changed;
			if (ChangeType == EStateTreeStateChangeType::Changed)
			{
				bAnyParentStateChanged = true;
			}

			CurrentTransition.CurrentState = CurrentHandle;
			CurrentTransition.ChangeType = ChangeType;

			// Do not enter a disabled State tasks but maintain property bindings
			const bool bIsEnteringState = (!bWasActive || bOnTargetBranch) && State.bEnabled;

			if (bIsEnteringState)
			{
				UE_STATETREE_DEBUG_STATE_EVENT(this, CurrentHandle, EStateTreeTraceEventType::OnEntering);
				STATETREE_LOG(Log, TEXT("%*sState '%s' (%s)"), (FrameIndex + StateIndex + 1)*UE::StateTree::Debug::IndentSize, TEXT("")
					, *GetSafeStateName(NextFrame, CurrentHandle)
					, *UEnum::GetDisplayValueAsText(CurrentTransition.ChangeType).ToString());
			}

			// Call state change events on conditions if needed.
			if (bIsEnteringState && State.bHasStateChangeConditions)
			{
				for (int32 ConditionIndex = State.EnterConditionsBegin; ConditionIndex < (State.EnterConditionsBegin + State.EnterConditionsNum); ConditionIndex++)
				{
					const FStateTreeConditionBase& Cond = CurrentFrame.StateTree->Nodes[ConditionIndex].Get<const FStateTreeConditionBase>();
					if (Cond.bHasShouldCallStateChangeEvents)
					{
						const bool bShouldCallEnterState = CurrentTransition.ChangeType == EStateTreeStateChangeType::Changed
															|| (CurrentTransition.ChangeType == EStateTreeStateChangeType::Sustained && Cond.bShouldStateChangeOnReselect);

						if (bShouldCallEnterState)
						{
							const FStateTreeDataView ConditionInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Cond.InstanceDataHandle);
							FNodeInstanceDataScope DataScope(*this, &Cond, ConditionIndex, Cond.InstanceDataHandle, ConditionInstanceView);

							if (Cond.BindingsBatch.IsValid())
							{
								// Use validated copy, since we test in situations where the sources are not always valid (e.g. enter conditions may try to access inactive parent state).
								CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, ConditionInstanceView, Cond.BindingsBatch);
							}

							UE_STATETREE_DEBUG_CONDITION_ENTER_STATE(this, CurrentFrame.StateTree, FStateTreeIndex16(ConditionIndex));
							Cond.EnterState(*this, Transition);

							// Reset copied properties that might contain object references.
							if (Cond.BindingsBatch.IsValid())
							{
								CurrentFrame.StateTree->PropertyBindings.Super::ResetObjects(Cond.BindingsBatch, ConditionInstanceView);
							}
						}
					}
				}
			}

			// Activate tasks on current state.
			for (int32 StateTaskIndex = 0; StateTaskIndex < State.TasksNum; ++StateTaskIndex)
			{
				const int32 AssetTaskIndex = State.TasksBegin + StateTaskIndex;
				const FStateTreeTaskBase& Task = NextFrame.StateTree->Nodes[AssetTaskIndex].Get<const FStateTreeTaskBase>();
				const FStateTreeDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);

				FNodeInstanceDataScope DataScope(*this, &Task, AssetTaskIndex, Task.InstanceDataHandle, TaskInstanceView);

				// Copy bound properties.
				if (Task.BindingsBatch.IsValid())
				{
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskInstanceView, Task.BindingsBatch);
				}

				// Ignore disabled task
				if (Task.bTaskEnabled == false)
				{
					STATETREE_LOG(VeryVerbose, TEXT("%*sSkipped 'EnterState' for disabled Task: '%s'"), UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
					continue;
				}

				const bool bShouldCallEnterState = CurrentTransition.ChangeType == EStateTreeStateChangeType::Changed
													|| (CurrentTransition.ChangeType == EStateTreeStateChangeType::Sustained && Task.bShouldStateChangeOnReselect);

				if (bIsEnteringState && bShouldCallEnterState)
				{
					STATETREE_LOG(Verbose, TEXT("%*sTask '%s'.EnterState()"), (FrameIndex + StateIndex + 1)*UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
					UE_STATETREE_DEBUG_TASK_ENTER_STATE(this, NextFrame.StateTree, FStateTreeIndex16(AssetTaskIndex));

					EStateTreeRunStatus TaskRunStatus = EStateTreeRunStatus::Unset;
					{
						QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_EnterState);
						CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Task_EnterState);
					
						TaskRunStatus = Task.EnterState(*this, CurrentTransition);
					}

					UE::StateTree::ETaskCompletionStatus TaskStatus = UE::StateTree::ExecutionContext::CastToTaskStatus(TaskRunStatus);
					TaskStatus = CurrentStateTasksStatus.SetStatusWithPriority(StateTaskIndex, TaskStatus);

					TaskRunStatus = UE::StateTree::ExecutionContext::CastToRunStatus(TaskStatus);
					UE_STATETREE_DEBUG_TASK_EVENT(this, AssetTaskIndex, TaskInstanceView, EStateTreeTraceEventType::OnEntered, TaskRunStatus);

					Result = UE::StateTree::ExecutionContext::GetPriorityRunStatus(Result, TaskRunStatus);
					if (TaskRunStatus == EStateTreeRunStatus::Failed && CurrentStateTasksStatus.IsConsideredForCompletion(StateTaskIndex))
					{
						Exec.EnterStateFailedFrameIndex = FStateTreeIndex16(FrameIndex);
						Exec.EnterStateFailedTaskIndex = FStateTreeIndex16(AssetTaskIndex);
						break;
					}
				}
			}

			if (bIsEnteringState)
			{
				UE_STATETREE_DEBUG_STATE_EVENT(this, CurrentHandle, EStateTreeTraceEventType::OnEntered);
			}
		}
	}

	UE_STATETREE_DEBUG_EXIT_PHASE(this, EStateTreeUpdatePhase::EnterStates);

	UE_STATETREE_DEBUG_ACTIVE_STATES_EVENT(this, Exec.ActiveFrames);

	Exec.bHasPendingCompletedState = Result != EStateTreeRunStatus::Running;
	return Result;
}

void FStateTreeExecutionContext::ExitState(const FStateTreeTransitionResult& Transition)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_ExitState);

	check(!GetExecState().LastExitedNodeIndex.IsValid());
	ON_SCOPE_EXIT { GetExecState().LastExitedNodeIndex = FStateTreeIndex16::Invalid; };

	FStateTreeExecutionState& Exec = GetExecState();

	if (Exec.ActiveFrames.IsEmpty())
	{
		return;
	}

	// On target branch means that the state is the target of current transition or child of it.
	// States which were active before and will remain active, but are not on target branch will not get
	// EnterState called. That is, a transition is handled as "replan from this state".
	bool bOnTargetBranch = false;

	struct FExitStateCall
	{
		FExitStateCall() = default;
		FExitStateCall(const EStateTreeStateChangeType InChangeType, const bool bInShouldCall)
			: ChangeType(InChangeType)
			, bShouldCall(bInShouldCall)
		{
		}

		EStateTreeStateChangeType ChangeType = EStateTreeStateChangeType::None;
		bool bShouldCall = false; 
	};

	TArray<FExitStateCall, FNonconcurrentLinearArrayAllocator> ExitStateCalls;

	// Track any changed state (i.e., not sustained) to prevent reused subtrees from being considered as sustained states since the whole
	// tree is reused and should be considered as a new instance.
	bool bAnyParentStateChanged = false;

	int32 FirstModifiedFrame = INDEX_NONE;
	int32 FirstModifiedState = INDEX_NONE;

	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); FrameIndex++)
	{
		FStateTreeExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UStateTree* CurrentStateTree = CurrentFrame.StateTree;

		FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);

		const FStateTreeExecutionFrame* NextFrame = nullptr;
		if (Transition.NextActiveFrames.IsValidIndex(FrameIndex)
			&& Transition.NextActiveFrames[FrameIndex].IsSameFrame(CurrentFrame))
		{
			NextFrame = &Transition.NextActiveFrames[FrameIndex];
		}

		const bool bShouldCallOnEvaluatorsAndGlobalTasks = NextFrame == nullptr && CurrentFrame.bIsGlobalFrame;
		ExitStateCalls.Emplace(EStateTreeStateChangeType::Changed, bShouldCallOnEvaluatorsAndGlobalTasks);
		
		if (bShouldCallOnEvaluatorsAndGlobalTasks)
		{
			for (int32 EvalIndex = CurrentStateTree->EvaluatorsBegin; EvalIndex < (CurrentStateTree->EvaluatorsBegin + CurrentStateTree->EvaluatorsNum); EvalIndex++)
			{
				const FStateTreeEvaluatorBase& Eval = CurrentStateTree->Nodes[EvalIndex].Get<const FStateTreeEvaluatorBase>();
				const FStateTreeDataView EvalInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Eval.InstanceDataHandle);
				FNodeInstanceDataScope DataScope(*this, &Eval, EvalIndex, Eval.InstanceDataHandle, EvalInstanceView);

				if (Eval.BindingsBatch.IsValid())
				{
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, EvalInstanceView, Eval.BindingsBatch);
				}
			}

			for (int32 TaskIndex = CurrentStateTree->GlobalTasksBegin; TaskIndex < (CurrentStateTree->GlobalTasksBegin + CurrentStateTree->GlobalTasksNum); TaskIndex++)
			{
				const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
				const FStateTreeDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
				FNodeInstanceDataScope DataScope(*this, &Task, TaskIndex, Task.InstanceDataHandle, TaskInstanceView);

				if (Task.BindingsBatch.IsValid() && Task.bShouldCopyBoundPropertiesOnExitState)
				{
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskInstanceView, Task.BindingsBatch);
				}
			}
		}

		for (int32 Index = 0; Index < CurrentFrame.ActiveStates.Num(); Index++)
		{
			const FStateTreeStateHandle CurrentHandle = CurrentFrame.ActiveStates[Index];
			const FStateTreeStateHandle NextHandle = NextFrame ? NextFrame->ActiveStates.GetStateSafe(Index) : FStateTreeStateHandle::Invalid;
			const FCompactStateTreeState& State = CurrentStateTree->States[CurrentHandle.Index];

			FCurrentlyProcessedStateScope StateScope(*this, CurrentHandle);

			if (State.Type == EStateTreeStateType::Linked
				|| State.Type == EStateTreeStateType::LinkedAsset)
			{
				if (State.ParameterDataHandle.IsValid()
					&& State.ParameterBindingsBatch.IsValid())
				{
					const FStateTreeDataView StateParamsDataView = GetDataView(CurrentParentFrame, CurrentFrame, State.ParameterDataHandle);
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, StateParamsDataView, State.ParameterBindingsBatch);
				}
			}

			if (CurrentFrame.FrameID == Transition.SourceFrameID
				&& CurrentHandle == Transition.TargetState)
			{
				bOnTargetBranch = true;
			}

			const bool bRemainsActive = (NextHandle == CurrentHandle) && !bAnyParentStateChanged;

			const EStateTreeStateChangeType ChangeType = bRemainsActive ? EStateTreeStateChangeType::Sustained : EStateTreeStateChangeType::Changed;
			if (ChangeType == EStateTreeStateChangeType::Changed)
			{
				bAnyParentStateChanged = true;
			}
			
			// Should call ExitState() on this state.
			// (No need to test for 'State.bEnabled' like EnterState since we can't enter a disabled state)
			const bool bShouldCallExitState = (!bRemainsActive || bOnTargetBranch);
			ExitStateCalls.Emplace(ChangeType, bShouldCallExitState);

			if (bShouldCallExitState && FirstModifiedFrame == INDEX_NONE)
			{
				FirstModifiedFrame = FrameIndex;
				FirstModifiedState = Index;
			}

			// Do property copies, ExitState() is called below.
			for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
			{
				const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
				const FStateTreeDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);

				// Copy bound properties.
				if (Task.BindingsBatch.IsValid() && Task.bShouldCopyBoundPropertiesOnExitState)
				{
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskInstanceView, Task.BindingsBatch);
				}
			}
		}
	}

	// Call in reverse order.
	STATETREE_LOG(Log, TEXT("Exit state '%s' (%d)"), *DebugGetStatePath(Exec.ActiveFrames), Exec.StateChangeCount);
	UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::ExitStates);

	FStateTreeTransitionResult CurrentTransition = Transition;
	int32 CallIndex = ExitStateCalls.Num() - 1;

	for (int32 FrameIndex = Exec.ActiveFrames.Num() - 1; FrameIndex >= 0; FrameIndex--)
	{
		FStateTreeExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UStateTree* CurrentStateTree = CurrentFrame.StateTree;

		FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);

		for (int32 StateIndex = CurrentFrame.ActiveStates.Num() - 1; StateIndex >= 0; StateIndex--)
		{
			const FStateTreeStateHandle CurrentHandle = CurrentFrame.ActiveStates[StateIndex];
			const UE::StateTree::FActiveStateID CurrentStateID = CurrentFrame.ActiveStates.StateIDs[StateIndex];
			const FCompactStateTreeState& State = CurrentStateTree->States[CurrentHandle.Index];

			const FExitStateCall& ExitCall = ExitStateCalls[CallIndex--];
			CurrentTransition.ChangeType = ExitCall.ChangeType;

			STATETREE_LOG(Log, TEXT("%*sState '%s' (%s)"), (FrameIndex + StateIndex + 1)*UE::StateTree::Debug::IndentSize, TEXT("")
				, *GetSafeStateName(CurrentFrame, CurrentHandle)
				, *UEnum::GetDisplayValueAsText(CurrentTransition.ChangeType).ToString());

			UE_STATETREE_DEBUG_STATE_EVENT(this, CurrentHandle, EStateTreeTraceEventType::OnExiting);

			if (ExitCall.bShouldCall)
			{
				FCurrentlyProcessedStateScope StateScope(*this, CurrentHandle);

				// Remove any delayed transitions that belong to this state.
				Exec.DelayedTransitions.RemoveAllSwap(
					[CurrentStateID, Begin = State.TransitionsBegin, End = State.TransitionsBegin + State.TransitionsNum](const FStateTreeTransitionDelayedState& DelayedState)
					{
						return  DelayedState.StateID == CurrentStateID
							&& DelayedState.TransitionIndex.Get() >= Begin
							&& DelayedState.TransitionIndex.Get() < End;
					});

				CurrentTransition.CurrentState = CurrentHandle;

				// Do property copies, ExitState() is called below.
				for (int32 TaskIndex = (State.TasksBegin + State.TasksNum) - 1; TaskIndex >= State.TasksBegin; TaskIndex--)
				{

					// Call task completed only if EnterState() was called.
					// The task order in the tree (BF) allows us to use the comparison.
					// Relying here that invalid value of Exec.EnterStateFailedTaskIndex == MAX_uint16.
					if (TaskIndex <= Exec.EnterStateFailedTaskIndex.Get())
					{
						const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
						const FStateTreeDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);

						FNodeInstanceDataScope DataScope(*this, &Task, TaskIndex, Task.InstanceDataHandle, TaskInstanceView);

						// Ignore disabled task
						if (Task.bTaskEnabled == false)
						{
							STATETREE_LOG(VeryVerbose, TEXT("%*sSkipped 'ExitState' for disabled Task: '%s'"), UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
							continue;
						}

						const bool bShouldCallStateChange = CurrentTransition.ChangeType == EStateTreeStateChangeType::Changed
									|| (CurrentTransition.ChangeType == EStateTreeStateChangeType::Sustained && Task.bShouldStateChangeOnReselect);

						if (bShouldCallStateChange)
						{
							STATETREE_LOG(Verbose, TEXT("%*sTask '%s'.ExitState()"), (FrameIndex + StateIndex + 1)*UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
							UE_STATETREE_DEBUG_TASK_EXIT_STATE(this, CurrentStateTree, FStateTreeIndex16(TaskIndex));
							{
								QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_ExitState);
								CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Task_ExitState);
								Task.ExitState(*this, CurrentTransition);
							}
							UE_STATETREE_DEBUG_TASK_EVENT(this, TaskIndex, TaskInstanceView, EStateTreeTraceEventType::OnExited, Transition.CurrentRunStatus);
							GetExecState().LastExitedNodeIndex = FStateTreeIndex16(TaskIndex);
						}
					}
				}

				// Call state change events on conditions if needed.
				if (State.bHasStateChangeConditions)
				{
					for (int32 ConditionIndex = (State.EnterConditionsBegin + State.EnterConditionsNum) - 1; ConditionIndex >= State.EnterConditionsBegin; ConditionIndex--)
					{
						const FStateTreeConditionBase& Cond = CurrentFrame.StateTree->Nodes[ConditionIndex].Get<const FStateTreeConditionBase>();
						if (Cond.bHasShouldCallStateChangeEvents)
						{
							const bool bShouldCallStateChange = CurrentTransition.ChangeType == EStateTreeStateChangeType::Changed
										|| (CurrentTransition.ChangeType == EStateTreeStateChangeType::Sustained && Cond.bShouldStateChangeOnReselect);

							if (bShouldCallStateChange)
							{
								const FStateTreeDataView ConditionInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Cond.InstanceDataHandle);
								FNodeInstanceDataScope DataScope(*this, &Cond, ConditionIndex, Cond.InstanceDataHandle, ConditionInstanceView);

								if (Cond.BindingsBatch.IsValid())
								{
									// Use validated copy, since we test in situations where the sources are not always valid (e.g. enter conditions may try to access inactive parent state).
									CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, ConditionInstanceView, Cond.BindingsBatch);
								}

								UE_STATETREE_DEBUG_CONDITION_EXIT_STATE(this, CurrentFrame.StateTree, FStateTreeIndex16(ConditionIndex));
								Cond.ExitState(*this, Transition);

								// Reset copied properties that might contain object references.
								if (Cond.BindingsBatch.IsValid())
								{
									CurrentFrame.StateTree->PropertyBindings.Super::ResetObjects(Cond.BindingsBatch, ConditionInstanceView);
								}
							}
						}
					}
				}

				// Delegate Listeners Cleanup
				GetExecState().DelegateActiveListeners.RemoveAll(CurrentFrame.ActiveStates.StateIDs[StateIndex]);
			}

			UE_STATETREE_DEBUG_STATE_EVENT(this, CurrentHandle, EStateTreeTraceEventType::OnExited);
		}

		// Frame exit call
		{
			const FExitStateCall& ExitCall = ExitStateCalls[CallIndex--];
			if (ExitCall.bShouldCall)
			{
				CurrentTransition.ChangeType = ExitCall.ChangeType;
				CallStopOnEvaluatorsAndGlobalTasks(CurrentParentFrame, CurrentFrame, CurrentTransition);

				// Delegate Listeners Cleanup
				GetExecState().DelegateActiveListeners.RemoveAll(CurrentFrame.FrameID);
			}
		}
	}
}

void FStateTreeExecutionContext::RemoveAllDelegateListeners()
{
	GetExecState().DelegateActiveListeners = FStateTreeDelegateActiveListeners();
}

void FStateTreeExecutionContext::StateCompleted()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_StateCompleted);

	const FStateTreeExecutionState& Exec = GetExecState();

	if (Exec.ActiveFrames.IsEmpty())
	{
		return;
	}

	STATETREE_LOG(Verbose, TEXT("State Completed %s (%d)"), *UEnum::GetDisplayValueAsText(Exec.LastTickStatus).ToString(), Exec.StateChangeCount);
	UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::StateCompleted);

	// Call from child towards root to allow to pass results back.
	// Note: Completed is assumed to be called immediately after tick or enter state, so there's no property copying.

	for (int32 FrameIndex = Exec.ActiveFrames.Num() - 1; FrameIndex >= 0; FrameIndex--)
	{
		const FStateTreeExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		const FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UStateTree* CurrentStateTree = CurrentFrame.StateTree;

		FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);

		if (FrameIndex <= Exec.EnterStateFailedFrameIndex.Get())
		{
			for (int32 StateIndex = CurrentFrame.ActiveStates.Num() - 1; StateIndex >= 0; StateIndex--)
			{
				const FStateTreeStateHandle CurrentHandle = CurrentFrame.ActiveStates[StateIndex];
				const FCompactStateTreeState& State = CurrentStateTree->States[CurrentHandle.Index];

				FCurrentlyProcessedStateScope StateScope(*this, CurrentHandle);
				
				STATETREE_LOG(Verbose, TEXT("%*sState '%s'"), (FrameIndex + StateIndex + 1)*UE::StateTree::Debug::IndentSize, TEXT(""), *GetSafeStateName(CurrentFrame, CurrentHandle));
				UE_STATETREE_DEBUG_STATE_EVENT(this, CurrentHandle, EStateTreeTraceEventType::OnStateCompleted);

				// Notify Tasks
				for (int32 TaskIndex = (State.TasksBegin + State.TasksNum) - 1; TaskIndex >= State.TasksBegin; TaskIndex--)
				{
					// Call task completed only if EnterState() was called.
					// The task order in the tree (BF) allows us to use the comparison.
					// Relying here that invalid value of Exec.EnterStateFailedTaskIndex == MAX_uint16.
					if (TaskIndex <= Exec.EnterStateFailedTaskIndex.Get())
					{
						const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
						const FStateTreeDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
						FNodeInstanceDataScope DataScope(*this, &Task, TaskIndex, Task.InstanceDataHandle, TaskInstanceView);

						// Ignore disabled task
						if (Task.bTaskEnabled == false)
						{
							STATETREE_LOG(VeryVerbose, TEXT("%*sSkipped 'StateCompleted' for disabled Task: '%s'"), UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
							continue;
						}

						STATETREE_LOG(Verbose, TEXT("%*sTask '%s'.StateCompleted()"), (FrameIndex + StateIndex + 1)*UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
						Task.StateCompleted(*this, Exec.LastTickStatus, CurrentFrame.ActiveStates);
					}
				}
				
				// Call state change events on conditions if needed.
				if (State.bHasStateChangeConditions)
				{
					for (int32 ConditionIndex = (State.EnterConditionsBegin + State.EnterConditionsNum) - 1; ConditionIndex >= State.EnterConditionsBegin; ConditionIndex--)
					{
						const FStateTreeConditionBase& Cond = CurrentFrame.StateTree->Nodes[ConditionIndex].Get<const FStateTreeConditionBase>();
						if (Cond.bHasShouldCallStateChangeEvents)
						{
							const FStateTreeDataView ConditionInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Cond.InstanceDataHandle);
							FNodeInstanceDataScope DataScope(*this, &Cond, ConditionIndex, Cond.InstanceDataHandle, ConditionInstanceView);

							if (Cond.BindingsBatch.IsValid())
							{
								// Use validated copy, since we test in situations where the sources are not always valid (e.g. enter conditions may try to access inactive parent state).
								CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, ConditionInstanceView, Cond.BindingsBatch);
							}
							
							Cond.StateCompleted(*this, Exec.LastTickStatus, CurrentFrame.ActiveStates);

							// Reset copied properties that might contain object references.
							if (Cond.BindingsBatch.IsValid())
							{
								CurrentFrame.StateTree->PropertyBindings.Super::ResetObjects(Cond.BindingsBatch, ConditionInstanceView);
							}
						}
					}
				}
			}
		}
	}
}

EStateTreeRunStatus FStateTreeExecutionContext::TickEvaluatorsAndGlobalTasks(const float DeltaTime, const bool bTickGlobalTasks)
{
	// When a global task is completed it completes the tree execution.
	// A global task can complete async. See CompletedStates.
	// When a global task fails, stop ticking the following tasks.

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_TickEvaluators);
	UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::TickingGlobalTasks);

	STATETREE_LOG(VeryVerbose, TEXT("Ticking Evaluators & Global Tasks"));

	FStateTreeExecutionState& Exec = GetExecState();

	EStateTreeRunStatus Result = EStateTreeRunStatus::Running;

	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
	{
		FStateTreeExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		if (CurrentFrame.bIsGlobalFrame)
		{
			FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);

			const EStateTreeRunStatus FrameResult = TickEvaluatorsAndGlobalTasksForFrame(DeltaTime, bTickGlobalTasks, FrameIndex, CurrentParentFrame, &CurrentFrame);
			Result = UE::StateTree::ExecutionContext::GetPriorityRunStatus(Result, FrameResult);

			if (Result == EStateTreeRunStatus::Failed)
			{
				break;
			}
		}
	}

	Exec.bHasPendingCompletedState = Exec.bHasPendingCompletedState || Result != EStateTreeRunStatus::Running;
	return Result;
}

EStateTreeRunStatus FStateTreeExecutionContext::TickEvaluatorsAndGlobalTasksForFrame(const float DeltaTime, const bool bTickGlobalTasks, const int32 FrameIndex, const FStateTreeExecutionFrame* CurrentParentFrame, const TNotNull<FStateTreeExecutionFrame*> CurrentFrame)
{
	check(CurrentFrame->bIsGlobalFrame);

	EStateTreeRunStatus Result = EStateTreeRunStatus::Running;

	const UStateTree* CurrentStateTree = CurrentFrame->StateTree;

	// Tick evaluators
	for (int32 EvalIndex = CurrentStateTree->EvaluatorsBegin; EvalIndex < (CurrentStateTree->EvaluatorsBegin + CurrentStateTree->EvaluatorsNum); EvalIndex++)
	{
		const FStateTreeEvaluatorBase& Eval = CurrentStateTree->Nodes[EvalIndex].Get<const FStateTreeEvaluatorBase>();
		const FStateTreeDataView EvalInstanceView = GetDataView(CurrentParentFrame, *CurrentFrame, Eval.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Eval, EvalIndex, Eval.InstanceDataHandle, EvalInstanceView);

		// Copy bound properties.
		if (Eval.BindingsBatch.IsValid())
		{
			CopyBatchOnActiveInstances(CurrentParentFrame, *CurrentFrame, EvalInstanceView, Eval.BindingsBatch);
		}
		STATETREE_LOG(VeryVerbose, TEXT("  Tick: '%s'"), *Eval.Name.ToString());
		UE_STATETREE_DEBUG_EVALUATOR_TICK(this, CurrentStateTree, EvalIndex);
		{
			QUICK_SCOPE_CYCLE_COUNTER(StateTree_Eval_Tick);
			Eval.Tick(*this, DeltaTime);

			UE_STATETREE_DEBUG_EVALUATOR_EVENT(this, EvalIndex, EvalInstanceView, EStateTreeTraceEventType::OnTicked);
		}
	}

	if (bTickGlobalTasks)
	{
		using namespace UE::StateTree;

		FTasksCompletionStatus CurrentGlobalTasksStatus = CurrentFrame->ActiveTasksStatus.GetStatus(CurrentStateTree);
		if (!CurrentGlobalTasksStatus.HasAnyFailed())
		{
			const bool bHasEvents = EventQueue && EventQueue->HasEvents();
			if (ExecutionContext::Private::bCopyBoundPropertiesOnNonTickedTask || CurrentStateTree->ShouldTickGlobalTasks(bHasEvents))
			{
				// Update Tasks data and tick if possible (ie. if no task has yet failed and bShouldTickTasks is true)
				FTickTaskArguments TickArgs;
				TickArgs.DeltaTime = DeltaTime;
				TickArgs.TasksBegin = CurrentStateTree->GlobalTasksBegin;
				TickArgs.TasksNum = CurrentStateTree->GlobalTasksNum;
				TickArgs.Indent = FrameIndex + 1;
				TickArgs.ParentFrame = CurrentParentFrame;
				TickArgs.Frame = CurrentFrame;
				TickArgs.TasksCompletionStatus = &CurrentGlobalTasksStatus;
				TickArgs.bIsGlobalTasks = true;
				TickArgs.bShouldTickTasks = true;
				TickTasks(TickArgs);
			}
		}

		// Completed global task stops the frame execution.
		const ETaskCompletionStatus GlobalTaskStatus = CurrentGlobalTasksStatus.GetCompletionStatus();
		Result = ExecutionContext::CastToRunStatus(GlobalTaskStatus);
	}

	return Result;
}

EStateTreeRunStatus FStateTreeExecutionContext::StartEvaluatorsAndGlobalTasks(FStateTreeIndex16& OutLastInitializedTaskIndex)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_StartEvaluators);
	UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::StartGlobalTasks);

	STATETREE_LOG(Verbose, TEXT("Start Evaluators & Global tasks"));

	FStateTreeExecutionState& Exec = GetExecState();

	OutLastInitializedTaskIndex = FStateTreeIndex16();
	EStateTreeRunStatus Result = EStateTreeRunStatus::Running;

	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); FrameIndex++)
	{
		FStateTreeExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UE::StateTree::FActiveFrameID CurrentFrameID = CurrentFrame.FrameID;
		if (CurrentFrame.bIsGlobalFrame)
		{
			FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
			
			const UStateTree* CurrentStateTree = CurrentFrame.StateTree;
			UE::StateTree::FTasksCompletionStatus CurrentGlobalTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(CurrentStateTree);

			// Start evaluators
			for (int32 EvalIndex = CurrentStateTree->EvaluatorsBegin; EvalIndex < (CurrentStateTree->EvaluatorsBegin + CurrentStateTree->EvaluatorsNum); EvalIndex++)
			{
				const FStateTreeEvaluatorBase& Eval = CurrentStateTree->Nodes[EvalIndex].Get<const FStateTreeEvaluatorBase>();
				const FStateTreeDataView EvalInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Eval.InstanceDataHandle);
				FNodeInstanceDataScope DataScope(*this, &Eval, EvalIndex, Eval.InstanceDataHandle, EvalInstanceView);

				// Copy bound properties.
				if (Eval.BindingsBatch.IsValid())
				{
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, EvalInstanceView, Eval.BindingsBatch);
				}
				STATETREE_LOG(Verbose, TEXT("  Start: '%s'"), *Eval.Name.ToString());
				UE_STATETREE_DEBUG_EVALUATOR_ENTER_TREE(this, CurrentStateTree, FStateTreeIndex16(EvalIndex));
				{
					QUICK_SCOPE_CYCLE_COUNTER(StateTree_Eval_TreeStart);
					Eval.TreeStart(*this);

					UE_STATETREE_DEBUG_EVALUATOR_EVENT(this, EvalIndex, EvalInstanceView, EStateTreeTraceEventType::OnTreeStarted);
				}
			}

			// Start Global tasks
			// Even if we call Enter/ExitState() on global tasks, they do not enter any specific state.
			const FStateTreeTransitionResult Transition = {}; // Empty transition

			for (int32 GlobalTaskIndex = 0; GlobalTaskIndex < CurrentStateTree->GlobalTasksNum; ++GlobalTaskIndex)
			{
				const int32 AssetTaskIndex = CurrentStateTree->GlobalTasksBegin + GlobalTaskIndex;
				const FStateTreeTaskBase& Task =  CurrentStateTree->Nodes[AssetTaskIndex].Get<const FStateTreeTaskBase>();
				const FStateTreeDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
				FNodeInstanceDataScope DataScope(*this, &Task, AssetTaskIndex, Task.InstanceDataHandle, TaskInstanceView);

				// Copy bound properties.
				if (Task.BindingsBatch.IsValid())
				{
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskInstanceView, Task.BindingsBatch);
				}

				// Ignore disabled task
				if (Task.bTaskEnabled == false)
				{
					STATETREE_LOG(VeryVerbose, TEXT("%*sSkipped 'EnterState' for disabled Task: '%s'"), UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
					continue;
				}

				STATETREE_LOG(Verbose, TEXT("  Start: '%s'"), *Task.Name.ToString());
				UE_STATETREE_DEBUG_TASK_ENTER_STATE(this, CurrentStateTree, FStateTreeIndex16(AssetTaskIndex));

				EStateTreeRunStatus TaskRunStatus = EStateTreeRunStatus::Unset;
				{
					QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_TreeStart);

					TaskRunStatus = Task.EnterState(*this, Transition);
				}

				UE::StateTree::ETaskCompletionStatus TaskStatus = UE::StateTree::ExecutionContext::CastToTaskStatus(TaskRunStatus);
				TaskStatus = CurrentGlobalTasksStatus.SetStatusWithPriority(GlobalTaskIndex, TaskStatus);

				TaskRunStatus = UE::StateTree::ExecutionContext::CastToRunStatus(TaskStatus);
				UE_STATETREE_DEBUG_TASK_EVENT(this, AssetTaskIndex, TaskInstanceView, EStateTreeTraceEventType::OnEntered, TaskRunStatus);

				Result = UE::StateTree::ExecutionContext::GetPriorityRunStatus(Result, TaskRunStatus);
				if (TaskRunStatus == EStateTreeRunStatus::Failed && CurrentGlobalTasksStatus.IsConsideredForCompletion(GlobalTaskIndex))
				{
					OutLastInitializedTaskIndex = FStateTreeIndex16(AssetTaskIndex);
				}
			}
		}
	}

	return Result;
}

void FStateTreeExecutionContext::StopEvaluatorsAndGlobalTasks(const EStateTreeRunStatus CompletionStatus, const FStateTreeIndex16 LastInitializedTaskIndex)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_StopEvaluators);
	UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::StopGlobalTasks);

	STATETREE_LOG(Verbose, TEXT("Stop Evaluators & Global Tasks"));

	FStateTreeExecutionState& Exec = GetExecState();

	// Update bindings
	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); FrameIndex++)
	{
		FStateTreeExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		if (CurrentFrame.bIsGlobalFrame)
		{
			FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
			
			const UStateTree* CurrentStateTree = CurrentFrame.StateTree;

			for (int32 EvalIndex = CurrentStateTree->EvaluatorsBegin; EvalIndex < (CurrentStateTree->EvaluatorsBegin + CurrentStateTree->EvaluatorsNum); EvalIndex++)
			{
				const FStateTreeEvaluatorBase& Eval = CurrentStateTree->Nodes[EvalIndex].Get<const FStateTreeEvaluatorBase>();
				const FStateTreeDataView EvalInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Eval.InstanceDataHandle);
				FNodeInstanceDataScope DataScope(*this, &Eval, EvalIndex, Eval.InstanceDataHandle, EvalInstanceView);

				// Copy bound properties.
				if (Eval.BindingsBatch.IsValid())
				{
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, EvalInstanceView, Eval.BindingsBatch);
				}
			}

			for (int32 TaskIndex = CurrentStateTree->GlobalTasksBegin; TaskIndex < (CurrentStateTree->GlobalTasksBegin + CurrentStateTree->GlobalTasksNum); TaskIndex++)
			{
				const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
				const FStateTreeDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
				FNodeInstanceDataScope DataScope(*this, &Task, TaskIndex, Task.InstanceDataHandle, TaskInstanceView);

				// Copy bound properties.
				if (Task.BindingsBatch.IsValid() && Task.bShouldCopyBoundPropertiesOnExitState)
				{
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskInstanceView, Task.BindingsBatch);
				}
			}
		}
	}

	// Call in reverse order.
	FStateTreeTransitionResult Transition;
	Transition.TargetState = FStateTreeStateHandle::FromCompletionStatus(CompletionStatus);
	Transition.CurrentRunStatus = CompletionStatus;

	bool bIsLastGlobalFrame = true;
	for (int32 FrameIndex = Exec.ActiveFrames.Num() - 1; FrameIndex >= 0; FrameIndex--)
	{
		const FStateTreeExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		const FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		if (CurrentFrame.bIsGlobalFrame)
		{
			// LastInitializedTaskIndex belongs to the last frame.
			const FStateTreeIndex16 LastTaskToBeStopped = bIsLastGlobalFrame ? LastInitializedTaskIndex : FStateTreeIndex16::Invalid;
			CallStopOnEvaluatorsAndGlobalTasks(CurrentParentFrame, CurrentFrame, Transition, LastTaskToBeStopped);
			bIsLastGlobalFrame = false;
		}
	}
}

void FStateTreeExecutionContext::CallStopOnEvaluatorsAndGlobalTasks(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& Frame, const FStateTreeTransitionResult& Transition, const FStateTreeIndex16 LastInitializedTaskIndex /*= FStateTreeIndex16()*/)
{
	check(Frame.bIsGlobalFrame);

	ON_SCOPE_EXIT { GetExecState().LastExitedNodeIndex = FStateTreeIndex16::Invalid; };

	FCurrentlyProcessedFrameScope FrameScope(*this, ParentFrame, Frame);
	const UStateTree* CurrentStateTree = Frame.StateTree;

	for (int32 TaskIndex = (CurrentStateTree->GlobalTasksBegin + CurrentStateTree->GlobalTasksNum) - 1;  TaskIndex >= CurrentStateTree->GlobalTasksBegin; TaskIndex--)
	{
		const FStateTreeTaskBase& Task =  CurrentStateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
		const FStateTreeDataView TaskInstanceView = GetDataView(ParentFrame, Frame, Task.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Task, TaskIndex, Task.InstanceDataHandle, TaskInstanceView);

		// Ignore disabled task
		if (Task.bTaskEnabled == false)
		{
			STATETREE_LOG(VeryVerbose, TEXT("%*sSkipped 'ExitState' for disabled Task: '%s'"), UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
			continue;
		}

		// Relying here that invalid value of LastInitializedTaskIndex == MAX_uint16.
		if (TaskIndex <= LastInitializedTaskIndex.Get())
		{
			STATETREE_LOG(Verbose, TEXT("  Stop: '%s'"), *Task.Name.ToString());
			UE_STATETREE_DEBUG_TASK_EXIT_STATE(this, CurrentStateTree, FStateTreeIndex16(TaskIndex));
			{
				QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_TreeStop);
				Task.ExitState(*this, Transition);
			}
			UE_STATETREE_DEBUG_TASK_EVENT(this, TaskIndex, TaskInstanceView, EStateTreeTraceEventType::OnExited, Transition.CurrentRunStatus);
			GetExecState().LastExitedNodeIndex = FStateTreeIndex16(TaskIndex);
		}
	}

	for (int32 EvalIndex = (CurrentStateTree->EvaluatorsBegin + CurrentStateTree->EvaluatorsNum) - 1; EvalIndex >= CurrentStateTree->EvaluatorsBegin; EvalIndex--)
	{
		const FStateTreeEvaluatorBase& Eval = CurrentStateTree->Nodes[EvalIndex].Get<const FStateTreeEvaluatorBase>();
		const FStateTreeDataView EvalInstanceView = GetDataView(ParentFrame, Frame, Eval.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Eval, EvalIndex, Eval.InstanceDataHandle, EvalInstanceView);

		STATETREE_LOG(Verbose, TEXT("  Stop: '%s'"), *Eval.Name.ToString());
		UE_STATETREE_DEBUG_EVALUATOR_EXIT_TREE(this, CurrentStateTree, FStateTreeIndex16(EvalIndex));
		{
			QUICK_SCOPE_CYCLE_COUNTER(StateTree_Eval_TreeStop);
			Eval.TreeStop(*this);

			UE_STATETREE_DEBUG_EVALUATOR_EVENT(this, EvalIndex, EvalInstanceView, EStateTreeTraceEventType::OnTreeStopped);
		}
		GetExecState().LastExitedNodeIndex = FStateTreeIndex16(EvalIndex);
	}
}

EStateTreeRunStatus FStateTreeExecutionContext::StartTemporaryEvaluatorsAndGlobalTasks(const FStateTreeExecutionFrame* CurrentParentFrame, FStateTreeExecutionFrame& CurrentFrame)
{
	if (!CurrentFrame.bIsGlobalFrame)
	{
		return EStateTreeRunStatus::Failed;
	}

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_StartEvaluators);
	STATETREE_LOG(Verbose, TEXT("Start Temporary Evaluators & Global tasks while trying to select linked asset: %s"), *GetNameSafe(CurrentFrame.StateTree));

	FStateTreeExecutionState& Exec = GetExecState();

	FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
	UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::StartGlobalTasksForSelection);

	EStateTreeRunStatus Result = EStateTreeRunStatus::Running;
	const UE::StateTree::FActiveFrameID CurrentFrameID = CurrentFrame.FrameID;
	const UStateTree* CurrentStateTree = CurrentFrame.StateTree;
	UE::StateTree::FTasksCompletionStatus CurrentTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(CurrentStateTree);

	// Start evaluators
	for (int32 EvalIndex = CurrentStateTree->EvaluatorsBegin; EvalIndex < (CurrentStateTree->EvaluatorsBegin + CurrentStateTree->EvaluatorsNum); EvalIndex++)
	{
		const FStateTreeEvaluatorBase& Eval = CurrentStateTree->Nodes[EvalIndex].Get<const FStateTreeEvaluatorBase>();
		FStateTreeDataView EvalInstanceView = GetDataViewOrTemporary(CurrentParentFrame, CurrentFrame, Eval.InstanceDataHandle);
		if (!EvalInstanceView.IsValid())
		{
			EvalInstanceView = AddTemporaryInstance(CurrentFrame, FStateTreeIndex16(EvalIndex), Eval.InstanceDataHandle, CurrentFrame.StateTree->DefaultInstanceData.GetStruct(Eval.InstanceTemplateIndex.Get()));
			check(EvalInstanceView.IsValid());
		}
		
		FNodeInstanceDataScope DataScope(*this, &Eval, EvalIndex, Eval.InstanceDataHandle, EvalInstanceView);
		// Copy bound properties with the temporary buffer.
		if (Eval.BindingsBatch.IsValid())
		{
			CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, EvalInstanceView, Eval.BindingsBatch);
		}

		STATETREE_LOG(Verbose, TEXT("  Start: '%s'"), *Eval.Name.ToString());
		UE_STATETREE_DEBUG_EVALUATOR_ENTER_TREE(this, CurrentStateTree, FStateTreeIndex16(EvalIndex));
		{
			QUICK_SCOPE_CYCLE_COUNTER(StateTree_Eval_TreeStart);
			Eval.TreeStart(*this);

			UE_STATETREE_DEBUG_EVALUATOR_EVENT(this, EvalIndex, EvalInstanceView, EStateTreeTraceEventType::OnTreeStarted);
		}
	}

	// Start Global tasks
	// Even if we call Enter/ExitState() on global tasks, they do not enter any specific state.
	const FStateTreeTransitionResult Transition = {}; // Empty transition

	for (int32 GlobalTaskIndex = 0; GlobalTaskIndex < CurrentStateTree->GlobalTasksNum; ++GlobalTaskIndex)
	{
		const int32 AssetTaskIndex = CurrentStateTree->GlobalTasksBegin + GlobalTaskIndex;
		const FStateTreeTaskBase& Task =  CurrentStateTree->Nodes[AssetTaskIndex].Get<const FStateTreeTaskBase>();
		// Ignore disabled task
		if (Task.bTaskEnabled == false)
		{
			STATETREE_LOG(VeryVerbose, TEXT("%*sSkipped 'EnterState' for disabled Task: '%s'"), UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
			continue;
		}

		FStateTreeDataView TaskDataView = GetDataViewOrTemporary(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
		if (!TaskDataView.IsValid())
		{
			TaskDataView = AddTemporaryInstance(CurrentFrame, FStateTreeIndex16(AssetTaskIndex), Task.InstanceDataHandle, CurrentFrame.StateTree->DefaultInstanceData.GetStruct(Task.InstanceTemplateIndex.Get()));
			check(TaskDataView.IsValid())
		}

		FNodeInstanceDataScope DataScope(*this, &Task, AssetTaskIndex, Task.InstanceDataHandle, TaskDataView);

		// Copy bound properties with the temporary buffer.
		if (Task.BindingsBatch.IsValid())
		{
			CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, TaskDataView, Task.BindingsBatch);
		}

		STATETREE_LOG(Verbose, TEXT("  Start: '%s'"), *Task.Name.ToString());
		UE_STATETREE_DEBUG_TASK_ENTER_STATE(this, CurrentStateTree, FStateTreeIndex16(AssetTaskIndex));

		EStateTreeRunStatus TaskRunStatus = EStateTreeRunStatus::Unset;
		{
			QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_TreeStart);
			TaskRunStatus = Task.EnterState(*this, Transition);
		}

		UE::StateTree::ETaskCompletionStatus TaskStatus = UE::StateTree::ExecutionContext::CastToTaskStatus(TaskRunStatus);
		TaskStatus = CurrentTasksStatus.SetStatusWithPriority(GlobalTaskIndex, TaskStatus);

		TaskRunStatus = UE::StateTree::ExecutionContext::CastToRunStatus(TaskStatus);
		UE_STATETREE_DEBUG_TASK_EVENT(this, AssetTaskIndex, TaskDataView, EStateTreeTraceEventType::OnEntered, TaskRunStatus);

		Result = UE::StateTree::ExecutionContext::GetPriorityRunStatus(Result, TaskRunStatus);
		if (TaskRunStatus == EStateTreeRunStatus::Failed && CurrentTasksStatus.IsConsideredForCompletion(GlobalTaskIndex))
		{
			break;
		}
	}

	return Result;
}

void FStateTreeExecutionContext::StopTemporaryEvaluatorsAndGlobalTasks(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame)
{
	//@todo only stop the evaluators and tasks that were started in StartTemporaryEvaluatorsAndGlobalTasks
	STATETREE_LOG(Verbose, TEXT("Stop Temporary Evaluators & Global tasks"));

	FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
	UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::StopGlobalTasksForSelection);

	// Create temporary transition to stop the unused global tasks and evaluators.
	constexpr  EStateTreeRunStatus CompletionStatus = EStateTreeRunStatus::Stopped; 
	FStateTreeTransitionResult Transition;
	Transition.TargetState = FStateTreeStateHandle::FromCompletionStatus(CompletionStatus);
	Transition.CurrentRunStatus = CompletionStatus;

	TArrayView<FStateTreeTemporaryInstanceData> TempInstances = Storage.GetMutableTemporaryInstances();
	for (int32 Index = TempInstances.Num() - 1; Index >= 0; Index--)
	{
		FStateTreeTemporaryInstanceData& TempInstance = TempInstances[Index];
		if (TempInstance.FrameID != CurrentFrame.FrameID)
		{
			continue;
		}

		if (TempInstance.OwnerNodeIndex.IsValid()
			&& TempInstance.Instance.IsValid())
		{
			FStateTreeDataView NodeInstanceView;
			if (FStateTreeInstanceObjectWrapper* Wrapper = TempInstance.Instance.GetMutablePtr<FStateTreeInstanceObjectWrapper>())
			{
				NodeInstanceView = FStateTreeDataView(Wrapper->InstanceObject);
			}
			else
			{
				NodeInstanceView = FStateTreeDataView(TempInstance.Instance);
			}


			FConstStructView NodeView = CurrentFrame.StateTree->Nodes[TempInstance.OwnerNodeIndex.Get()];
			if (const FStateTreeTaskBase* Task = NodeView.GetPtr<const FStateTreeTaskBase>())
			{
				FNodeInstanceDataScope DataScope(*this, Task, TempInstance.OwnerNodeIndex.Get(), TempInstance.DataHandle, NodeInstanceView);

				STATETREE_LOG(Verbose, TEXT("  Stop: '%s'"), *Task->Name.ToString());
				UE_STATETREE_DEBUG_TASK_EXIT_STATE(this, CurrentFrame.StateTree, FStateTreeIndex16(TempInstance.OwnerNodeIndex.Get()));
				{
					QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_TreeStop);
					Task->ExitState(*this, Transition);
				}
				UE_STATETREE_DEBUG_TASK_EVENT(this, TempInstance.OwnerNodeIndex.Get(), NodeInstanceView, EStateTreeTraceEventType::OnExited, Transition.CurrentRunStatus);
			}
			else if (const FStateTreeEvaluatorBase* Eval = NodeView.GetPtr<const FStateTreeEvaluatorBase>())
			{
				FNodeInstanceDataScope DataScope(*this, Eval, TempInstance.OwnerNodeIndex.Get(), TempInstance.DataHandle, NodeInstanceView);

				STATETREE_LOG(Verbose, TEXT("  Stop: '%s'"), *Eval->Name.ToString());
				UE_STATETREE_DEBUG_EVALUATOR_EXIT_TREE(this, CurrentFrame.StateTree, FStateTreeIndex16(TempInstance.OwnerNodeIndex.Get()));
				{
					QUICK_SCOPE_CYCLE_COUNTER(StateTree_Eval_TreeStop);
					Eval->TreeStop(*this);

					UE_STATETREE_DEBUG_EVALUATOR_EVENT(this, TempInstance.OwnerNodeIndex.Get(), NodeInstanceView, EStateTreeTraceEventType::OnTreeStopped);
				}
			}
		}
	}
}

EStateTreeRunStatus FStateTreeExecutionContext::TickTasks(const float DeltaTime)
{
	// When a task is completed it also completes the state and triggers the completion transition (because LastTickStatus is set).
	// A task can complete async. See CompletedStates.
	// When a task fails, stop ticking the following tasks.
	// When no task ticks, then the leaf completes.

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_TickTasks);
	UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::TickingTasks);

	using namespace UE::StateTree;

	FStateTreeExecutionState& Exec = GetExecState();
	Exec.bHasPendingCompletedState = false;

	if (Exec.ActiveFrames.IsEmpty())
	{
		return EStateTreeRunStatus::Failed;
	}

	int32 NumTotalEnabledTasks = 0;
	const bool bCopyBoundPropertiesOnNonTickedTask = ExecutionContext::Private::bCopyBoundPropertiesOnNonTickedTask;

	FTickTaskArguments TickArgs;
	TickArgs.DeltaTime = DeltaTime;
	TickArgs.bIsGlobalTasks = false;
	TickArgs.bShouldTickTasks = true;

	STATETREE_CLOG(Exec.ActiveFrames.Num() > 0, VeryVerbose, TEXT("Ticking Tasks"));

	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
	{
		TickArgs.ParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		TickArgs.Frame = &Exec.ActiveFrames[FrameIndex];
		const UStateTree* CurrentStateTree = TickArgs.Frame->StateTree;

		FCurrentlyProcessedFrameScope FrameScope(*this, TickArgs.ParentFrame, *TickArgs.Frame);

		if (ExecutionContext::Private::bTickGlobalNodesFollowingTreeHierarchy)
		{
			if (TickArgs.Frame->bIsGlobalFrame)
			{
				constexpr bool bTickGlobalTasks = true;
				const EStateTreeRunStatus FrameResult = TickEvaluatorsAndGlobalTasksForFrame(DeltaTime, bTickGlobalTasks, FrameIndex, TickArgs.ParentFrame, TickArgs.Frame);
				if (FrameResult != EStateTreeRunStatus::Running)
				{
					if (ExecutionContext::Private::bGlobalTasksCompleteOwningFrame == false || FrameIndex == 0)
					{
						// Stop the tree execution when it's the root frame or if the previous behavior is desired.
						Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, FrameResult);
					}
					TickArgs.bShouldTickTasks = false;
					break;
				}
			}
		}

		for (int32 StateIndex = 0; StateIndex < TickArgs.Frame->ActiveStates.Num(); ++StateIndex)
		{
			const FStateTreeStateHandle CurrentHandle = TickArgs.Frame->ActiveStates[StateIndex];
			const FCompactStateTreeState& CurrentState = CurrentStateTree->States[CurrentHandle.Index];
			FTasksCompletionStatus CurrentCompletionStatus = TickArgs.Frame->ActiveTasksStatus.GetStatus(CurrentState);

			TickArgs.StateID = TickArgs.Frame->ActiveStates.StateIDs[StateIndex];
			TickArgs.TasksCompletionStatus = &CurrentCompletionStatus;

			FCurrentlyProcessedStateScope StateScope(*this, CurrentHandle);
			UE_STATETREE_DEBUG_SCOPED_STATE(this, CurrentHandle);

			STATETREE_CLOG(CurrentState.TasksNum > 0, VeryVerbose, TEXT("%*sState '%s'")
				, (FrameIndex + StateIndex + 1) * Debug::IndentSize, TEXT("")
				, *DebugGetStatePath(Exec.ActiveFrames, TickArgs.Frame, StateIndex));

			if (CurrentState.Type == EStateTreeStateType::Linked || CurrentState.Type == EStateTreeStateType::LinkedAsset)
			{
				if (CurrentState.ParameterDataHandle.IsValid() && CurrentState.ParameterBindingsBatch.IsValid())
				{
					const FStateTreeDataView StateParamsDataView = GetDataView(TickArgs.ParentFrame, *TickArgs.Frame, CurrentState.ParameterDataHandle);
					CopyBatchOnActiveInstances(TickArgs.ParentFrame, *TickArgs.Frame, StateParamsDataView, CurrentState.ParameterBindingsBatch);
				}
			}

			const bool bHasEvents = EventQueue && EventQueue->HasEvents();
			bool bRequestLoopStop = false;
			if (bCopyBoundPropertiesOnNonTickedTask || CurrentState.ShouldTickTasks(bHasEvents))
			{
				// Update Tasks data and tick if possible (ie. if no task has yet failed and bShouldTickTasks is true)
				TickArgs.TasksBegin = CurrentState.TasksBegin;
				TickArgs.TasksNum = CurrentState.TasksNum;
				TickArgs.Indent = (FrameIndex + StateIndex + 1);
				const FTickTaskResult TickTasksResult = TickTasks(TickArgs);

				// Keep updating the binding but do not call tick on tasks if there's a failure.
				TickArgs.bShouldTickTasks = TickTasksResult.bShouldTickTasks
					&& !CurrentCompletionStatus.HasAnyFailed();
				// If a failure and we do not copy then bindings, then we can stop.
				bRequestLoopStop = !bCopyBoundPropertiesOnNonTickedTask && !TickTasksResult.bShouldTickTasks;
			}

			NumTotalEnabledTasks += CurrentState.EnabledTasksNum;

			if (bRequestLoopStop)
			{
				break;
			}
		}
	}

	// Collect the result after every tasks has the chance to tick.
	//An async or delegate might complete a global or "previous" task (in a different order).
	EStateTreeRunStatus FirstFrameResult = EStateTreeRunStatus::Running;
	EStateTreeRunStatus FrameResult = EStateTreeRunStatus::Running;
	EStateTreeRunStatus StateResult = EStateTreeRunStatus::Running;
	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
	{
		using namespace UE::StateTree::ExecutionContext;

		const FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UStateTree* CurrentStateTree = CurrentFrame.StateTree;
		if (CurrentFrame.bIsGlobalFrame)
		{
			const ETaskCompletionStatus GlobalTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(CurrentStateTree).GetCompletionStatus();
			if (FrameIndex == 0)
			{
				FirstFrameResult = CastToRunStatus(GlobalTasksStatus);
			}
			FrameResult = GetPriorityRunStatus(FrameResult, CastToRunStatus(GlobalTasksStatus));
		}

		for (int32 StateIndex = 0; StateIndex < CurrentFrame.ActiveStates.Num() && StateResult != EStateTreeRunStatus::Failed; ++StateIndex)
		{
			const FStateTreeStateHandle CurrentHandle = CurrentFrame.ActiveStates[StateIndex];
			const FCompactStateTreeState& State = CurrentStateTree->States[CurrentHandle.Index];
			const ETaskCompletionStatus StateTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(State).GetCompletionStatus();
			StateResult = GetPriorityRunStatus(StateResult, CastToRunStatus(StateTasksStatus));
		}
	}
	
	if (ExecutionContext::Private::bGlobalTasksCompleteOwningFrame && FirstFrameResult != EStateTreeRunStatus::Running)
	{
		Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, FrameResult);
	}
	else if (ExecutionContext::Private::bGlobalTasksCompleteOwningFrame == false && FrameResult != EStateTreeRunStatus::Running)
	{
		Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, FrameResult);
	}
	else if (NumTotalEnabledTasks == 0 && StateResult == EStateTreeRunStatus::Running && FrameResult == EStateTreeRunStatus::Running)
	{
		// No enabled tasks, done ticking.
		//Complete the the bottom state in the bottom frame (to trigger the completion transitions).
		if (ensureMsgf(Exec.ActiveFrames.Num() > 0, TEXT("No task is allowed to clear/stop/transition. Those actions should be delayed inside the execution context.")))
		{
			FStateTreeExecutionFrame& LastFrame = Exec.ActiveFrames.Last();
			const int32 NumberOfActiveState = LastFrame.ActiveStates.Num();
			if (ensureMsgf(NumberOfActiveState != 0, TEXT("No task is allowed to clear/stop/transition. Those action should be delayed inside the execution context.")))
			{
				const FStateTreeStateHandle CurrentHandle = LastFrame.ActiveStates[NumberOfActiveState - 1];
				const FCompactStateTreeState& State = LastFrame.StateTree->States[CurrentHandle.Index];
				LastFrame.ActiveTasksStatus.GetStatus(State).SetCompletionStatus(ETaskCompletionStatus::Succeeded);
			}
			else
			{
				LastFrame.ActiveTasksStatus.GetStatus(LastFrame.StateTree).SetCompletionStatus(ETaskCompletionStatus::Succeeded);
			}
		}
		else
		{
			Exec.RequestedStop = EStateTreeRunStatus::Stopped;
		}
		StateResult = EStateTreeRunStatus::Succeeded;
	}

	Exec.bHasPendingCompletedState = StateResult != EStateTreeRunStatus::Running || FrameResult != EStateTreeRunStatus::Running;
	return StateResult;
}

FStateTreeExecutionContext::FTickTaskResult FStateTreeExecutionContext::TickTasks(const FTickTaskArguments& Args)
{
	using namespace UE::StateTree;

	check(Args.Frame);
	check(Args.TasksCompletionStatus);

	bool bShouldTickTasks = Args.bShouldTickTasks;

	FStateTreeExecutionState& Exec = GetExecState();
	const bool bCopyBoundPropertiesOnNonTickedTask = ExecutionContext::Private::bCopyBoundPropertiesOnNonTickedTask;
	const UStateTree* CurrentStateTree = Args.Frame->StateTree;
	const FActiveFrameID CurrentFrameID = Args.Frame->FrameID;
	check(CurrentStateTree);

	for (int32 OwnerTaskIndex = 0; OwnerTaskIndex < Args.TasksNum; ++OwnerTaskIndex)
	{
		const int32 AssetTaskIndex = Args.TasksBegin + OwnerTaskIndex;
		const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[AssetTaskIndex].Get<const FStateTreeTaskBase>();

		// Ignore disabled task
		if (Task.bTaskEnabled == false)
		{
			STATETREE_LOG(VeryVerbose, TEXT("%*sSkipped 'Tick' for disabled Task: '%s'"), Debug::IndentSize, TEXT(""), *Task.Name.ToString());
			continue;
		}

		const FStateTreeDataView TaskInstanceView = GetDataView(Args.ParentFrame, *Args.Frame, Task.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Task, AssetTaskIndex, Task.InstanceDataHandle, TaskInstanceView);

		const bool bHasEvents = EventQueue && EventQueue->HasEvents();
		const bool bIsTaskRunning = Args.TasksCompletionStatus->IsRunning(OwnerTaskIndex);
		const bool bNeedsTick = bShouldTickTasks
			&& bIsTaskRunning
			&& (Task.bShouldCallTick || (bHasEvents && Task.bShouldCallTickOnlyOnEvents));
		STATETREE_LOG(VeryVerbose, TEXT("%*s  Tick: '%s' %s"), Args.Indent * Debug::IndentSize, TEXT("")
			, *Task.Name.ToString()
			, !bNeedsTick ? TEXT("[not ticked]") : TEXT(""));

		// Copy bound properties.
		// Only copy properties when the task is actually ticked, and copy properties at tick is requested.
		const bool bCopyBatch = (bCopyBoundPropertiesOnNonTickedTask || bNeedsTick)
			&& Task.BindingsBatch.IsValid()
			&& Task.bShouldCopyBoundPropertiesOnTick;
		if (bCopyBatch)
		{
			CopyBatchOnActiveInstances(Args.ParentFrame, *Args.Frame, TaskInstanceView, Task.BindingsBatch);
		}

		if (!bNeedsTick)
		{
			// Task didn't tick because it failed.
			//The following tasks should not tick but we might still need to update their bindings.
			if (!bIsTaskRunning && bShouldTickTasks && Args.TasksCompletionStatus->HasAnyFailed())
			{
				bShouldTickTasks = false;
			}
			continue;
		}

		//UE_STATETREE_DEBUG_TASK_EVENT(this, AssetTaskIndex, TaskDataView, EStateTreeTraceEventType::OnTickingTask, EStateTreeRunStatus::Running);
		UE_STATETREE_DEBUG_TASK_TICK(this, CurrentStateTree, AssetTaskIndex);

		EStateTreeRunStatus TaskRunStatus = EStateTreeRunStatus::Unset;
		{
			QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_Tick);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Task_Tick);

			TaskRunStatus = Task.Tick(*this, Args.DeltaTime);
		}

		// Set the new status and fetch back the status with priority.
		//In case an async task completes the same task.
		//Or in case FinishTask() inside the Task.Tick()
		ETaskCompletionStatus TaskStatus = UE::StateTree::ExecutionContext::CastToTaskStatus(TaskRunStatus);
		TaskStatus = Args.TasksCompletionStatus->SetStatusWithPriority(OwnerTaskIndex, TaskStatus);
		TaskRunStatus = ExecutionContext::CastToRunStatus(TaskStatus);

		UE_STATETREE_DEBUG_TASK_EVENT(this, AssetTaskIndex, TaskInstanceView,
			TaskRunStatus != EStateTreeRunStatus::Running ? EStateTreeTraceEventType::OnTaskCompleted : EStateTreeTraceEventType::OnTicked,
			TaskRunStatus);

		if (TaskRunStatus == EStateTreeRunStatus::Failed && Args.TasksCompletionStatus->IsConsideredForCompletion(OwnerTaskIndex))
		{
			bShouldTickTasks = false;
		}
	}

	return FTickTaskResult{bShouldTickTasks};
}

bool FStateTreeExecutionContext::TestAllConditions(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const int32 ConditionsOffset, const int32 ConditionsNum)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_TestConditions);

	if (ConditionsNum == 0)
	{
		return true;
	}

	TStaticArray<EStateTreeExpressionOperand, UE::StateTree::MaxExpressionIndent + 1> Operands(InPlace, EStateTreeExpressionOperand::Copy);
	TStaticArray<bool, UE::StateTree::MaxExpressionIndent + 1> Values(InPlace, false);

	int32 Level = 0;
	
	for (int32 Index = 0; Index < ConditionsNum; Index++)
	{
		const int32 ConditionIndex = ConditionsOffset + Index;
		const FStateTreeConditionBase& Cond = CurrentFrame.StateTree->Nodes[ConditionIndex].Get<const FStateTreeConditionBase>();
		const FStateTreeDataView ConditionInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Cond.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Cond, ConditionIndex, Cond.InstanceDataHandle, ConditionInstanceView);

		bool bValue = false;
		if (Cond.EvaluationMode == EStateTreeConditionEvaluationMode::Evaluated)
		{
			// Copy bound properties.
			if (Cond.BindingsBatch.IsValid())
			{
				// Use validated copy, since we test in situations where the sources are not always valid (e.g. enter conditions may try to access inactive parent state). 
				if (!CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, ConditionInstanceView, Cond.BindingsBatch))
				{
					// If the source data cannot be accessed, the whole expression evaluates to false.
					UE_STATETREE_DEBUG_CONDITION_EVENT(this, ConditionIndex, ConditionInstanceView, EStateTreeTraceEventType::InternalForcedFailure);
					UE_STATETREE_DEBUG_LOG_EVENT(this, Warning, TEXT("Evaluation forced to false: source data cannot be accessed (e.g. enter conditions trying to access inactive parent state)"));
					Values[0] = false;
					break;
				}
			}
			
			UE_STATETREE_DEBUG_CONDITION_TEST_CONDITION(this, CurrentFrame.StateTree, Index);

			bValue = Cond.TestCondition(*this);
			UE_STATETREE_DEBUG_CONDITION_EVENT(this, ConditionIndex, ConditionInstanceView, bValue ? EStateTreeTraceEventType::Passed : EStateTreeTraceEventType::Failed);
			
			// Reset copied properties that might contain object references.
			if (Cond.BindingsBatch.IsValid())
			{
				CurrentFrame.StateTree->PropertyBindings.Super::ResetObjects(Cond.BindingsBatch, ConditionInstanceView);
			}
		}
		else
		{
			bValue = Cond.EvaluationMode == EStateTreeConditionEvaluationMode::ForcedTrue;
			UE_STATETREE_DEBUG_CONDITION_EVENT(this, ConditionIndex, FStateTreeDataView{}, bValue ? EStateTreeTraceEventType::ForcedSuccess : EStateTreeTraceEventType::ForcedFailure);
		}

		const int32 DeltaIndent = Cond.DeltaIndent;
		const int32 OpenParens = FMath::Max(0, DeltaIndent) + 1;	// +1 for the current value that is stored at the empty slot at the top of the value stack.
		const int32 ClosedParens = FMath::Max(0, -DeltaIndent) + 1;

		// Store the operand to apply when merging higher level down when returning to this level.
		// @todo: remove this conditions in 5.1, needs resaving existing StateTrees.
		const EStateTreeExpressionOperand Operand = Index == 0 ? EStateTreeExpressionOperand::Copy : Cond.Operand;
		Operands[Level] = Operand;

		// Store current value at the top of the stack.
		Level += OpenParens;
		Values[Level] = bValue;

		// Evaluate and merge down values based on closed braces.
		// The current value is placed in parens (see +1 above), which makes merging down and applying the new value consistent.
		// The default operand is copy, so if the value is needed immediately, it is just copied down, or if we're on the same level,
		// the operand storing above gives handles with the right logic.
		for (int32 Paren = 0; Paren < ClosedParens; Paren++)
		{
			Level--;
			switch (Operands[Level])
			{
			case EStateTreeExpressionOperand::Copy:
				Values[Level] = Values[Level + 1];
				break;
			case EStateTreeExpressionOperand::And:
				Values[Level] &= Values[Level + 1];
				break;
			case EStateTreeExpressionOperand::Or:
				Values[Level] |= Values[Level + 1];
				break;
			}
			Operands[Level] = EStateTreeExpressionOperand::Copy;
		}
	}
	
	return Values[0];
}

float FStateTreeExecutionContext::EvaluateUtility(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const int32 ConsiderationsOffset, const int32 ConsiderationsNum, const float StateWeight)
{
	// @todo: Tracing support
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_EvaluateUtility);

	if (ConsiderationsNum == 0)
	{
		return .0f;
	}

	TStaticArray<EStateTreeExpressionOperand, UE::StateTree::MaxExpressionIndent + 1> Operands(InPlace, EStateTreeExpressionOperand::Copy);
	TStaticArray<float, UE::StateTree::MaxExpressionIndent + 1> Values(InPlace, false);

	int32 Level = 0;
	float Value = .0f;
	for (int32 Index = 0; Index < ConsiderationsNum; Index++)
	{
		const int32 ConsiderationIndex = ConsiderationsOffset + Index;
		const FStateTreeConsiderationBase& Consideration = CurrentFrame.StateTree->Nodes[ConsiderationIndex].Get<const FStateTreeConsiderationBase>();
		const FStateTreeDataView ConsiderationInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Consideration.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Consideration, ConsiderationIndex, Consideration.InstanceDataHandle, ConsiderationInstanceView);

		// Copy bound properties.
		if (Consideration.BindingsBatch.IsValid())
		{
			// Use validated copy, since we test in situations where the sources are not always valid (e.g. considerations may try to access inactive parent state). 
			if (!CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, ConsiderationInstanceView, Consideration.BindingsBatch))
			{
				// If the source data cannot be accessed, the whole expression evaluates to zero.
				Values[0] = .0f;
				break;
			}
		}

		Value = Consideration.GetNormalizedScore(*this);

		// Reset copied properties that might contain object references.
		if (Consideration.BindingsBatch.IsValid())
		{
			CurrentFrame.StateTree->PropertyBindings.Super::ResetObjects(Consideration.BindingsBatch, ConsiderationInstanceView);
		}

		const int32 DeltaIndent = Consideration.DeltaIndent;
		const int32 OpenParens = FMath::Max(0, DeltaIndent) + 1;	// +1 for the current value that is stored at the empty slot at the top of the value stack.
		const int32 ClosedParens = FMath::Max(0, -DeltaIndent) + 1;

		// Store the operand to apply when merging higher level down when returning to this level.
		const EStateTreeExpressionOperand Operand = Index == 0 ? EStateTreeExpressionOperand::Copy : Consideration.Operand;
		Operands[Level] = Operand;

		// Store current value at the top of the stack.
		Level += OpenParens;
		Values[Level] = Value;

		// Evaluate and merge down values based on closed braces.
		// The current value is placed in parens (see +1 above), which makes merging down and applying the new value consistent.
		// The default operand is copy, so if the value is needed immediately, it is just copied down, or if we're on the same level,
		// the operand storing above gives handles with the right logic.
		for (int32 Paren = 0; Paren < ClosedParens; Paren++)
		{
			Level--;
			switch (Operands[Level])
			{
			case EStateTreeExpressionOperand::Copy:
				Values[Level] = Values[Level + 1];
				break;
			case EStateTreeExpressionOperand::And:
				Values[Level] = FMath::Min(Values[Level], Values[Level + 1]);
				break;
			case EStateTreeExpressionOperand::Or:
				Values[Level] = FMath::Max(Values[Level], Values[Level + 1]);
				break;
			}
			Operands[Level] = EStateTreeExpressionOperand::Copy;
		}
	}

	return StateWeight * Values[0];
}

void FStateTreeExecutionContext::EvaluatePropertyFunctionsOnActiveInstances(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame, FStateTreeIndex16 FuncsBegin, uint16 FuncsNum)
{
	for (int32 FuncIndex = FuncsBegin.Get(); FuncIndex < FuncsBegin.Get() + FuncsNum; ++FuncIndex)
	{
		const FStateTreePropertyFunctionBase& Func = CurrentFrame.StateTree->Nodes[FuncIndex].Get<const FStateTreePropertyFunctionBase>();
		const FStateTreeDataView FuncInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Func.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Func, FuncIndex, Func.InstanceDataHandle, FuncInstanceView);

		// Copy bound properties.
		if (Func.BindingsBatch.IsValid())
		{
			// Use validated copy, since we test in situations where the sources are not always valid (e.g. enter conditions may try to access inactive parent state). 
			CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, FuncInstanceView, Func.BindingsBatch);
		}
			
		Func.Execute(*this);
			
		// Reset copied properties that might contain object references.
		if (Func.BindingsBatch.IsValid())
		{
			CurrentFrame.StateTree->PropertyBindings.Super::ResetObjects(Func.BindingsBatch, FuncInstanceView);
		}
	}
}

void FStateTreeExecutionContext::EvaluatePropertyFunctionsWithValidation(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame, FStateTreeIndex16 FuncsBegin, uint16 FuncsNum)
{
	for (int32 FuncIndex = FuncsBegin.Get(); FuncIndex < FuncsBegin.Get() + FuncsNum; ++FuncIndex)
	{
		const FStateTreePropertyFunctionBase& Func = CurrentFrame.StateTree->Nodes[FuncIndex].Get<const FStateTreePropertyFunctionBase>();
		const FStateTreeDataView FuncInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Func.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Func, FuncIndex, Func.InstanceDataHandle, FuncInstanceView);

		// Copy bound properties.
		if (Func.BindingsBatch.IsValid())
		{
			// Use validated copy, since we test in situations where the sources are not always valid (e.g. enter conditions may try to access inactive parent state). 
			CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, FuncInstanceView, Func.BindingsBatch);
		}
			
		Func.Execute(*this);
			
		// Reset copied properties that might contain object references.
		if (Func.BindingsBatch.IsValid())
		{
			CurrentFrame.StateTree->PropertyBindings.Super::ResetObjects(Func.BindingsBatch, FuncInstanceView);
		}
	}
}

FString FStateTreeExecutionContext::DebugGetEventsAsString() const
{
	TStringBuilder<512> StrBuilder;

	if (EventQueue)
	{
		for (const FStateTreeSharedEvent& Event : EventQueue->GetEventsView())
		{
			if (Event.IsValid())
			{
				if (StrBuilder.Len() > 0)
				{
					StrBuilder << TEXT(", ");
				}

				const bool bHasTag = Event->Tag.IsValid();
				const bool bHasPayload = Event->Payload.GetScriptStruct() != nullptr;
			
				if (bHasTag || bHasPayload)
				{
					StrBuilder << (TEXT('('));
				
					if (bHasTag)
					{
						StrBuilder << TEXT("Tag: '");
						StrBuilder << Event->Tag.GetTagName();
						StrBuilder << TEXT('\'');
					}
					if (bHasTag && bHasPayload)
					{
						StrBuilder << TEXT(", ");
					}
					if (bHasPayload)
					{
						StrBuilder << TEXT(" Payload: '");
						StrBuilder << Event->Payload.GetScriptStruct()->GetFName();
						StrBuilder << TEXT('\'');
					}
					StrBuilder << TEXT(") ");
				}
			}
		}
	}

	return StrBuilder.ToString();
}

bool FStateTreeExecutionContext::RequestTransition(
	const FStateTreeExecutionFrame& CurrentFrame,
	const FStateTreeStateHandle NextState,
	const EStateTreeTransitionPriority Priority,
	const FStateTreeSharedEvent* TransitionEvent,
	const EStateTreeSelectionFallback Fallback)
{
	// Skip lower priority transitions.
	if (NextTransition.Priority >= Priority)
	{
		return false;
	}

	if (NextState.IsCompletionState())
	{
		SetupNextTransition(CurrentFrame, NextState, Priority);
		STATETREE_LOG(Verbose, TEXT("Transition on state '%s' -> state '%s'"),
			*GetSafeStateName(CurrentFrame, CurrentFrame.ActiveStates.Last()), *NextState.Describe());
		return true;
	}
	if (!NextState.IsValid())
	{
		// NotSet is no-operation, but can be used to mask a transition at parent state. Returning unset keeps updating current state.
		SetupNextTransition(CurrentFrame, FStateTreeStateHandle::Invalid, Priority);
		return true;
	}

	FStateSelectionResult StateSelectionResult;
	if (SelectState(CurrentFrame, NextState, StateSelectionResult, TransitionEvent, Fallback))
	{
		SetupNextTransition(CurrentFrame, NextState, Priority);
		NextTransition.NextActiveFrames = StateSelectionResult.GetSelectedFrames();
		NextTransition.NextActiveFrameEvents = StateSelectionResult.GetFramesStateSelectionEvents();

		// Consume events from states, if required. 
		for (int32 FrameIndex = 0; FrameIndex < NextTransition.NextActiveFrames.Num(); FrameIndex++)
		{
			const FStateTreeExecutionFrame& Frame = NextTransition.NextActiveFrames[FrameIndex];
			const FStateTreeFrameStateSelectionEvents& FrameEvents = NextTransition.NextActiveFrameEvents[FrameIndex];

			for (int32 StateIndex = 0; StateIndex < Frame.ActiveStates.Num(); StateIndex++)
			{
				if (FrameEvents.Events[StateIndex].IsValid())
				{
					const FCompactStateTreeState& State = Frame.StateTree->States[StateIndex];
					if (State.bConsumeEventOnSelect)
					{
						ConsumeEvent(FrameEvents.Events[StateIndex]);
					}
				}
			}
		}

		STATETREE_LOG(Verbose, TEXT("Transition on state '%s' -[%s]-> state '%s'"),
			*GetSafeStateName(CurrentFrame, CurrentFrame.ActiveStates.Last()),
			*GetSafeStateName(CurrentFrame, NextState),
			*GetSafeStateName(NextTransition.NextActiveFrames.Last(), NextTransition.NextActiveFrames.Last().ActiveStates.Last()));

		return true;
	}

	return false;
}

void FStateTreeExecutionContext::SetupNextTransition(const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeStateHandle NextState, const EStateTreeTransitionPriority Priority)
{
	const FStateTreeExecutionState& Exec = GetExecState();

	NextTransition.SourceFrameID = CurrentFrame.FrameID;
	NextTransition.SourceStateID = UE::StateTree::FActiveStateID();
	if (CurrentlyProcessedState.IsValid())
	{
		const int32 CurrentStateIndex = CurrentFrame.ActiveStates.IndexOfReverse(CurrentlyProcessedState);
		if (CurrentStateIndex != INDEX_NONE)
		{
			NextTransition.SourceStateID = CurrentFrame.ActiveStates.StateIDs[CurrentStateIndex];
		}
	}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	NextTransition.SourceState = CurrentlyProcessedState;
	NextTransition.SourceStateTree = CurrentFrame.StateTree;
	NextTransition.SourceRootState = CurrentFrame.ActiveStates.GetStateSafe(0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	NextTransition.TargetState = NextState;
	NextTransition.CurrentState = FStateTreeStateHandle::Invalid;
	NextTransition.CurrentRunStatus = Exec.LastTickStatus;
	NextTransition.ChangeType = EStateTreeStateChangeType::Changed; 
	NextTransition.Priority = Priority;

	FStateTreeExecutionFrame& NewFrame = NextTransition.NextActiveFrames.AddDefaulted_GetRef();
	NewFrame.StateTree = CurrentFrame.StateTree;
	NewFrame.RootState = CurrentFrame.RootState;
	NewFrame.ActiveTasksStatus = CurrentFrame.ActiveTasksStatus;

	if (NextState == FStateTreeStateHandle::Invalid)
	{
		NewFrame.ActiveStates = {};
	}
	else
	{
		NewFrame.ActiveStates = FStateTreeActiveStates(NextState, UE::StateTree::FActiveStateID::Invalid);
	}
}

bool FStateTreeExecutionContext::TriggerTransitions()
{
	//1. Process transition requests. Keep the single request with the highest priority.
	//2. Process tick/event/delegate transitions and tasks. TriggerTransitions, from bottom to top.
	// If delayed,
	//	If delayed completed, then process.
	//	Else add them to the delayed transition list.
	//3. If no transition, Process completion transitions, from top to bottom.
	//4. If transition occurs, check if there are any frame (sub-tree) that completed.

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_TriggerTransition);
	UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::TriggerTransitions);

	FAllowDirectTransitionsScope AllowDirectTransitionsScope(*this); // Set flag for the scope of this function to allow direct transitions without buffering.
	FStateTreeExecutionState& Exec = GetExecState();

	if (EventQueue && EventQueue->HasEvents())
	{
		STATETREE_LOG(Verbose, TEXT("Trigger transitions with events: %s"), *DebugGetEventsAsString());
		UE_STATETREE_DEBUG_LOG_EVENT(this, Log, TEXT("Trigger transitions with events: %s"), *DebugGetEventsAsString());
	}

	NextTransition = FStateTreeTransitionResult();

	//
	// Process transition requests
	//
	for (const FStateTreeTransitionRequest& Request : InstanceData.GetTransitionRequests())
	{
		// Find frame associated with the request.
		const FStateTreeExecutionFrame* CurrentFrame = Exec.FindActiveFrame(Request.SourceFrameID);
		if (CurrentFrame)
		{
			if (RequestTransition(*CurrentFrame, Request.TargetState, Request.Priority, /*TransitionEvent*/nullptr, Request.Fallback))
			{
				NextTransitionSource = FStateTreeTransitionSource(CurrentFrame->StateTree, EStateTreeTransitionSourceType::ExternalRequest, Request.TargetState, Request.Priority);
			}
		}
	}

	//@todo should only clear once when the transition is successful.
	//to prevent 2 async requests and the first requests fails for X reason.
	//they will be identified by a Frame/StateID so it's fine if they stay in the array.
	InstanceData.ResetTransitionRequests();

	//
	// Collect expired delayed transitions
	//
	TArray<FStateTreeTransitionDelayedState, TInlineAllocator<8>> ExpiredTransitionsDelayed;
	for (TArray<FStateTreeTransitionDelayedState>::TIterator It = Exec.DelayedTransitions.CreateIterator(); It; ++It)
	{
		if (It->TimeLeft <= 0.0f)
		{
			ExpiredTransitionsDelayed.Emplace(MoveTemp(*It));
			It.RemoveCurrentSwap();
		}
	}

	//
	// Collect tick, event, and task based transitions.
	//
	struct FTransitionHandler
	{
		FTransitionHandler() = default;
		
		FTransitionHandler(const int32 InFrameIndex, const FStateTreeStateHandle InStateHandle, const UE::StateTree::FActiveStateID InStateID, const EStateTreeTransitionPriority InPriority)
			: StateHandle(InStateHandle)
			, StateID(InStateID)
			, TaskIndex(FStateTreeIndex16::Invalid)
			, FrameIndex(InFrameIndex)
			, Priority(InPriority)
		{
		}

		FTransitionHandler(const int32 InFrameIndex, const FStateTreeStateHandle InStateHandle, const UE::StateTree::FActiveStateID InStateID, const FStateTreeIndex16 InTaskIndex, const EStateTreeTransitionPriority InPriority)
			: StateHandle(InStateHandle)
			, StateID(InStateID)
			, TaskIndex(InTaskIndex)
			, FrameIndex(InFrameIndex)
			, Priority(InPriority)
		{
		}

		FStateTreeStateHandle StateHandle;
		UE::StateTree::FActiveStateID StateID;
		FStateTreeIndex16 TaskIndex = FStateTreeIndex16::Invalid;
		int32 FrameIndex = 0;
		EStateTreeTransitionPriority Priority = EStateTreeTransitionPriority::Normal;

		bool operator<(const FTransitionHandler& Other) const
		{
			// Highest priority first.
			return Priority > Other.Priority;
		}
	};

	TArray<FTransitionHandler, TInlineAllocator<16>> TransitionHandlers;

	if (Exec.ActiveFrames.Num() > 0)
	{
		// Re-cache bHasEvents, RequestTransition above can create new events.
		const bool bHasEvents = EventQueue && EventQueue->HasEvents();
		const bool bHasBroadcastedDelegates = Storage.HasBroadcastedDelegates();

		// Transition() can TriggerTransitions() in a loop when a sub-frame completes.
		//We do not want to evaluate the transition from that sub-frame.
		const int32 EndFrameIndex = TriggerTransitionsFromFrameIndex.Get(Exec.ActiveFrames.Num() - 1);
		for (int32 FrameIndex = EndFrameIndex; FrameIndex >= 0; FrameIndex--)
		{
			FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
			const UStateTree* CurrentStateTree = CurrentFrame.StateTree;

			for (int32 StateIndex = CurrentFrame.ActiveStates.Num() - 1; StateIndex >= 0; StateIndex--)
			{
				const FStateTreeStateHandle StateHandle = CurrentFrame.ActiveStates[StateIndex];
				const UE::StateTree::FActiveStateID StateID = CurrentFrame.ActiveStates.StateIDs[StateIndex];
				const FCompactStateTreeState& State = CurrentStateTree->States[StateHandle.Index];

				// Do not process any transitions from a disabled state
				if (!State.bEnabled)
				{
					continue;
				}

				// Transition tasks.
				if (State.bHasTransitionTasks)
				{
					bool bAdded = false;
					for (int32 TaskIndex = (State.TasksBegin + State.TasksNum) - 1; TaskIndex >= State.TasksBegin; TaskIndex--)
					{
						const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
						if (Task.bShouldAffectTransitions && Task.bTaskEnabled)
						{
							TransitionHandlers.Emplace(FrameIndex, StateHandle, StateID, FStateTreeIndex16(TaskIndex), Task.TransitionHandlingPriority);
							bAdded = true;
						}
					}
					ensureMsgf(bAdded, TEXT("bHasTransitionTasks is set but not task were added for the State: '%s' inside theStateTree %s"), *State.Name.ToString(), *CurrentStateTree->GetPathName());
				}

				// Has expired transition delayed.
				const bool bHasActiveTransitionDelayed = ExpiredTransitionsDelayed.ContainsByPredicate([StateID](const FStateTreeTransitionDelayedState& Other)
				{
					return Other.StateID == StateID;
				});
				
				// Regular transitions on state
				//or A transition task can trigger an event. We need to add the state if that is a possibility
				//or Expired transition delayed
				if (State.ShouldTickTransitions(bHasEvents, bHasBroadcastedDelegates) || State.bHasTransitionTasks || bHasActiveTransitionDelayed)
				{
					TransitionHandlers.Emplace(FrameIndex, StateHandle, StateID, EStateTreeTransitionPriority::Normal);
				}
			}

			if (CurrentFrame.bIsGlobalFrame)
			{
				// Global transition tasks.
				if (CurrentFrame.StateTree->bHasGlobalTransitionTasks)
				{
					bool bAdded = false;
					for (int32 TaskIndex = (CurrentStateTree->GlobalTasksBegin + CurrentStateTree->GlobalTasksNum) - 1; TaskIndex >= CurrentFrame.StateTree->GlobalTasksBegin; TaskIndex--)
					{
						const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
						if (Task.bShouldAffectTransitions && Task.bTaskEnabled)
						{
							TransitionHandlers.Emplace(FrameIndex, FStateTreeStateHandle(), UE::StateTree::FActiveStateID::Invalid, FStateTreeIndex16(TaskIndex), Task.TransitionHandlingPriority);
							bAdded = true;
						}
					}
					ensureMsgf(bAdded, TEXT("bHasGlobalTransitionTasks is set but not task were added for the StateTree `%s`"), *CurrentStateTree->GetPathName());
				}
			}
		}

		// Sort by priority and adding order.
		TransitionHandlers.StableSort();
	}

	//
	// Process task and state transitions in priority order. 
	//
	for (const FTransitionHandler& Handler : TransitionHandlers)
	{
		const int32 FrameIndex = Handler.FrameIndex;
		FStateTreeExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UE::StateTree::FActiveFrameID CurrentFrameID = CurrentFrame.FrameID;
		const UStateTree* CurrentStateTree = CurrentFrame.StateTree;

		FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
		FCurrentlyProcessedStateScope StateScope(*this, Handler.StateHandle);
		UE_STATETREE_DEBUG_SCOPED_STATE(this, Handler.StateHandle);

		if (Handler.TaskIndex.IsValid())
		{
			const FStateTreeTaskBase& Task = CurrentStateTree->Nodes[Handler.TaskIndex.Get()].Get<const FStateTreeTaskBase>();

			// Ignore disabled task
			if (Task.bTaskEnabled == false)
			{
				STATETREE_LOG(VeryVerbose, TEXT("%*sSkipped 'TriggerTransitions' for disabled Task: '%s'"), UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
				continue;
			}

			const FStateTreeDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
			FNodeInstanceDataScope DataScope(*this, &Task, Handler.TaskIndex.Get(), Task.InstanceDataHandle, TaskInstanceView);

			// Copy bound properties.
			if (Task.BindingsBatch.IsValid())
			{
				CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskInstanceView, Task.BindingsBatch);
			}

			STATETREE_LOG(VeryVerbose, TEXT("%*sTriggerTransitions: '%s'"), UE::StateTree::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
			UE_STATETREE_DEBUG_TASK_EVENT(this, Handler.TaskIndex.Get(), TaskInstanceView, EStateTreeTraceEventType::OnEvaluating, EStateTreeRunStatus::Running);
			check(TaskInstanceView.IsValid());

			Task.TriggerTransitions(*this);
		}
		else if (Handler.StateHandle.IsValid())
		{
			check(Handler.StateID.IsValid());
			const FCompactStateTreeState& State = CurrentStateTree->States[Handler.StateHandle.Index];

			// Transitions
			for (uint8 TransitionCounter = 0; TransitionCounter < State.TransitionsNum; ++TransitionCounter)
			{
				// All transition conditions must pass
				const int16 TransitionIndex = State.TransitionsBegin + TransitionCounter;
				const FCompactStateTransition& Transition = CurrentStateTree->Transitions[TransitionIndex];

				// Skip disabled transitions
				if (Transition.bTransitionEnabled == false)
				{
					continue;
				}
				
				// No need to test the transition if same or higher priority transition has already been processed.
				if (Transition.Priority <= NextTransition.Priority)
				{
					continue;
				}

				// Skip completion transitions
				if (EnumHasAnyFlags(Transition.Trigger, EStateTreeTransitionTrigger::OnStateCompleted))
				{
					continue;
				}

				// If a delayed transition has passed the delay, try trigger it.
				if (Transition.HasDelay())
				{
					bool bTriggeredDelayedTransition = false;
					for (const FStateTreeTransitionDelayedState& DelayedTransition : ExpiredTransitionsDelayed)
					{
						if (DelayedTransition.StateID == Handler.StateID && DelayedTransition.TransitionIndex == FStateTreeIndex16(TransitionIndex))
						{
							STATETREE_LOG(Verbose, TEXT("Passed delayed transition from '%s' (%s) -> '%s'"),
								*GetSafeStateName(CurrentFrame, CurrentFrame.ActiveStates.Last()), *State.Name.ToString(), *GetSafeStateName(CurrentFrame, Transition.State));

							// Trigger Delayed Transition when the delay has passed.
							if (RequestTransition(CurrentFrame, Transition.State, Transition.Priority, &DelayedTransition.CapturedEvent, Transition.Fallback))
							{
								// If the transition was successfully requested with a specific event, consume and remove the event, it's been used.
								if (DelayedTransition.CapturedEvent.IsValid() && Transition.bConsumeEventOnSelect)
								{
									ConsumeEvent(DelayedTransition.CapturedEvent);
								}

								NextTransitionSource = FStateTreeTransitionSource(CurrentFrame.StateTree, FStateTreeIndex16(TransitionIndex), Transition.State, Transition.Priority);
								bTriggeredDelayedTransition = true;
								break;
							}
						}
					}

					if (bTriggeredDelayedTransition)
					{
						continue;
					}
				}

				TArray<const FStateTreeSharedEvent*, TInlineAllocator<8>> TransitionEvents;

				if (Transition.Trigger == EStateTreeTransitionTrigger::OnEvent)
				{
					check(Transition.RequiredEvent.IsValid());

					TConstArrayView<FStateTreeSharedEvent> EventsQueue = GetEventsToProcessView();
					for (const FStateTreeSharedEvent& Event : EventsQueue)
					{
						check(Event.IsValid());
						if (Transition.RequiredEvent.DoesEventMatchDesc(*Event))
						{
							TransitionEvents.Emplace(&Event);
						}
					}
				}
				else if (EnumHasAnyFlags(Transition.Trigger, EStateTreeTransitionTrigger::OnTick))
				{
					// Dummy event to make sure we iterate to loop below once.
					TransitionEvents.Emplace(nullptr);
				}
				else if (EnumHasAnyFlags(Transition.Trigger, EStateTreeTransitionTrigger::OnDelegate))
				{
					if (Storage.IsDelegateBroadcasted(Transition.RequiredDelegateDispatcher))
					{
						// Dummy event to make sure we iterate to loop below once.
						TransitionEvents.Emplace(nullptr);
					}
				}
				else
				{
					ensureMsgf(false, TEXT("The trigger type is not supported."));
				}

				for (const FStateTreeSharedEvent* TransitionEvent : TransitionEvents)
				{
					bool bPassed = false; 
					{
						FCurrentlyProcessedTransitionEventScope TransitionEventScope(*this, TransitionEvent ? TransitionEvent->Get() : nullptr);
						UE_STATETREE_DEBUG_TRANSITION_EVENT(this, FStateTreeTransitionSource(CurrentFrame.StateTree, FStateTreeIndex16(TransitionIndex), Transition.State, Transition.Priority), EStateTreeTraceEventType::OnEvaluating);
						UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::TransitionConditions);
						bPassed = TestAllConditions(CurrentParentFrame, CurrentFrame, Transition.ConditionsBegin, Transition.ConditionsNum);
					}

					if (bPassed)
					{
						// If the transitions is delayed, set up the delay. 
						if (Transition.HasDelay())
						{
							uint32 TransitionEventHash = 0u;
							if (TransitionEvent && TransitionEvent->IsValid())
							{		
								TransitionEventHash = GetTypeHash(*TransitionEvent->Get());
							}

							const bool bIsDelayedTransitionExisting = Exec.DelayedTransitions.ContainsByPredicate([CurrentStateID =  Handler.StateID, TransitionIndex, TransitionEventHash](const FStateTreeTransitionDelayedState& DelayedState)
							{
								return DelayedState.StateID == CurrentStateID
									&& DelayedState.TransitionIndex.Get() == TransitionIndex
									&& DelayedState.CapturedEventHash == TransitionEventHash;
							});

							if (!bIsDelayedTransitionExisting)
							{
								// Initialize new delayed transition.
								const float DelayDuration = Transition.Delay.GetRandomDuration(Exec.RandomStream);
								if (DelayDuration > 0.0f)
								{
									FStateTreeTransitionDelayedState& DelayedState = Exec.DelayedTransitions.AddDefaulted_GetRef();
									DelayedState.StateID = Handler.StateID;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
									DelayedState.StateTree = CurrentFrame.StateTree;
									DelayedState.StateHandle = Handler.StateHandle;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
									DelayedState.TransitionIndex = FStateTreeIndex16(TransitionIndex);
									DelayedState.TimeLeft = DelayDuration;
									if (TransitionEvent && TransitionEvent->IsValid())
									{
										DelayedState.CapturedEvent = *TransitionEvent;
										DelayedState.CapturedEventHash = TransitionEventHash;
									}

									BeginDelayedTransition(DelayedState);
									STATETREE_LOG(Verbose, TEXT("Delayed transition triggered from '%s' (%s) -> '%s' %.1fs"),
										*GetSafeStateName(CurrentFrame, CurrentFrame.ActiveStates.Last()), *State.Name.ToString(), *GetSafeStateName(CurrentFrame, Transition.State), DelayedState.TimeLeft);
								
									// Delay state added, skip requesting the transition.
									continue;
								}
								// Fallthrough to request transition if duration was zero.
							}
							else
							{
								// We get here if the transitions re-triggers during the delay, on which case we'll just ignore it.
								continue;
							}
						}

						UE_STATETREE_DEBUG_TRANSITION_EVENT(this, FStateTreeTransitionSource(CurrentFrame.StateTree, FStateTreeIndex16(TransitionIndex), Transition.State, Transition.Priority), EStateTreeTraceEventType::OnRequesting);
						if (RequestTransition(CurrentFrame, Transition.State, Transition.Priority, TransitionEvent, Transition.Fallback))
						{
							// If the transition was successfully requested with a specific event, consume and remove the event, it's been used.
							if (TransitionEvent && Transition.bConsumeEventOnSelect)
							{
								ConsumeEvent(*TransitionEvent);
							}
							
							NextTransitionSource = FStateTreeTransitionSource(CurrentFrame.StateTree, FStateTreeIndex16(TransitionIndex), Transition.State, Transition.Priority);
							break;
						}
					}
				}
			}
		}
	}

	// All events have had the change to be reacted to, clear the event queue (if this instance owns it).
	if (InstanceData.IsOwningEventQueue() && EventQueue)
	{
		EventQueue->Reset();
	}

	Storage.ResetBroadcastedDelegates();

	//
	// Check state completion transitions.
	//
	bool bProcessSubTreeCompletion = true;

	if (NextTransition.Priority == EStateTreeTransitionPriority::None
		&& (Exec.LastTickStatus != EStateTreeRunStatus::Running || Exec.bHasPendingCompletedState))
	{
		// Find the pending completed frame/state. Don't cache the result because this function is reentrant.
		//Stop at the first completion.
		int32 FrameIndexToStart = -1; 
		int32 StateIndexToStart = -1;
		EStateTreeRunStatus CurrentStatus = EStateTreeRunStatus::Unset;
		for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
		{
			using namespace UE::StateTree;

			const FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
			const UStateTree* CurrentStateTree = CurrentFrame.StateTree;
			const ETaskCompletionStatus FrameTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(CurrentStateTree).GetCompletionStatus();
			if (FrameTasksStatus != ETaskCompletionStatus::Running)
			{
				if (FrameIndex == 0)
				{
					// If first frame, then complete the tree execution.
					Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, ExecutionContext::CastToRunStatus(FrameTasksStatus));
					break;
				}
				else if (ExecutionContext::Private::bGlobalTasksCompleteOwningFrame)
				{
					const int32 ParentFrameIndex = FrameIndex - 1;
					FStateTreeExecutionFrame& ParentFrame = Exec.ActiveFrames[ParentFrameIndex];
					const FStateTreeStateHandle ParentLinkedState = ParentFrame.ActiveStates.Last();
					if (ensure(ParentLinkedState.IsValid()))
					{
						// Set the parent linked state as last completed state, and update tick status to the status from the transition.
						STATETREE_LOG(Verbose, TEXT("Completed subtree '%s' from global: %s"),
							*GetSafeStateName(ParentFrame, ParentLinkedState),
							*UEnum::GetDisplayValueAsText(ExecutionContext::CastToRunStatus(FrameTasksStatus)).ToString()
							);

						const FCompactStateTreeState& State = ParentFrame.StateTree->States[ParentLinkedState.Index];
						ParentFrame.ActiveTasksStatus.GetStatus(State).SetCompletionStatus(FrameTasksStatus);
						Exec.bHasPendingCompletedState = true;

						CurrentStatus = ExecutionContext::CastToRunStatus(FrameTasksStatus);
						FrameIndexToStart = ParentFrameIndex;
						StateIndexToStart = ParentFrame.ActiveStates.Num() - 1;
						break;
					}
				}
			}

			for (int32 StateIndex = 0; StateIndex < CurrentFrame.ActiveStates.Num(); ++StateIndex)
			{
				const FStateTreeStateHandle CurrentHandle = CurrentFrame.ActiveStates[StateIndex];
				const FCompactStateTreeState& State = CurrentStateTree->States[CurrentHandle.Index];
				const UE::StateTree::ETaskCompletionStatus StateTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(State).GetCompletionStatus();
				if (StateTasksStatus != UE::StateTree::ETaskCompletionStatus::Running)
				{
					CurrentStatus = ExecutionContext::CastToRunStatus(StateTasksStatus);
					FrameIndexToStart = FrameIndex;
					StateIndexToStart = StateIndex;
					break;
				}
			}

			if (CurrentStatus != EStateTreeRunStatus::Unset)
			{
				break;
			}
		}

		if (CurrentStatus != EStateTreeRunStatus::Unset)
		{
			const bool bIsCurrentStatusSucceeded = CurrentStatus == EStateTreeRunStatus::Succeeded;
			const bool bIsCurrentStatusFailed = CurrentStatus == EStateTreeRunStatus::Failed;
			const bool bIsCurrentStatusStopped = CurrentStatus == EStateTreeRunStatus::Stopped;
			checkf(bIsCurrentStatusSucceeded || bIsCurrentStatusFailed || bIsCurrentStatusStopped, TEXT("Running is not accepted in the CurrentStatus loop."));

			const EStateTreeTransitionTrigger CompletionTrigger = bIsCurrentStatusSucceeded ? EStateTreeTransitionTrigger::OnStateSucceeded : EStateTreeTransitionTrigger::OnStateFailed;

			// Start from the last completed state and move up to the first state.
			for (int32 FrameIndex = FrameIndexToStart; FrameIndex >= 0; --FrameIndex)
			{
				FStateTreeExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
				FStateTreeExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
				const UStateTree* CurrentStateTree = CurrentFrame.StateTree;

				FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);

				const int32 CurrentStateIndexToStart = FrameIndex == FrameIndexToStart ? StateIndexToStart : CurrentFrame.ActiveStates.Num() - 1;

				// Check completion transitions
				for (int32 StateIndex = CurrentStateIndexToStart; StateIndex >= 0; --StateIndex)
				{
					const FStateTreeStateHandle StateHandle = CurrentFrame.ActiveStates[StateIndex];
					const FCompactStateTreeState& State = CurrentStateTree->States[StateHandle.Index];

					if (State.ShouldTickCompletionTransitions(bIsCurrentStatusSucceeded, bIsCurrentStatusFailed))
					{
						FCurrentlyProcessedStateScope StateScope(*this, StateHandle);
						UE_STATETREE_DEBUG_SCOPED_STATE_PHASE(this, StateHandle, EStateTreeUpdatePhase::TriggerTransitions);

						for (uint8 TransitionCounter = 0; TransitionCounter < State.TransitionsNum; ++TransitionCounter)
						{
							// All transition conditions must pass
							const int16 TransitionIndex = State.TransitionsBegin + TransitionCounter;
							const FCompactStateTransition& Transition = CurrentStateTree->Transitions[TransitionIndex];

							// Skip disabled transitions
							if (!Transition.bTransitionEnabled)
							{
								continue;
							}


							const bool bTransitionAccepted = !bIsCurrentStatusStopped 
								? EnumHasAnyFlags(Transition.Trigger, CompletionTrigger)
								: Transition.Trigger == EStateTreeTransitionTrigger::OnStateCompleted;
							if (bTransitionAccepted)
							{
								bool bPassed = false;
								{
									UE_STATETREE_DEBUG_TRANSITION_EVENT(this, FStateTreeTransitionSource(CurrentFrame.StateTree, FStateTreeIndex16(TransitionIndex), Transition.State, Transition.Priority), EStateTreeTraceEventType::OnEvaluating);
									UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::TransitionConditions);
									bPassed = TestAllConditions(CurrentParentFrame, CurrentFrame, Transition.ConditionsBegin, Transition.ConditionsNum);
								}

								if (bPassed)
								{
									// No delay allowed on completion conditions.
									// No priority on completion transitions, use the priority to signal that state is selected.
									UE_STATETREE_DEBUG_TRANSITION_EVENT(this, FStateTreeTransitionSource(CurrentFrame.StateTree, FStateTreeIndex16(TransitionIndex), Transition.State, Transition.Priority), EStateTreeTraceEventType::OnRequesting);
									if (RequestTransition(CurrentFrame, Transition.State, EStateTreeTransitionPriority::Normal, /*TransitionEvent*/nullptr, Transition.Fallback))
									{
										NextTransitionSource = FStateTreeTransitionSource(CurrentFrame.StateTree, FStateTreeIndex16(TransitionIndex), Transition.State, Transition.Priority);
										break;
									}
								}
							}
						}

						if (NextTransition.Priority != EStateTreeTransitionPriority::None) //-V547
						{
							break;
						}
					}
				}

				// if a valid completion transition has already been found, the remaining transitions in parent frames won't have a higher priority than the found one
				// so skip the remainder. this also prevented false positive warnings and ensures from STDebugger
				if (NextTransition.Priority != EStateTreeTransitionPriority::None) //-V547
				{
					break;
				}
			}

			// Handle the case where no transition was found.
			if (NextTransition.Priority == EStateTreeTransitionPriority::None) //-V547
			{
				STATETREE_LOG(Verbose, TEXT("Could not trigger completion transition, jump back to root state."));
				UE_STATETREE_DEBUG_LOG_EVENT(this, Warning, TEXT("Could not trigger completion transition, jump back to root state."));

				check(!Exec.ActiveFrames.IsEmpty());
				FStateTreeExecutionFrame& RootFrame = Exec.ActiveFrames[0];
				FCurrentlyProcessedFrameScope RootFrameScope(*this, nullptr, RootFrame);
				FCurrentlyProcessedStateScope RootStateScope(*this, FStateTreeStateHandle::Root);
				
				if (RequestTransition(RootFrame, FStateTreeStateHandle::Root, EStateTreeTransitionPriority::Normal))
				{
					NextTransitionSource = FStateTreeTransitionSource(RootFrame.StateTree, EStateTreeTransitionSourceType::Internal, FStateTreeStateHandle::Root, EStateTreeTransitionPriority::Normal);
				}
				else
				{
					STATETREE_LOG(Warning, TEXT("Failed to select root state. Stopping the tree with failure."));
					UE_STATETREE_DEBUG_LOG_EVENT(this, Error, TEXT("Failed to select root state. Stopping the tree with failure."));

					SetupNextTransition(RootFrame, FStateTreeStateHandle::Failed, EStateTreeTransitionPriority::Critical);

					// In this case we don't want to complete subtrees, we want to force the whole tree to stop.
					bProcessSubTreeCompletion = false;
				}
			}
		}
	}

	// Check if the transition was succeed/failed, if we're on a sub-tree, complete the subtree instead of transition.
	if (NextTransition.TargetState.IsCompletionState() && bProcessSubTreeCompletion)
	{
		// Check that the transition source frame is a sub-tree, the first frame (0 index) is not a subtree. 
		const int32 SourceFrameIndex = Exec.IndexOfActiveFrame(NextTransition.SourceFrameID);
		if (SourceFrameIndex > 0)
		{
			const FStateTreeExecutionFrame& SourceFrame = Exec.ActiveFrames[SourceFrameIndex];
			const int32 ParentFrameIndex = SourceFrameIndex - 1;
			FStateTreeExecutionFrame& ParentFrame = Exec.ActiveFrames[ParentFrameIndex];
			const FStateTreeStateHandle ParentLinkedState = ParentFrame.ActiveStates.Last();

			if (ParentLinkedState.IsValid())
			{
				const EStateTreeRunStatus RunStatus = NextTransition.TargetState.ToCompletionStatus();

#if ENABLE_VISUAL_LOG
				const int32 NextTransitionSourceIndex = SourceFrame.ActiveStates.IndexOfReverse(NextTransition.SourceStateID);
				const FStateTreeStateHandle NextTransitionSourceState = NextTransitionSourceIndex != INDEX_NONE
					? SourceFrame.ActiveStates[NextTransitionSourceIndex]
					: FStateTreeStateHandle::Invalid;
				STATETREE_LOG(Verbose, TEXT("Completed subtree '%s' from state '%s': %s"),
					*GetSafeStateName(ParentFrame, ParentLinkedState),
					*GetSafeStateName(SourceFrame, NextTransitionSourceState),
					*UEnum::GetDisplayValueAsText(RunStatus).ToString()
					);
#endif

				// Set the parent linked state as last completed state, and update tick status to the status from the transition.
				const UE::StateTree::ETaskCompletionStatus TaskStatus = UE::StateTree::ExecutionContext::CastToTaskStatus(RunStatus);
				const FCompactStateTreeState& State = ParentFrame.StateTree->States[ParentLinkedState.Index];
				ParentFrame.ActiveTasksStatus.GetStatus(State).SetCompletionStatus(TaskStatus);
				Exec.bHasPendingCompletedState = true;
				Exec.LastTickStatus = RunStatus;

				// Clear the transition and return that no transition took place.
				// Since the LastTickStatus != running, the transition loop will try another transition
				// now starting from the linked parent state. If we run out of retires in the selection loop (e.g. very deep hierarchy)
				// we will continue on next tick.
				TriggerTransitionsFromFrameIndex = ParentFrameIndex;
				NextTransition = FStateTreeTransitionResult();
				return false;
			}
		}
	}

	return NextTransition.TargetState.IsValid();
}

TOptional<FStateTreeTransitionResult> FStateTreeExecutionContext::MakeTransitionResult(const FRecordedStateTreeTransitionResult& RecordedTransition) const
{
	FStateTreeTransitionResult Result;

	for (int32 RecordedFrameIndex = 0; RecordedFrameIndex < RecordedTransition.NextActiveFrames.Num(); RecordedFrameIndex++)
	{
		const FRecordedStateTreeExecutionFrame& RecordedExecutionFrame = RecordedTransition.NextActiveFrames[RecordedFrameIndex];

		if (RecordedExecutionFrame.StateTree == nullptr)
		{
			return {};
		}

		if (RecordedExecutionFrame.StateTree->GetStateFromHandle(RecordedExecutionFrame.RootState) == nullptr)
		{
			return {};
		}

		const FCompactStateTreeFrame* CompactFrame = RecordedExecutionFrame.StateTree->GetFrameFromHandle(RecordedExecutionFrame.RootState);
		if (CompactFrame == nullptr)
		{
			return {};
		}

		FStateTreeExecutionFrame& ExecutionFrame = Result.NextActiveFrames.AddDefaulted_GetRef();
		ExecutionFrame.StateTree = RecordedExecutionFrame.StateTree;
		ExecutionFrame.RootState = RecordedExecutionFrame.RootState;
		ExecutionFrame.ActiveStates = RecordedExecutionFrame.ActiveStates;
		ExecutionFrame.ActiveTasksStatus = FStateTreeTasksCompletionStatus(*CompactFrame);
		ExecutionFrame.bIsGlobalFrame = RecordedExecutionFrame.bIsGlobalFrame;

		FStateTreeFrameStateSelectionEvents& StateTreeFrameStateSelectionEvents = Result.NextActiveFrameEvents.AddDefaulted_GetRef();
		for (int32 EventIdx = 0; EventIdx < RecordedExecutionFrame.EventIndices.Num(); EventIdx++)
		{
			if (RecordedTransition.NextActiveFrameEvents.IsValidIndex(EventIdx))
			{
				const FStateTreeEvent& RecordedStateTreeEvent = RecordedTransition.NextActiveFrameEvents[EventIdx];
				StateTreeFrameStateSelectionEvents.Events[EventIdx] = FStateTreeSharedEvent(RecordedStateTreeEvent);
			}
		}
	}


	if (Result.NextActiveFrames.Num() != Result.NextActiveFrameEvents.Num())
	{
		return {};
	}

	if (RecordedTransition.SourceStateTree == nullptr)
	{
		return {};
	}

	if (RecordedTransition.SourceStateTree->GetFrameFromHandle(RecordedTransition.SourceRootState) == nullptr)
	{
		return {};
	}

	// Try to find the same frame and the same state in the currently active frames.
	// Recorded transitions can be saved and replayed out of context.
	const FStateTreeExecutionState& Exec = GetExecState();
	const FStateTreeExecutionFrame* ExecFrame = Exec.ActiveFrames.FindByPredicate([StateTree = RecordedTransition.SourceStateTree, RootState = RecordedTransition.SourceRootState](const FStateTreeExecutionFrame& Frame)
		{
			return Frame.StateTree == StateTree && Frame.RootState == RootState;
		});
	if (ExecFrame)
	{
		Result.SourceFrameID = ExecFrame->FrameID;
		const int32 SourceStateIndex = ExecFrame->ActiveStates.IndexOfReverse(RecordedTransition.SourceState);
		if (SourceStateIndex != INDEX_NONE)
		{
			Result.SourceStateID = ExecFrame->ActiveStates.StateIDs[SourceStateIndex];
		}
	}
	Result.TargetState = RecordedTransition.TargetState;
	Result.Priority = RecordedTransition.Priority;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Result.SourceState = RecordedTransition.SourceState;
	Result.SourceStateTree = RecordedTransition.SourceStateTree;
	Result.SourceRootState = RecordedTransition.SourceRootState;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	return Result;
}

FRecordedStateTreeTransitionResult FStateTreeExecutionContext::MakeRecordedTransitionResult(const FStateTreeTransitionResult& Transition) const
{
	check(Transition.NextActiveFrames.Num() == Transition.NextActiveFrameEvents.Num());

	FRecordedStateTreeTransitionResult Result;

	for (int32 FrameIndex = 0; FrameIndex < Transition.NextActiveFrames.Num(); FrameIndex++)
	{
		const FStateTreeExecutionFrame& ExecutionFrame = Transition.NextActiveFrames[FrameIndex];
		const FStateTreeFrameStateSelectionEvents& StateSelectionEvents = Transition.NextActiveFrameEvents[FrameIndex];

		FRecordedStateTreeExecutionFrame& RecordedFrame = Result.NextActiveFrames.AddDefaulted_GetRef();
		RecordedFrame.StateTree = ExecutionFrame.StateTree;
		RecordedFrame.RootState = ExecutionFrame.RootState;
		RecordedFrame.ActiveStates = ExecutionFrame.ActiveStates;
		RecordedFrame.bIsGlobalFrame = ExecutionFrame.bIsGlobalFrame;

		for (int32 StateIndex = 0; StateIndex < ExecutionFrame.ActiveStates.Num(); StateIndex++)
		{
			const FStateTreeEvent* Event = StateSelectionEvents.Events[StateIndex].Get();
			if (Event)
			{
				const int32 EventIndex = Result.NextActiveFrameEvents.Add(*Event);
				RecordedFrame.EventIndices[StateIndex] = static_cast<uint8>(EventIndex);
			}
		}
	}

	const FStateTreeExecutionState& Exec = GetExecState();
	if (const FStateTreeExecutionFrame* FoundSourceFrame = Exec.FindActiveFrame(Transition.SourceFrameID))
	{
		Result.SourceStateTree = FoundSourceFrame->StateTree;
		Result.SourceRootState = FoundSourceFrame->RootState;
		const int32 ActiveStateIndex = FoundSourceFrame->ActiveStates.IndexOfReverse(Transition.SourceStateID);
		if (ActiveStateIndex != INDEX_NONE)
		{
			Result.SourceState = FoundSourceFrame->ActiveStates[ActiveStateIndex];
		}
	}
	Result.TargetState = Transition.TargetState;
	Result.Priority = Transition.Priority;

	return Result;
}

bool FStateTreeExecutionContext::SelectState(const FStateTreeExecutionFrame& CurrentFrame,
											const FStateTreeStateHandle NextState,
											FStateSelectionResult& OutSelectionResult,
											const FStateTreeSharedEvent* TransitionEvent,
											const EStateTreeSelectionFallback Fallback)
{
	TGuardValue<const FStateSelectionResult*> GuardValue(CurrentSelectionResult, &OutSelectionResult);

	const FStateTreeExecutionState& Exec = GetExecState();

	if (Exec.ActiveFrames.IsEmpty())
	{
		STATETREE_LOG(Error, TEXT("%hs: SelectState can only be called on initialized tree.  '%s' using StateTree '%s'."),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
		return false;
	}
	
	if (!NextState.IsValid())
	{
		return false;
	}

	// Walk towards the root from current state.
	TArray<FStateTreeStateHandle, TInlineAllocator<FStateTreeActiveStates::MaxStates>> PathToNextState;
	FStateTreeStateHandle CurrState = NextState;
	while (CurrState.IsValid())
	{
		if (PathToNextState.Num() == FStateTreeActiveStates::MaxStates)
		{
			STATETREE_LOG(Error, TEXT("%hs: Reached max execution depth when trying to select state %s from '%s'.  '%s' using StateTree '%s'."),
				__FUNCTION__, *GetSafeStateName(CurrentFrame, NextState), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(&RootStateTree));
			return false;
		}
		// Store the states that are in between the 'NextState' and common ancestor. 
		PathToNextState.Push(CurrState);
		CurrState = CurrentFrame.StateTree->States[CurrState.Index].Parent;
	}

	Algo::Reverse(PathToNextState);

	const UStateTree* NextStateTree = CurrentFrame.StateTree;
	const FStateTreeStateHandle NextRootState = PathToNextState[0]; 

	// Find the frame that the next state belongs to.
	int32 CurrentFrameIndex = INDEX_NONE;
	int32 CurrentStateTreeIndex = INDEX_NONE;

	for (int32 FrameIndex = Exec.ActiveFrames.Num() - 1; FrameIndex >= 0; FrameIndex--)
	{
		const FStateTreeExecutionFrame& Frame = Exec.ActiveFrames[FrameIndex]; 
		if (Frame.StateTree == NextStateTree)
		{
			CurrentStateTreeIndex = FrameIndex;
			if (Frame.RootState == NextRootState)
			{
				CurrentFrameIndex = FrameIndex;
				break;
			}
		}
	}

	// Copy common frames over.
	// ReferenceCurrentFrame is the original of the last copied frame. It will be used to keep track if we are following the current active frames and states.
	const FStateTreeExecutionFrame* CurrentFrameInActiveFrames  = nullptr;
	if (CurrentFrameIndex != INDEX_NONE)
	{
		const int32 NumCommonFrames = CurrentFrameIndex + 1;
		OutSelectionResult = FStateSelectionResult(MakeArrayView(Exec.ActiveFrames.GetData(), NumCommonFrames));
		CurrentFrameInActiveFrames  = &Exec.ActiveFrames[CurrentFrameIndex];
	}
	else if (CurrentStateTreeIndex != INDEX_NONE)
	{
		// If we could not find a common frame, we assume that we jumped to different subtree in same asset.
		const int32 NumCommonFrames = CurrentStateTreeIndex + 1;
		OutSelectionResult = FStateSelectionResult(MakeArrayView(Exec.ActiveFrames.GetData(), NumCommonFrames));
		CurrentFrameInActiveFrames  = &Exec.ActiveFrames[CurrentStateTreeIndex];
	}
	else
	{
		STATETREE_LOG(Error, TEXT("%hs: Encountered unrecognized state %s during state selection from '%s'.  '%s' using StateTree '%s'."),
			__FUNCTION__, *GetNameSafe(NextStateTree), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(NextStateTree));
		return false;
	}
	
	// Append in between state in reverse order, they were collected from leaf towards the root.
	// Note: NextState will be added by SelectStateInternal() if conditions pass.
	const int32 LastFrameIndex = OutSelectionResult.FramesNum() - 1;
	FStateTreeExecutionFrame& LastFrame = OutSelectionResult.GetSelectedFrames()[LastFrameIndex];

	// Find index of the first state to be evaluated.
	int32 FirstNewStateIndex = 0;
	if (CurrentFrameIndex != INDEX_NONE)
	{
		// If LastFrame.ActiveStates is a subset of PathToNextState (e.g when someone use "TryEnter" selection behavior and then make a transition to it's child or if one is reentering the same state).
		// In such case loop below won't break on anything and FirstNewStateIndex will be incorrectly 0, thus we initialize it to be right after the shorter range.
		FirstNewStateIndex = FMath::Max(0, FMath::Min(PathToNextState.Num(), LastFrame.ActiveStates.Num()) - 1);
		for (int32 Index = 0; Index < FMath::Min(PathToNextState.Num(), LastFrame.ActiveStates.Num()); ++Index)
		{
			if (LastFrame.ActiveStates[Index] != PathToNextState[Index])
			{
				FirstNewStateIndex = Index;
				break;
			}
		}
	}

	ensureMsgf(LastFrame.ActiveStates.Num() >= FirstNewStateIndex, TEXT("ActiveTasksStatus won't be in sync with the amount of states."));
	LastFrame.ActiveStates.SetNum(FirstNewStateIndex);

	// Existing state's data is safe to access during select.
	LastFrame.NumCurrentlyActiveStates = static_cast<uint8>(LastFrame.ActiveStates.Num());

	FStateSelectionResult InitialSelection;

	if (Fallback == EStateTreeSelectionFallback::NextSelectableSibling)
	{
		InitialSelection = OutSelectionResult;
	}
	
	// We take copy of the last frame and assign it later, as SelectStateInternal() might change the array and invalidate the pointer.
	const FStateTreeExecutionFrame* CurrentParentFrame = LastFrameIndex > 0 ? &OutSelectionResult.GetSelectedFrames()[LastFrameIndex - 1] : nullptr;

	// Path from the first new state up to the NextState
	TConstArrayView<FStateTreeStateHandle> NewStatesPathToNextState(&PathToNextState[FirstNewStateIndex], PathToNextState.Num() - FirstNewStateIndex);

	if (SelectStateInternal(CurrentParentFrame, OutSelectionResult.GetSelectedFrames()[LastFrameIndex], CurrentFrameInActiveFrames, NewStatesPathToNextState, OutSelectionResult, TransitionEvent))
	{
		return true;
	}

	// Failed to Select Next State, handle fallback here
	// Return true on the first next sibling that gets selected successfully
	if (Fallback == EStateTreeSelectionFallback::NextSelectableSibling && PathToNextState.Num() >= 2)
	{
		const FStateTreeStateHandle Parent = PathToNextState.Last(1);
		if (Parent.IsValid())
		{
			const FCompactStateTreeState& ParentState = CurrentFrame.StateTree->States[Parent.Index];

			uint16 ChildState = CurrentFrame.StateTree->States[NextState.Index].GetNextSibling();
			for (; ChildState < ParentState.ChildrenEnd; ChildState = CurrentFrame.StateTree->States[ChildState].GetNextSibling())
			{
				FStateTreeStateHandle ChildStateHandle = FStateTreeStateHandle(ChildState);

				// Start selection from blank slate.
				OutSelectionResult = InitialSelection;
	
				// We take copy of the last frame and assign it later, as SelectStateInternal() might change the array and invalidate the pointer.
				CurrentParentFrame = LastFrameIndex > 0 ? &OutSelectionResult.GetSelectedFrames()[LastFrameIndex - 1] : nullptr; 
				if (SelectStateInternal(CurrentParentFrame, OutSelectionResult.GetSelectedFrames()[LastFrameIndex], CurrentFrameInActiveFrames, {ChildStateHandle}, OutSelectionResult))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FStateTreeExecutionContext::SelectStateInternal(
	const FStateTreeExecutionFrame* CurrentParentFrame,
	FStateTreeExecutionFrame& CurrentFrame,
	const FStateTreeExecutionFrame* CurrentFrameInActiveFrames,
	TConstArrayView<FStateTreeStateHandle> PathToNextState,
	FStateSelectionResult& OutSelectionResult,
	const FStateTreeSharedEvent* TransitionEvent)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_SelectState);

	const FStateTreeExecutionState& Exec = GetExecState();

	check(!PathToNextState.IsEmpty());
	const FStateTreeStateHandle NextStateHandle = PathToNextState[0];
	if (!NextStateHandle.IsValid())
	{
		// Trying to select non-existing state.
		STATETREE_LOG(Error, TEXT("%hs: Trying to select invalid state from '%s'.  '%s' using StateTree '%s'."),
            __FUNCTION__, *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
		return false;
	}

	FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
	FCurrentlyProcessedStateScope NextStateScope(*this, NextStateHandle);
	FCurrentFrameStateSelectionEventsScope CapturedEventsScope(*this, OutSelectionResult.GetFramesStateSelectionEvents().Last());

	const UStateTree* CurrentStateTree = CurrentFrame.StateTree;
	const FCompactStateTreeState& NextState = CurrentStateTree->States[NextStateHandle.Index];

	if (NextState.bEnabled == false)
	{
		// Do not select disabled state
		STATETREE_LOG(VeryVerbose, TEXT("%hs: Ignoring disabled state '%s'.  '%s' using StateTree '%s'."),
			__FUNCTION__, *GetSafeStateName(CurrentFrame, NextStateHandle), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
		return false;
	}

	UE_STATETREE_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EStateTreeUpdatePhase::StateSelection);

	// The state cannot be directly selected.
	if (NextState.SelectionBehavior == EStateTreeStateSelectionBehavior::None)
	{
		return false;
	}

	const UStateTree* NextLinkedStateAsset = NextState.LinkedAsset;

	// Look up linked state overrides
	const FInstancedPropertyBag* NextLinkedStateParameterOverride = nullptr;
	if (NextState.Type == EStateTreeStateType::LinkedAsset)
	{
		if (const FStateTreeReference* Override = GetLinkedStateTreeOverrideForTag(NextState.Tag))
		{
			NextLinkedStateAsset = Override->GetStateTree();
			NextLinkedStateParameterOverride = &Override->GetParameters();

			STATETREE_LOG(VeryVerbose, TEXT("%hs: In state '%s', overriding linked asset '%s' with '%s'. '%s' using StateTree '%s'."),
					__FUNCTION__, *GetSafeStateName(CurrentFrame, NextStateHandle),
					*GetFullNameSafe(NextState.LinkedAsset), *GetFullNameSafe(NextLinkedStateAsset),
					*GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
		}
	}

	if (NextState.ParameterDataHandle.IsValid())
	{
		// Instantiate state parameters if not done yet.
		FStateTreeDataView NextStateParametersView = GetDataViewOrTemporary(CurrentParentFrame, CurrentFrame, NextState.ParameterDataHandle);
		if (!NextStateParametersView.IsValid())
		{
			// Allocate temporary instance for parameters if the state has params.
			// The subtree state selection below assumes that this creates always a valid temporary, we'll create the temp data even if parameters are empty.
			// @todo: Empty params is valid and common case, we should not require to create empty parameters data (this needs to be handle in compiler and UpdateInstanceData too).
			if (NextLinkedStateParameterOverride)
			{
				// Create from an override.
				FStateTreeDataView TempStateParametersView = AddTemporaryInstance(CurrentFrame, FStateTreeIndex16::Invalid, NextState.ParameterDataHandle, FConstStructView(TBaseStructure<FCompactStateTreeParameters>::Get()));
				check(TempStateParametersView.IsValid());
				FCompactStateTreeParameters& StateParams = TempStateParametersView.GetMutable<FCompactStateTreeParameters>();
				StateParams.Parameters = *NextLinkedStateParameterOverride;
				NextStateParametersView = FStateTreeDataView(StateParams.Parameters.GetMutableValue());
			}
			else
			{
				// Create from template in the asset.
				const FConstStructView DefaultStateParamsInstanceData = CurrentFrame.StateTree->DefaultInstanceData.GetStruct(NextState.ParameterTemplateIndex.Get());
				FStateTreeDataView TempStateParametersView = AddTemporaryInstance(CurrentFrame, FStateTreeIndex16::Invalid, NextState.ParameterDataHandle, DefaultStateParamsInstanceData);
				check(TempStateParametersView.IsValid());
				FCompactStateTreeParameters& StateParams = TempStateParametersView.GetMutable<FCompactStateTreeParameters>();
				NextStateParametersView = FStateTreeDataView(StateParams.Parameters.GetMutableValue());
			}
		}

		// Copy parameters if needed
		if (NextStateParametersView.IsValid()
			&& NextState.ParameterDataHandle.IsValid()
			&& NextState.ParameterBindingsBatch.IsValid())
		{
			// Note: the parameters are for the current (linked) state, stored in current frame.
			// The copy can fail, if the overridden parameters do not match, this is by design.
			CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, NextStateParametersView, NextState.ParameterBindingsBatch);
		}
	}

	const bool bIsDestinationState = PathToNextState.Num() < 2;
	const bool bShouldPrerequisitesBeChecked = bIsDestinationState || NextState.bCheckPrerequisitesWhenActivatingChildDirectly;
	TArray<const FStateTreeSharedEvent*, TInlineAllocator<FStateTreeEventQueue::MaxActiveEvents>> StateSelectionEvents;
	if (NextState.EventDataIndex.IsValid())
	{
		check(NextState.RequiredEventToEnter.IsValid());

		// Use the same event as performed transition unless it didn't lead to this state as only state selected by the transition should get it's event.
		if (TransitionEvent && TransitionEvent->IsValid() && bIsDestinationState)
		{
			if (NextState.RequiredEventToEnter.DoesEventMatchDesc(*TransitionEvent->Get()))
			{
				StateSelectionEvents.Emplace(TransitionEvent);
			}
		}
		else
		{
			TArrayView<FStateTreeSharedEvent> EventsQueue = GetMutableEventsToProcessView();
			for (FStateTreeSharedEvent& Event : EventsQueue)
			{
				check(Event.IsValid());
				if (NextState.RequiredEventToEnter.DoesEventMatchDesc(*Event))
				{
					StateSelectionEvents.Emplace(&Event);
				}
			}

			// Couldn't find matching state's event, but it's marked as not required. Adding an empty event which allows us to continue the state selection.
			if (!bShouldPrerequisitesBeChecked && StateSelectionEvents.IsEmpty())
			{
				StateSelectionEvents.Emplace();
			}
		}

		if (StateSelectionEvents.IsEmpty())
		{
			return false;
		}
	}
	else
	{
		StateSelectionEvents.Emplace();
	}

	// Activate/Push a new state
	if (!CurrentFrame.ActiveStates.Push(NextStateHandle, UE::StateTree::FActiveStateID(Storage.GenerateUniqueId())))
	{
		STATETREE_LOG(Error, TEXT("%hs: Reached max execution depth when trying to select state %s from '%s'.  '%s' using StateTree '%s'."),
			__FUNCTION__, *GetSafeStateName(CurrentFrame, NextStateHandle), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
		return false;
	}
	CurrentFrame.ActiveTasksStatus.Push(NextState);

	// Check if we're still tracking on the current active frame and state.
	// If we are, update the NumCurrentlyActiveStates to indicate that this state's instance data can be accessed. 
	const uint8 PrevNumCurrentlyActiveStates = CurrentFrame.NumCurrentlyActiveStates; 
	if (CurrentFrame.ActiveInstanceIndexBase.IsValid()
		&& CurrentFrameInActiveFrames)
	{
		const int32 CurrentStateIndex = CurrentFrame.ActiveStates.Num() - 1;
		const FStateTreeStateHandle MatchingActiveHandle = CurrentFrameInActiveFrames->ActiveStates.GetStateSafe(CurrentStateIndex);
		if (MatchingActiveHandle == NextStateHandle)
		{
			CurrentFrame.NumCurrentlyActiveStates = static_cast<uint8>(CurrentFrame.ActiveStates.Num());
		}
	}

	bool bSucceededToSelectState = false;

	for (const FStateTreeSharedEvent* StateSelectionEvent : StateSelectionEvents)
	{
		if (StateSelectionEvent)
		{
			CurrentlyProcessedStateSelectionEvents->Events[NextState.Depth] = *StateSelectionEvent;
		}
		
		if (bShouldPrerequisitesBeChecked)
		{
			// Check that the state can be entered
			UE_STATETREE_DEBUG_ENTER_PHASE(this, EStateTreeUpdatePhase::EnterConditions);
			const bool bEnterConditionsPassed = TestAllConditions(CurrentParentFrame, CurrentFrame, NextState.EnterConditionsBegin, NextState.EnterConditionsNum);
			UE_STATETREE_DEBUG_EXIT_PHASE(this, EStateTreeUpdatePhase::EnterConditions);

			if (!bEnterConditionsPassed)
			{
				continue;
			}
		}
		
		if (!bIsDestinationState)
		{
			// Next child state is already known. Passing TransitionEvent further so state selected directly by transition can use it.
			if (SelectStateInternal(CurrentParentFrame, CurrentFrame, CurrentFrameInActiveFrames, PathToNextState.Mid(1), OutSelectionResult, TransitionEvent))
			{
				bSucceededToSelectState = true;
				break;
			}
		}
		else if (NextState.Type == EStateTreeStateType::Linked)
		{
			if (NextState.LinkedState.IsValid())
			{
				if (OutSelectionResult.IsFull())
				{
					STATETREE_LOG(Error, TEXT("%hs: Reached max execution depth when trying to select state %s from '%s'.  '%s' using StateTree '%s'."),
						__FUNCTION__, *GetSafeStateName(CurrentFrame, NextStateHandle), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
					break;
				}

				FStateTreeExecutionFrame NewFrame;
				NewFrame.StateTree = CurrentFrame.StateTree;
				NewFrame.RootState = NextState.LinkedState;
				NewFrame.ExternalDataBaseIndex = CurrentFrame.ExternalDataBaseIndex;

				// Check and prevent recursion.
				const bool bNewFrameAlreadySelected = OutSelectionResult.GetSelectedFrames().ContainsByPredicate([&NewFrame](const FStateTreeExecutionFrame& Frame) {
					return Frame.IsSameFrame(NewFrame);
				});
				
				if (bNewFrameAlreadySelected)
				{
					STATETREE_LOG(Error, TEXT("%hs: Trying to recursively enter subtree '%s' from '%s'.  '%s' using StateTree '%s'."),
						__FUNCTION__, *GetSafeStateName(NewFrame, NewFrame.RootState), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
					break;
				}

				// If the Frame already exists, copy instance indices so that conditions that rely on active states work correctly.
				const FStateTreeExecutionFrame* ExistingFrame = Exec.ActiveFrames.FindByPredicate(
					[StateTree = NewFrame.StateTree, RootState = NewFrame.RootState](const FStateTreeExecutionFrame& Frame)
					{
						return Frame.StateTree == StateTree && Frame.RootState == RootState;
					});
				if (ExistingFrame)
				{
					NewFrame.FrameID = ExistingFrame->FrameID;
					NewFrame.ActiveTasksStatus = ExistingFrame->ActiveTasksStatus;
					NewFrame.ActiveInstanceIndexBase = ExistingFrame->ActiveInstanceIndexBase;
					NewFrame.GlobalInstanceIndexBase = ExistingFrame->GlobalInstanceIndexBase;
					NewFrame.StateParameterDataHandle = ExistingFrame->StateParameterDataHandle;
					NewFrame.GlobalParameterDataHandle = ExistingFrame->GlobalParameterDataHandle;
				}
				else
				{
					NewFrame.FrameID = UE::StateTree::FActiveFrameID(Storage.GenerateUniqueId());

					const FCompactStateTreeFrame* FrameInfo = NewFrame.StateTree->GetFrameFromHandle(NewFrame.RootState);
					ensureAlwaysMsgf(FrameInfo, TEXT("The compiled data is invalid. It should contains the information for the new root frame."));
					NewFrame.ActiveTasksStatus = FrameInfo ? FStateTreeTasksCompletionStatus(*FrameInfo) : FStateTreeTasksCompletionStatus();

					// Since the StateTree is the same, we can access the global tasks of CurrentFrame, if they are initialized.
					NewFrame.GlobalParameterDataHandle = CurrentFrame.GlobalParameterDataHandle;
					NewFrame.GlobalInstanceIndexBase = CurrentFrame.GlobalInstanceIndexBase;
					NewFrame.StateParameterDataHandle = NextState.ParameterDataHandle; // Temporary allocated earlier if did not exists.
				}

				OutSelectionResult.PushFrame(NewFrame);

				// If State is linked, proceed to the linked state.
				if (SelectStateInternal(&CurrentFrame, OutSelectionResult.GetSelectedFrames().Last(), ExistingFrame, {NewFrame.RootState}, OutSelectionResult))
				{
					bSucceededToSelectState = true;
					break;
				}
				
				OutSelectionResult.PopFrame();
			}
			else
			{
				STATETREE_LOG(Warning, TEXT("%hs: Trying to enter invalid linked subtree from '%s'.  '%s' using StateTree '%s'."),
					__FUNCTION__, *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
			}
		}
		else if (NextState.Type == EStateTreeStateType::LinkedAsset)
		{
			if (NextLinkedStateAsset == nullptr || NextLinkedStateAsset->States.Num() == 0)
			{
				break;
			}

			if (OutSelectionResult.IsFull())
			{
				STATETREE_LOG(Error, TEXT("%hs: Reached max execution depth when trying to select state %s from '%s'.  '%s' using StateTree '%s'."),
					__FUNCTION__, *GetSafeStateName(CurrentFrame, NextStateHandle), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
				break;
			}

			// The linked state tree should have compatible context requirements.
			if (!NextLinkedStateAsset->HasCompatibleContextData(RootStateTree)
				|| NextLinkedStateAsset->GetSchema()->GetClass() != RootStateTree.GetSchema()->GetClass())
			{
				STATETREE_LOG(Error, TEXT("%hs: The linked State Tree '%s' does not have compatible schema, trying to select state %s from '%s'.  '%s' using StateTree '%s'."),
					__FUNCTION__, *GetFullNameSafe(NextLinkedStateAsset), *GetSafeStateName(CurrentFrame, NextStateHandle), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
				break;
			}
				
			FStateTreeExecutionFrame NewFrame;
			NewFrame.StateTree = NextLinkedStateAsset;
			NewFrame.RootState = FStateTreeStateHandle::Root;
			NewFrame.bIsGlobalFrame = true;

			// Check and prevent recursion.
			const bool bNewFrameAlreadySelected = OutSelectionResult.GetSelectedFrames().ContainsByPredicate([&NewFrame](const FStateTreeExecutionFrame& Frame) {
				return Frame.IsSameFrame(NewFrame);
			});
				
			if (bNewFrameAlreadySelected)
			{
				STATETREE_LOG(Error, TEXT("%hs: Trying to recursively enter subtree '%s' from '%s'.  '%s' using StateTree '%s'."),
					__FUNCTION__, *GetSafeStateName(NewFrame, NewFrame.RootState), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
				break;
			}

			// If the Frame already exists, copy instance indices so that conditions that rely on active states work correctly.
			const FStateTreeExecutionFrame* ExistingFrame = Exec.ActiveFrames.FindByPredicate(
				[StateTree = NewFrame.StateTree, RootState = NewFrame.RootState](const FStateTreeExecutionFrame& Frame)
				{
					return Frame.StateTree == StateTree && Frame.RootState == RootState;
				});

			bool bStartedTemporaryEvaluatorsAndGlobalTasks = false;
			if (ExistingFrame)
			{
				NewFrame.FrameID = ExistingFrame->FrameID;
				NewFrame.ActiveTasksStatus = ExistingFrame->ActiveTasksStatus;
				NewFrame.ActiveInstanceIndexBase = ExistingFrame->ActiveInstanceIndexBase;
				NewFrame.GlobalInstanceIndexBase = ExistingFrame->GlobalInstanceIndexBase;
				NewFrame.StateParameterDataHandle = ExistingFrame->StateParameterDataHandle;
				NewFrame.GlobalParameterDataHandle = ExistingFrame->GlobalParameterDataHandle;
				NewFrame.ExternalDataBaseIndex = ExistingFrame->ExternalDataBaseIndex;
			}
			else
			{
				NewFrame.FrameID = UE::StateTree::FActiveFrameID(Storage.GenerateUniqueId());
				const FCompactStateTreeFrame* FrameInfo = NewFrame.StateTree->GetFrameFromHandle(NewFrame.RootState);
				ensureMsgf(FrameInfo, TEXT("The compiled data is invalid. It should contains the information for the root frame."));
				NewFrame.ActiveTasksStatus = FrameInfo ? FStateTreeTasksCompletionStatus(*FrameInfo) : FStateTreeTasksCompletionStatus();

				// Pass the linked state's parameters as global parameters to the linked asset.
				NewFrame.GlobalParameterDataHandle = NextState.ParameterDataHandle;

				// Collect external data if needed
				NewFrame.ExternalDataBaseIndex = CollectExternalData(NewFrame.StateTree);
				if (!NewFrame.ExternalDataBaseIndex.IsValid())
				{
					STATETREE_LOG(VeryVerbose, TEXT("%hs: Cannot select state '%s' because failed to collect external data for nested tree '%s'.  '%s' using StateTree '%s'."),
						__FUNCTION__, *GetSafeStateName(CurrentFrame, NextStateHandle), *GetFullNameSafe(NewFrame.StateTree), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
					break;
				}
					
				// The state parameters will be from the root state.
				const FCompactStateTreeState& RootState = NewFrame.StateTree->States[NewFrame.RootState.Index];
				NewFrame.StateParameterDataHandle = RootState.ParameterDataHandle;

				// Start global tasks and evaluators temporarily, so that their data is available already during select.
				if (StartTemporaryEvaluatorsAndGlobalTasks(&CurrentFrame, NewFrame) != EStateTreeRunStatus::Running)
				{
					STATETREE_LOG(VeryVerbose, TEXT("%hs: Cannot select state '%s' because cannot start nested tree's '%s' global tasks and evaluators.  '%s' using StateTree '%s'."),
						__FUNCTION__, *GetSafeStateName(CurrentFrame, NextStateHandle), *GetFullNameSafe(NewFrame.StateTree), *GetNameSafe(&Owner), *GetFullNameSafe(CurrentFrame.StateTree));
					
					StopTemporaryEvaluatorsAndGlobalTasks(nullptr, NewFrame);
					GetExecState().DelegateActiveListeners.RemoveAll(NewFrame.FrameID);

					break;
				}

				bStartedTemporaryEvaluatorsAndGlobalTasks = true;
			}
				
			OutSelectionResult.PushFrame(NewFrame);

			// If State is linked, proceed to the linked state.
			if (SelectStateInternal(&CurrentFrame, OutSelectionResult.GetSelectedFrames().Last(), ExistingFrame, {NewFrame.RootState}, OutSelectionResult))
			{
				bSucceededToSelectState = true;
				break;
			}

			if (bStartedTemporaryEvaluatorsAndGlobalTasks)
			{
				StopTemporaryEvaluatorsAndGlobalTasks(&CurrentFrame, NewFrame);
				GetExecState().DelegateActiveListeners.RemoveAll(NewFrame.FrameID);
			}
				
			OutSelectionResult.PopFrame();
		}
		else if (NextState.SelectionBehavior == EStateTreeStateSelectionBehavior::TryEnterState)
		{
			// Select this state.
			UE_STATETREE_DEBUG_STATE_EVENT(this, NextStateHandle, EStateTreeTraceEventType::OnStateSelected);
			bSucceededToSelectState = true;
			break;
		}
		else if (NextState.SelectionBehavior == EStateTreeStateSelectionBehavior::TryFollowTransitions)
		{
			UE_STATETREE_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EStateTreeUpdatePhase::TrySelectBehavior);

			EStateTreeTransitionPriority CurrentPriority = EStateTreeTransitionPriority::None;

			for (uint8 i = 0; i < NextState.TransitionsNum; i++)
			{
				const int16 TransitionIndex = NextState.TransitionsBegin + i;
				const FCompactStateTransition& Transition = CurrentStateTree->Transitions[TransitionIndex];

				// Skip disabled transitions
				if (Transition.bTransitionEnabled == false)
				{
					continue;
				}

				// No need to test the transition if same or higher priority transition has already been processed.
				if (Transition.Priority <= CurrentPriority)
				{
					continue;
				}

				// Skip completion transitions
				if (EnumHasAnyFlags(Transition.Trigger, EStateTreeTransitionTrigger::OnStateCompleted))
				{
					continue;
				}

				// Cannot follow transitions with delay.
				if (Transition.HasDelay())
				{
					continue;
				}

				// Try to prevent (infinite) loops in the selection.
				if (CurrentFrame.ActiveStates.Contains(Transition.State))
				{
					STATETREE_LOG(Error, TEXT("%hs: Loop detected when trying to select state %s from '%s'. Prior states: %s.  '%s' using StateTree '%s'.")
						, __FUNCTION__
						, *GetSafeStateName(CurrentFrame, NextStateHandle)
						, *GetStateStatusString(Exec)
						, *DebugGetStatePath(OutSelectionResult.GetSelectedFrames(), &CurrentFrame)
						, *GetNameSafe(&Owner)
						, *GetFullNameSafe(CurrentFrame.StateTree));
					continue;
				}

				TArray<const FStateTreeSharedEvent*, TInlineAllocator<FStateTreeEventQueue::MaxActiveEvents>> SelectedStateTransitionEvents;
				if (Transition.Trigger == EStateTreeTransitionTrigger::OnEvent)
				{
					check(Transition.RequiredEvent.IsValid());

					if (StateSelectionEvent)
					{
						SelectedStateTransitionEvents.Emplace(StateSelectionEvent);
					}
					else
					{
						TArrayView<FStateTreeSharedEvent> EventsQueue = GetMutableEventsToProcessView();
						for (FStateTreeSharedEvent& Event : EventsQueue)
						{
							check(Event.IsValid());
							if (Transition.RequiredEvent.DoesEventMatchDesc(*Event))
							{
								SelectedStateTransitionEvents.Emplace(&Event);
							}
						}
					}
				}
				else if (EnumHasAnyFlags(Transition.Trigger, EStateTreeTransitionTrigger::OnTick))
				{
					SelectedStateTransitionEvents.Emplace();
				}
				else if (EnumHasAnyFlags(Transition.Trigger, EStateTreeTransitionTrigger::OnDelegate))
				{
					if (Storage.IsDelegateBroadcasted(Transition.RequiredDelegateDispatcher))
					{
						SelectedStateTransitionEvents.Emplace();
					}
				}
				else
				{
					ensureMsgf(false, TEXT("Missing a transition trigger type."));
				}

				for (const FStateTreeSharedEvent* SelectedStateTransitionEvent : SelectedStateTransitionEvents)
				{
					bool bTransitionConditionsPassed = false;
					{
						FCurrentlyProcessedTransitionEventScope TransitionEventScope(*this, SelectedStateTransitionEvent ? SelectedStateTransitionEvent->Get() : nullptr);

						UE_STATETREE_DEBUG_TRANSITION_EVENT(this, FStateTreeTransitionSource(CurrentFrame.StateTree, FStateTreeIndex16(TransitionIndex), Transition.State, Transition.Priority), EStateTreeTraceEventType::OnEvaluating);

						UE_STATETREE_DEBUG_SCOPED_PHASE(this, EStateTreeUpdatePhase::TransitionConditions);

						bTransitionConditionsPassed = TestAllConditions(CurrentParentFrame, CurrentFrame, Transition.ConditionsBegin, Transition.ConditionsNum);
					}

					if (bTransitionConditionsPassed)
					{
						// Using SelectState() instead of SelectStateInternal to treat the transitions the same way as regular transitions,
						// e.g. it may jump to a completely different branch.
						FStateSelectionResult StateSelectionResult;
						if (SelectState(CurrentFrame, Transition.State, StateSelectionResult, SelectedStateTransitionEvent, Transition.Fallback))
						{
							// Selection succeeded.
							// Cannot break yet because higher priority transitions may override the selection. 
							OutSelectionResult = StateSelectionResult;
							CurrentPriority = Transition.Priority;
							break;
						}
					}
				}
			}

			if (CurrentPriority != EStateTreeTransitionPriority::None)
			{
				bSucceededToSelectState = true;
				break;
			}
		}
		else if (NextState.SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder)
		{
			if (NextState.HasChildren())
			{
				UE_STATETREE_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EStateTreeUpdatePhase::TrySelectBehavior);

				// If the state has children, proceed to select children.
				for (uint16 ChildState = NextState.ChildrenBegin; ChildState < NextState.ChildrenEnd; ChildState = CurrentStateTree->States[ChildState].GetNextSibling())
				{
					if (SelectStateInternal(CurrentParentFrame, CurrentFrame, CurrentFrameInActiveFrames, {FStateTreeStateHandle(ChildState)}, OutSelectionResult))
					{
						// Selection succeeded
						bSucceededToSelectState = true;
						break;
					}
				}

				if (bSucceededToSelectState)
				{
					break;
				}
			}
			else
			{
				// Select this state (For backwards compatibility)
				UE_STATETREE_DEBUG_STATE_EVENT(this, NextStateHandle, EStateTreeTraceEventType::OnStateSelected);
				bSucceededToSelectState = true;
				break;
			}
		}
		else if (NextState.SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandom)
		{
			if (NextState.HasChildren())
			{
				UE_STATETREE_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EStateTreeUpdatePhase::TrySelectBehavior);
				
				TArray<uint16, TInlineAllocator<8>> NextLevelChildStates;
				for (uint16 ChildState = NextState.ChildrenBegin; ChildState < NextState.ChildrenEnd; ChildState = CurrentStateTree->States[ChildState].GetNextSibling())
				{
					NextLevelChildStates.Push(ChildState);
				}

				while (!NextLevelChildStates.IsEmpty())
				{
					const int32 ChildStateIndex = Exec.RandomStream.RandRange(0, NextLevelChildStates.Num() - 1);
					if (SelectStateInternal(CurrentParentFrame, CurrentFrame, CurrentFrameInActiveFrames,
					                        {FStateTreeStateHandle(NextLevelChildStates[ChildStateIndex])}, OutSelectionResult))
					{
						// Selection succeeded
						bSucceededToSelectState = true;
						break;
					}

					constexpr EAllowShrinking AllowShrinking = EAllowShrinking::No;
					NextLevelChildStates.RemoveAtSwap(ChildStateIndex, AllowShrinking);
				}

				if (bSucceededToSelectState)
				{
					break;
				}
			}
			else
			{
				// Select this state (For backwards compatibility)
				UE_STATETREE_DEBUG_STATE_EVENT(this, NextStateHandle, EStateTreeTraceEventType::OnStateSelected);
				bSucceededToSelectState = true;
				break;
			}
		}
		else if (NextState.SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility)
		{
			if (NextState.HasChildren())
			{
				UE_STATETREE_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EStateTreeUpdatePhase::TrySelectBehavior);

				TArray<uint16, TInlineAllocator<8>> NextLevelChildStates;
				for (uint16 ChildState = NextState.ChildrenBegin; ChildState < NextState.ChildrenEnd; ChildState = CurrentStateTree->States[ChildState].GetNextSibling())
				{
					NextLevelChildStates.Push(ChildState);
				}

				while (!NextLevelChildStates.IsEmpty())
				{
					//Find one with highest score in the remaining candidates
					float HighestScore = -std::numeric_limits<float>::infinity();;
					uint16 StateIndexWithHighestScore = FStateTreeStateHandle::InvalidIndex;
					int32 ArrayIndexWithHighestScore = INDEX_NONE;
					for (int32 Index = 0; Index < NextLevelChildStates.Num(); ++Index)
					{
						const uint16 CurrentStateIndex = NextLevelChildStates[Index];
						const FCompactStateTreeState& CurrentState = CurrentStateTree->States[CurrentStateIndex];
						const float Score = EvaluateUtility(CurrentParentFrame, CurrentFrame, CurrentState.UtilityConsiderationsBegin, CurrentState.UtilityConsiderationsNum, CurrentState.Weight);
						if (Score > HighestScore)
						{
							HighestScore = Score;
							StateIndexWithHighestScore = CurrentStateIndex;
							ArrayIndexWithHighestScore = Index;
						}
					}

					if (FStateTreeStateHandle::IsValidIndex(StateIndexWithHighestScore))
					{
						if (SelectStateInternal(CurrentParentFrame, CurrentFrame, CurrentFrameInActiveFrames, { FStateTreeStateHandle(StateIndexWithHighestScore) }, OutSelectionResult))
						{
							// Selection succeeded
							bSucceededToSelectState = true;
							break;
						}
						
						// Disqualify the state we failed to enter
						NextLevelChildStates.RemoveAtSwap(ArrayIndexWithHighestScore, EAllowShrinking::No);
					}
					else
					{
						// No states in array were valid
						break;
					}
				}

				if (bSucceededToSelectState)
				{
					break;
				}
			}
			else
			{
				// Select this state (For backwards compatibility)
				UE_STATETREE_DEBUG_STATE_EVENT(this, NextStateHandle, EStateTreeTraceEventType::OnStateSelected);
				bSucceededToSelectState = true;
				break;
			}
		}
		else if (NextState.SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility)
		{
			if (NextState.HasChildren())
			{
				TArray<TTuple<uint16, float>, TInlineAllocator<8>> NextLevelChildStates;
				float TotalScore = .0f;
				for (uint16 CurrentStateIndex = NextState.ChildrenBegin; CurrentStateIndex < NextState.ChildrenEnd; CurrentStateIndex = CurrentStateTree->States[CurrentStateIndex].GetNextSibling())
				{
					const FCompactStateTreeState& CurrentState = CurrentStateTree->States[CurrentStateIndex];
					const float CurrentStateScore = EvaluateUtility(CurrentParentFrame, CurrentFrame, CurrentState.UtilityConsiderationsBegin, CurrentState.UtilityConsiderationsNum, CurrentState.Weight);
					NextLevelChildStates.Emplace(CurrentStateIndex, CurrentStateScore);
					TotalScore += CurrentStateScore;
				}

				while (!NextLevelChildStates.IsEmpty())
				{
					const float RandomScore = Exec.RandomStream.FRand() * TotalScore;
					float AccumulatedScore = .0f;
					for (int32 Index = 0; Index < NextLevelChildStates.Num(); ++Index)
					{
						const TTuple<uint16, float>& StateScorePair = NextLevelChildStates[Index];
						const uint16 StateIndex = StateScorePair.Key;
						const float StateScore = StateScorePair.Value;
						AccumulatedScore += StateScore;

						if (RandomScore < AccumulatedScore || (Index == (NextLevelChildStates.Num() - 1)))
						{
							// States with zero possibility won't be selected
							if (StateScore != 0.f && SelectStateInternal(CurrentParentFrame, CurrentFrame, CurrentFrameInActiveFrames, { FStateTreeStateHandle(StateIndex) }, OutSelectionResult))
							{
								// Selection succeeded
								bSucceededToSelectState = true;
								break;
							}

							//Disqualify the state we failed to enter, and restart the loop
							TotalScore -= StateScore;
							constexpr EAllowShrinking AllowShrinking = EAllowShrinking::No;
							NextLevelChildStates.RemoveAtSwap(Index, AllowShrinking);

							break;
						}
					}

					if (bSucceededToSelectState)
					{
						break;
					}
				}

				if (bSucceededToSelectState)
				{
					break;
				}
			}
			else
			{
				// Select this state (For backwards compatibility)
				UE_STATETREE_DEBUG_STATE_EVENT(this, NextStateHandle, EStateTreeTraceEventType::OnStateSelected);
				bSucceededToSelectState = true;
				break;
			}
		}
	}

	if (!bSucceededToSelectState)
	{
		// State could not be selected, restore.
		CurrentFrame.NumCurrentlyActiveStates = PrevNumCurrentlyActiveStates;
		CurrentFrame.ActiveStates.Pop();
	}

	return bSucceededToSelectState;
}

FString FStateTreeExecutionContext::GetSafeStateName(const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeStateHandle State) const
{
	if (State == FStateTreeStateHandle::Invalid)
	{
		return TEXT("(State Invalid)");
	}
	else if (State == FStateTreeStateHandle::Succeeded)
	{
		return TEXT("(State Succeeded)");
	}
	else if (State == FStateTreeStateHandle::Failed)
	{
		return TEXT("(State Failed)");
	}
	else if (CurrentFrame.StateTree && CurrentFrame.StateTree->States.IsValidIndex(State.Index))
	{
		return *CurrentFrame.StateTree->States[State.Index].Name.ToString();
	}
	return TEXT("(Unknown)");
}

FString FStateTreeExecutionContext::DebugGetStatePath(TConstArrayView<FStateTreeExecutionFrame> ActiveFrames, const FStateTreeExecutionFrame* CurrentFrame, const int32 ActiveStateIndex) const
{
	FString StatePath;
	const UStateTree* LastStateTree = &RootStateTree;
		
	for (const FStateTreeExecutionFrame& Frame : ActiveFrames)
	{
		if (!ensure(Frame.StateTree))
		{
			return StatePath;
		}

		// If requested up the active state, clamp count.
		int32 Num = Frame.ActiveStates.Num();
		if (CurrentFrame == &Frame && Frame.ActiveStates.IsValidIndex(ActiveStateIndex))
		{
			Num = ActiveStateIndex + 1;
		}

		if (Frame.StateTree != LastStateTree)
		{
			StatePath.Appendf(TEXT("[%s]"), *GetNameSafe(Frame.StateTree));
			LastStateTree = Frame.StateTree;
		}
		
		for (int32 i = 0; i < Num; i++)
		{
			const FCompactStateTreeState& State = Frame.StateTree->States[Frame.ActiveStates[i].Index];
			StatePath.Appendf(TEXT("%s%s"), i == 0 ? TEXT("") : TEXT("."), *State.Name.ToString());
		}
	}
		
	return StatePath;
}

FString FStateTreeExecutionContext::GetStateStatusString(const FStateTreeExecutionState& ExecState) const
{
	if (ExecState.TreeRunStatus != EStateTreeRunStatus::Running)
	{
		return TEXT("--:") + UEnum::GetDisplayValueAsText(ExecState.LastTickStatus).ToString();
	}
	return GetSafeStateName(ExecState.ActiveFrames.Last(), ExecState.ActiveFrames.Last().ActiveStates.Last()) + TEXT(":") + UEnum::GetDisplayValueAsText(ExecState.LastTickStatus).ToString();
}

// Deprecated
FString FStateTreeExecutionContext::GetInstanceDescription() const
{
	return GetInstanceDescriptionInternal();
}


#undef STATETREE_LOG
#undef STATETREE_CLOG
#undef UE_STATETREE_ENSURE_COMPLETED_STATE_RUN_STATUS
