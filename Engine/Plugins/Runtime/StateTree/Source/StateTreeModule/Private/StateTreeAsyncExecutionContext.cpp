// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"

namespace UE::StateTree::Async
{
	const FStateTreeNodeBase& FActivePathInfo::GetNode() const
	{
		check(IsValid() && Frame->StateTree && NodeIndex.IsValid());

		return *(Frame->StateTree->GetNode(NodeIndex.AsInt32()).GetPtr<const FStateTreeNodeBase>());
	}
}

template <bool bWithWriteAccess>
TStateTreeStrongExecutionContext<bWithWriteAccess>::TStateTreeStrongExecutionContext(const FStateTreeWeakExecutionContext& WeakContext)
	: Owner(WeakContext.Owner.Pin()),
	StateTree(WeakContext.StateTree.Pin()),
	Storage(WeakContext.Storage.Pin()),
	FrameID(WeakContext.FrameID),
	StateID(WeakContext.StateID),
	NodeIndex(WeakContext.NodeIndex)
{
	if (Owner && StateTree && Storage)
	{
		bAccessAcquired = true;
		if constexpr (bWithWriteAccess)
		{
			Storage->AcquireWriteAccess();
		}
		else
		{
			Storage->AcquireReadAccess();
		}
	}
}

template <bool bWithWriteAccess>
TStateTreeStrongExecutionContext<bWithWriteAccess>::~TStateTreeStrongExecutionContext()
{
	if (bAccessAcquired)
	{
		check(IsValidInstanceStorage());

		if constexpr (bWithWriteAccess)
		{
			Storage->ReleaseWriteAccess();
		}
		else
		{
			Storage->ReleaseReadAccess();
		}
	}
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin) const
{
	static_assert(bWithWriteAccess);

	if (IsValid())
	{
		FStateTreeMinimalExecutionContext Context(Owner.Get(), StateTree.Get(), *Storage.Get());
		Context.SendEvent(Tag, Payload, Origin);
		return true;
	}

	return false;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::RequestTransition(FStateTreeStateHandle TargetState,
	EStateTreeTransitionPriority Priority, EStateTreeSelectionFallback Fallback) const
{
	static_assert(bWithWriteAccess);

	UE::StateTree::Async::FActivePathInfo ActivePath = GetActivePathInfo();
	if (ActivePath.IsValid())
	{
		FStateTreeTransitionRequest Request;
		Request.SourceFrameID = FrameID;
		Request.SourceStateID = StateID;
		Request.TargetState = TargetState;
		Request.Priority = Priority;
		Request.Fallback = Fallback;

		Storage->AddTransitionRequest(Owner.Get(), Request);
		ScheduleNextTick(Owner.Get(), StateTree.Get(), *Storage);

		return true;
	}

	return false;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::BroadcastDelegate(const FStateTreeDelegateDispatcher& Dispatcher)
const
{
	static_assert(bWithWriteAccess);

	if (!Dispatcher.IsValid())
	{
		// Nothings binds to the delegate, not an error.
		return true;
	}

	UE::StateTree::Async::FActivePathInfo ActivePath = GetActivePathInfo();
	if (ActivePath.IsValid())
	{
		FStateTreeExecutionState& Exec = Storage->GetMutableExecutionState();
		Exec.DelegateActiveListeners.BroadcastDelegate(Dispatcher, Exec);
		if (UE::StateTree::ExecutionContext::MarkDelegateAsBroadcasted(Dispatcher, *ActivePath.Frame, *Storage))
		{
			ScheduleNextTick(Owner.Get(), StateTree.Get(), *Storage);
		}

		return true;
	}

	return false;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::BindDelegate(const FStateTreeDelegateListener& Listener, FSimpleDelegate Delegate) const
{
	static_assert(bWithWriteAccess);

	if (!Listener.IsValid())
	{
		// Nothing binds to the delegate, not an error.
		return true;
	}

	UE::StateTree::Async::FActivePathInfo ActivePath = GetActivePathInfo();
	if (ActivePath.IsValid())
	{
		FStateTreeExecutionState& Exec = Storage->GetMutableExecutionState();
		if (ensure(ActivePath.Frame->StateTree))
		{
			Exec.DelegateActiveListeners.Add(Listener, MoveTemp(Delegate), FrameID, StateID, NodeIndex);
			return true;
		}
	}

	return false;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::UnbindDelegate(const FStateTreeDelegateListener& Listener) const
{
	static_assert(bWithWriteAccess);

	if (!Listener.IsValid())
	{
		// The listener is not bound to a dispatcher. It will never trigger the delegate. It is not an error.
		return true;
	}

	// Allow unbinding from context created outside the ExecContext loop
	if (IsValid())
	{
		FStateTreeExecutionState& Exec = Storage->GetMutableExecutionState();
		Exec.DelegateActiveListeners.Remove(Listener);

		return true;
	}

	return false;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::FinishTask(EStateTreeFinishTaskType FinishType) const
{
	static_assert(bWithWriteAccess);

	bool bSucceed = false;

	UE::StateTree::Async::FActivePathInfo ActivePath = GetActivePathInfo();
	if (ActivePath.IsValid())
	{
		FStateTreeExecutionState& Exec = Storage->GetMutableExecutionState();
		FStateTreeExecutionFrame* NonConstFrame = const_cast<FStateTreeExecutionFrame*>(ActivePath.Frame);
		if (ensure(NonConstFrame->StateTree))
		{
			using namespace UE::StateTree;

			const UStateTree* FrameStateTree = NonConstFrame->StateTree;

			const int32 AssetNodeIndex = ActivePath.NodeIndex.AsInt32();
			const int32 GlobalTaskBeginIndex = FrameStateTree->GlobalTasksBegin;
			const int32 GlobalTaskEndIndex = FrameStateTree->GlobalTasksBegin + FrameStateTree->GlobalTasksNum;
			const bool bIsGlobalTask = AssetNodeIndex >= GlobalTaskBeginIndex && AssetNodeIndex < GlobalTaskEndIndex;
			const ETaskCompletionStatus TaskStatus = ExecutionContext::CastToTaskStatus(FinishType);

			bool bCompleted = false;
			if (NonConstFrame->bIsGlobalFrame && bIsGlobalTask)
			{
				const int32 FrameTaskIndex = AssetNodeIndex - GlobalTaskBeginIndex;
				FTasksCompletionStatus GlobalTasksStatus = NonConstFrame->ActiveTasksStatus.GetStatus(NonConstFrame->StateTree);
				GlobalTasksStatus.SetStatusWithPriority(FrameTaskIndex, TaskStatus);
				bCompleted = GlobalTasksStatus.IsCompleted();
				bSucceed = true;
			}
			else
			{
				const FCompactStateTreeState& State = NonConstFrame->StateTree->States[ActivePath.StateHandle.Index];
				const int32 StateTaskBeginIndex = State.TasksBegin;
				const int32 StateTaskEndIndex = State.TasksBegin + State.TasksNum;
				const bool bIsStateTask = AssetNodeIndex >= StateTaskBeginIndex && AssetNodeIndex < StateTaskEndIndex;
				if (bIsStateTask)
				{
					const int32 StateTaskIndex = AssetNodeIndex - State.TasksBegin;
					FTasksCompletionStatus StateTasksStatus = NonConstFrame->ActiveTasksStatus.GetStatus(State);
					StateTasksStatus.SetStatusWithPriority(StateTaskIndex, TaskStatus);
					bCompleted = StateTasksStatus.IsCompleted();
					bSucceed = true;
				}
			}

			if (bCompleted)
			{
				Exec.bHasPendingCompletedState = true;
				ScheduleNextTick(Owner.Get(), StateTree.Get(), *Storage);
			}
		}
	}

	return bSucceed;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::UpdateScheduledTickRequest(UE::StateTree::FScheduledTickHandle Handle, FStateTreeScheduledTick ScheduledTick) const
{
	static_assert(bWithWriteAccess);

	if (IsValid())
	{
		FStateTreeExecutionState& Exec = Storage->GetMutableExecutionState();
		if (Exec.UpdateScheduledTickRequest(Handle, ScheduledTick))
		{
			ScheduleNextTick(Owner.Get(), StateTree.Get(), *Storage);
		}

		return true;
	}

	return false;
}

template <bool bWithWriteAccess>
UE::StateTree::Async::FActivePathInfo TStateTreeStrongExecutionContext<bWithWriteAccess>::GetActivePathInfo() const
{
	if (IsValidInstanceStorage())
	{
		FStateTreeExecutionState& Exec = Storage->GetMutableExecutionState();

		int32 FrameIndex = Exec.IndexOfActiveFrame(FrameID);
		if (FrameIndex != INDEX_NONE)
		{
			FStateTreeExecutionFrame* Frame = &Exec.ActiveFrames[FrameIndex];
			FStateTreeStateHandle StateHandle = StateID.IsValid() ? Frame->ActiveStates.FindStateHandle(StateID) : FStateTreeStateHandle::Invalid;
			if (!StateID.IsValid() || StateHandle.IsValid())
			{
				FStateTreeExecutionFrame* ParentFrame = FrameIndex != 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
				return {Frame, ParentFrame, StateHandle, NodeIndex};
			}
		}
	}

	return {};
}

template <bool bWithWriteAccess>
FStateTreeDataView TStateTreeStrongExecutionContext<bWithWriteAccess>::GetInstanceDataPtrInternal() const
{
	using namespace UE::StateTree;

	Async::FActivePathInfo ActivePath = GetActivePathInfo();
	if (ActivePath.IsValid())
	{
		const FStateTreeNodeBase& Node = ActivePath.GetNode();
		FStateTreeDataView InstanceDataView = InstanceData::GetDataViewOrTemporary(
			*Storage,
			/* SharedInstanceStorage */ nullptr,
			ActivePath.ParentFrame,
			*ActivePath.Frame,
			ActivePath.GetNode().InstanceDataHandle);

		return InstanceDataView;
	}

	return {};
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
void TStateTreeStrongExecutionContext<bWithWriteAccess>::ScheduleNextTick(TNotNull<UObject*> Owner, TNotNull<const UStateTree*> RootStateTree, FStateTreeInstanceStorage& Storage)
{
	static_assert(bWithWriteAccess);

	TInstancedStruct<FStateTreeExecutionExtension>& ExecutionExtension = Storage.GetMutableExecutionState().ExecutionExtension;
	if (RootStateTree->IsScheduledTickAllowed() && ExecutionExtension.IsValid())
	{
		ExecutionExtension.GetMutable().ScheduleNextTick(FStateTreeExecutionExtension::FContextParameters(*Owner, *RootStateTree, Storage));
	}
}

template struct TStateTreeStrongExecutionContext<false>;
template struct TStateTreeStrongExecutionContext<true>;

//@todo: remove all these instantiations once past 5.6 because of c++20 support
template bool STATETREEMODULE_API TStateTreeStrongExecutionContext<true>::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin) const;
template bool STATETREEMODULE_API TStateTreeStrongExecutionContext<true>::RequestTransition(FStateTreeStateHandle TargetState, EStateTreeTransitionPriority Priority, EStateTreeSelectionFallback Fallback) const;
template bool STATETREEMODULE_API TStateTreeStrongExecutionContext<true>::BroadcastDelegate(const FStateTreeDelegateDispatcher& Dispatcher) const;
template bool STATETREEMODULE_API TStateTreeStrongExecutionContext<true>::BindDelegate(const FStateTreeDelegateListener& Listener, FSimpleDelegate Delegate) const;
template bool STATETREEMODULE_API TStateTreeStrongExecutionContext<true>::UnbindDelegate(const FStateTreeDelegateListener& Listener) const;
template bool STATETREEMODULE_API TStateTreeStrongExecutionContext<true>::FinishTask(EStateTreeFinishTaskType FinishType) const;
template bool STATETREEMODULE_API TStateTreeStrongExecutionContext<true>::UpdateScheduledTickRequest(UE::StateTree::FScheduledTickHandle Handle, FStateTreeScheduledTick ScheduledTick) const;
template void STATETREEMODULE_API TStateTreeStrongExecutionContext<true>::ScheduleNextTick(TNotNull<UObject*> Owner, TNotNull<const UStateTree*> RootStateTree, FStateTreeInstanceStorage& Storage);

FStateTreeWeakExecutionContext::FStateTreeWeakExecutionContext(const FStateTreeExecutionContext& Context)
	: Owner(Context.GetOwner())
	, StateTree(Context.GetStateTree())
	, Storage(Context.GetMutableInstanceData()->GetWeakMutableStorage())
{
	if (const FStateTreeExecutionFrame* Frame = Context.GetCurrentlyProcessedFrame())
	{
		FrameID = Frame->FrameID;
		const int32 ActiveStateIndex = Frame->ActiveStates.IndexOfReverse(Context.GetCurrentlyProcessedState());
		if (ActiveStateIndex != INDEX_NONE)
		{
			StateID = Frame->ActiveStates.StateIDs[ActiveStateIndex];
		}

		// Only writes down the NodeIndex if it is created synchronously from the ExecContext tick frame
		NodeIndex = Context.GetCurrentlyProcessedNodeIndex();
	}
}

bool FStateTreeWeakExecutionContext::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin) const
{
	return MakeStrongExecutionContext().SendEvent(Tag, Payload, Origin);
}

bool FStateTreeWeakExecutionContext::RequestTransition(FStateTreeStateHandle TargetState, EStateTreeTransitionPriority Priority, const EStateTreeSelectionFallback Fallback) const
{
	return MakeStrongExecutionContext().RequestTransition(TargetState, Priority, Fallback);
}

bool FStateTreeWeakExecutionContext::BroadcastDelegate(const FStateTreeDelegateDispatcher& Dispatcher) const
{
	return MakeStrongExecutionContext().BroadcastDelegate(Dispatcher);
}

bool FStateTreeWeakExecutionContext::BindDelegate(const FStateTreeDelegateListener& Listener, FSimpleDelegate Delegate) const
{
	constexpr bool bWithWriteAccess = true;
	return MakeStrongExecutionContext().BindDelegate(Listener, Delegate);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
bool FStateTreeWeakExecutionContext::BindDelegate(const FStateTreeWeakTaskRef& Task, const FStateTreeDelegateListener& Listener, FSimpleDelegate Delegate) const
{
	// Deprecated. Use the version without the TaskRef.
	return BindDelegate(Listener, Delegate);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

bool FStateTreeWeakExecutionContext::RemoveDelegateListener(const FStateTreeDelegateListener& Listener) const
{
	return UnbindDelegate(Listener);
}

bool FStateTreeWeakExecutionContext::UnbindDelegate(const FStateTreeDelegateListener& Listener) const
{
	return MakeStrongExecutionContext().UnbindDelegate(Listener);
}

bool FStateTreeWeakExecutionContext::FinishTask(EStateTreeFinishTaskType FinishType) const
{
	return MakeStrongExecutionContext().FinishTask(FinishType);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
bool FStateTreeWeakExecutionContext::FinishTask(const FStateTreeWeakTaskRef& Task, EStateTreeFinishTaskType FinishType) const
{
	return FinishTask(FinishType);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

bool FStateTreeWeakExecutionContext::UpdateScheduledTickRequest(UE::StateTree::FScheduledTickHandle Handle, FStateTreeScheduledTick ScheduledTick) const
{
	return MakeStrongExecutionContext().UpdateScheduledTickRequest(Handle, ScheduledTick);
}
