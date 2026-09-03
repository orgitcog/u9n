// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeExecutionTypes.h"
#include "StateTree.h"
#include "StateTreeDelegate.h"

const FStateTreeExternalDataHandle FStateTreeExternalDataHandle::Invalid = FStateTreeExternalDataHandle();

#if WITH_STATETREE_TRACE
const FStateTreeInstanceDebugId FStateTreeInstanceDebugId::Invalid = FStateTreeInstanceDebugId();
#endif // WITH_STATETREE_TRACE

//----------------------------------------------------------------------//
// FStateTreeTransitionSource
//----------------------------------------------------------------------//
FStateTreeTransitionSource::FStateTreeTransitionSource(
	const UStateTree* StateTree,
	const EStateTreeTransitionSourceType SourceType,
	const FStateTreeIndex16 TransitionIndex,
	const FStateTreeStateHandle TargetState,
	const EStateTreeTransitionPriority Priority
	)
	: Asset(StateTree)
	, SourceType(SourceType)
	, TransitionIndex(TransitionIndex)
	, TargetState(TargetState)
	, Priority(Priority)
{
}

//----------------------------------------------------------------------//
// FStateTreeTransitionResult
//----------------------------------------------------------------------//
//Deprecated
FStateTreeTransitionResult::FStateTreeTransitionResult(const FRecordedStateTreeTransitionResult& RecordedTransition)
{
}

//----------------------------------------------------------------------//
// FRecordedStateTreeTransitionResult
//----------------------------------------------------------------------//
//Deprecated
FRecordedStateTreeTransitionResult::FRecordedStateTreeTransitionResult(const FStateTreeTransitionResult& Transition)
{
}


//----------------------------------------------------------------------//
// FStateTreeExecutionState
//----------------------------------------------------------------------//
UE::StateTree::FActiveStatePath FStateTreeExecutionState::GetActiveStatePath() const
{
	int32 NewNum = 0;
	for (const FStateTreeExecutionFrame& Frame : ActiveFrames)
	{
		NewNum += Frame.ActiveStates.Num();
	}

	if (NewNum == 0 || ActiveFrames[0].StateTree == nullptr)
	{
		return UE::StateTree::FActiveStatePath();
	}

	TArray<UE::StateTree::FActiveState> Elements;
	Elements.Reserve(NewNum);

	for (const FStateTreeExecutionFrame& Frame : ActiveFrames)
	{
		for (int32 StateIndex = 0; StateIndex < Frame.ActiveStates.Num(); ++StateIndex)
		{
			Elements.Emplace(Frame.FrameID, Frame.ActiveStates.StateIDs[StateIndex], Frame.ActiveStates.States[StateIndex]);
		}
	}

	return UE::StateTree::FActiveStatePath(ActiveFrames[0].StateTree, MoveTemp(Elements));
}

const FStateTreeExecutionFrame* FStateTreeExecutionState::FindActiveFrame(UE::StateTree::FActiveFrameID FrameID) const
{
	return ActiveFrames.FindByPredicate([FrameID](const FStateTreeExecutionFrame& Other)
		{
			return Other.FrameID == FrameID;
		});
}

FStateTreeExecutionFrame* FStateTreeExecutionState::FindActiveFrame(UE::StateTree::FActiveFrameID FrameID)
{
	return ActiveFrames.FindByPredicate([FrameID](const FStateTreeExecutionFrame& Other)
		{
			return Other.FrameID == FrameID;
		});
}

int32 FStateTreeExecutionState::IndexOfActiveFrame(UE::StateTree::FActiveFrameID FrameID) const
{
	return ActiveFrames.IndexOfByPredicate([FrameID](const FStateTreeExecutionFrame& Other)
		{
			return Other.FrameID == FrameID;
		});
}

UE::StateTree::FScheduledTickHandle FStateTreeExecutionState::AddScheduledTickRequest(FStateTreeScheduledTick ScheduledTick)
{
	UE::StateTree::FScheduledTickHandle Result = UE::StateTree::FScheduledTickHandle::GenerateNewHandle();
	ScheduledTickRequests.Add(FScheduledTickRequest{.Handle = Result, .ScheduledTick = ScheduledTick});
	CacheScheduledTickRequest();
	return Result;
}

bool FStateTreeExecutionState::UpdateScheduledTickRequest(UE::StateTree::FScheduledTickHandle Handle, FStateTreeScheduledTick ScheduledTick)
{
	FScheduledTickRequest* Found = ScheduledTickRequests.FindByPredicate([Handle](const FScheduledTickRequest& Other) { return Other.Handle == Handle; });
	if (Found && Found->ScheduledTick != ScheduledTick)
	{
		Found->ScheduledTick = ScheduledTick;
		CacheScheduledTickRequest();
		return true;
	}
	return false;
}

bool FStateTreeExecutionState::RemoveScheduledTickRequest(UE::StateTree::FScheduledTickHandle Handle)
{
	const int32 IndexOf = ScheduledTickRequests.IndexOfByPredicate([Handle](const FScheduledTickRequest& Other) { return Other.Handle == Handle; });
	if (IndexOf != INDEX_NONE)
	{
		ScheduledTickRequests.RemoveAtSwap(IndexOf);
		CacheScheduledTickRequest();
	}
	return IndexOf != INDEX_NONE;
}

void FStateTreeExecutionState::CacheScheduledTickRequest()
{
	auto GetBestRequest = [](TConstArrayView<FScheduledTickRequest> Requests)
		{
			const int32 ScheduledTickRequestsNum = Requests.Num();
			if (ScheduledTickRequestsNum == 0)
			{
				return FStateTreeScheduledTick();
			}
			if (ScheduledTickRequestsNum == 1)
			{
				return Requests[0].ScheduledTick;
			}

			for (const FScheduledTickRequest& Request : Requests)
			{
				if (Request.ScheduledTick.ShouldTickEveryFrames())
				{
					return Request.ScheduledTick;
				}
			}

			for (const FScheduledTickRequest& Request : Requests)
			{
				if (Request.ScheduledTick.ShouldTickOnceNextFrame())
				{
					return Request.ScheduledTick;
				}
			}

			TOptional<float> CustomTickRate;
			for (const FScheduledTickRequest& Request : Requests)
			{
				const float CachedTickRate = Request.ScheduledTick.GetTickRate();
				CustomTickRate = CustomTickRate.IsSet() ? FMath::Min(CustomTickRate.GetValue(), CachedTickRate) : CachedTickRate;
			}
			return FStateTreeScheduledTick::MakeCustomTickRate(CustomTickRate.GetValue());
		};

	CachedScheduledTickRequest = GetBestRequest(ScheduledTickRequests);
}

//----------------------------------------------------------------------//
// FStateTreeExecutionFrame
//----------------------------------------------------------------------//
//Deprecated
FStateTreeExecutionFrame::FStateTreeExecutionFrame(const FRecordedStateTreeExecutionFrame& RecordedExecutionFrame)
{
}

//----------------------------------------------------------------------//
// FRecordedStateTreeExecutionFrame
//----------------------------------------------------------------------//
//Deprecated
FRecordedStateTreeExecutionFrame::FRecordedStateTreeExecutionFrame(const FStateTreeExecutionFrame& ExecutionFrame)
{
}

//Deprecated
//----------------------------------------------------------------------//
// FFinishedTask
//----------------------------------------------------------------------//
UE::StateTree::FFinishedTask::FFinishedTask(FActiveFrameID InFrameID, FActiveStateID InStateID, FStateTreeIndex16 InTaskIndex, EStateTreeRunStatus InRunStatus, EReasonType InReason, bool bInTickProcessed)
	: FrameID(InFrameID)
	, StateID(InStateID)
	, TaskIndex(InTaskIndex)
	, RunStatus(InRunStatus)
	, Reason(InReason)
	, bTickProcessed(bInTickProcessed)
{
}

//----------------------------------------------------------------------//
// FStateTreeScheduledTick
//----------------------------------------------------------------------//
FStateTreeScheduledTick FStateTreeScheduledTick::MakeSleep()
{
	return FStateTreeScheduledTick(UE_FLOAT_NON_FRACTIONAL);
}

FStateTreeScheduledTick FStateTreeScheduledTick::MakeEveryFrames()
{
	return FStateTreeScheduledTick(0.0f);
}

FStateTreeScheduledTick FStateTreeScheduledTick::MakeNextFrame()
{
	return FStateTreeScheduledTick(UE_KINDA_SMALL_NUMBER);
}

FStateTreeScheduledTick FStateTreeScheduledTick::MakeCustomTickRate(float DeltaTime)
{
	ensureMsgf(DeltaTime >= 0.0f, TEXT("Use a value greater than zero."));
	if (DeltaTime > 0.0f)
	{
		return FStateTreeScheduledTick(DeltaTime);
	}
	return MakeEveryFrames();
}

bool FStateTreeScheduledTick::ShouldSleep() const
{
	return NextDeltaTime >= UE_FLOAT_NON_FRACTIONAL;
}

bool FStateTreeScheduledTick::ShouldTickEveryFrames() const
{
	return NextDeltaTime == 0.0f;
}

bool FStateTreeScheduledTick::ShouldTickOnceNextFrame() const
{
	return NextDeltaTime == UE_KINDA_SMALL_NUMBER;
}

bool FStateTreeScheduledTick::HasCustomTickRate() const
{
	return NextDeltaTime > 0.0f;
}

float FStateTreeScheduledTick::GetTickRate() const
{
	return NextDeltaTime;
}

//----------------------------------------------------------------------//
// FScheduledTickHandle
//----------------------------------------------------------------------//
UE::StateTree::FScheduledTickHandle UE::StateTree::FScheduledTickHandle::GenerateNewHandle()
{
	static std::atomic<uint32> Value = 0;

	uint32 Result = 0;
	UE_AUTORTFM_OPEN
	{
		Result = ++Value;

		// Check that we wrap round to 0, because we reserve 0 for invalid.
		if (Result == 0)
		{
			Result = ++Value;
		}
	};

	return FScheduledTickHandle(Result);
}


//----------------------------------------------------------------------//
// FStateTreeDelegateActiveListeners
//----------------------------------------------------------------------//
FStateTreeDelegateActiveListeners::FActiveListener::FActiveListener(const FStateTreeDelegateListener& InListener, FSimpleDelegate InDelegate, UE::StateTree::FActiveFrameID InFrameID, UE::StateTree::FActiveStateID InStateID, FStateTreeIndex16 InOwningNodeIndex)
	: Listener(InListener)
	, Delegate(MoveTemp(InDelegate))
	, FrameID(InFrameID)
	, StateID(InStateID)
	, OwningNodeIndex(InOwningNodeIndex)
{}

FStateTreeDelegateActiveListeners::~FStateTreeDelegateActiveListeners()
{
	check(BroadcastingLockCount == 0);
}

void FStateTreeDelegateActiveListeners::Add(const FStateTreeDelegateListener& Listener, FSimpleDelegate Delegate, UE::StateTree::FActiveFrameID InFrameID, UE::StateTree::FActiveStateID InStateID, FStateTreeIndex16 OwningNodeIndex)
{
	check(Listener.IsValid());
	Remove(Listener);
	Listeners.Emplace(Listener, MoveTemp(Delegate), InFrameID, InStateID, OwningNodeIndex);
}

void FStateTreeDelegateActiveListeners::Remove(const FStateTreeDelegateListener& Listener)
{
	check(Listener.IsValid());

	const int32 Index = Listeners.IndexOfByPredicate([Listener](const FActiveListener& ActiveListener)
		{
			return ActiveListener.Listener == Listener;
		});

	if (Index == INDEX_NONE)
	{
		return;
	}

	if (BroadcastingLockCount > 0)
	{
		Listeners[Index] = FActiveListener();
		bContainsUnboundListeners = true;
	}
	else
	{
		Listeners.RemoveAtSwap(Index);
	}
}

void FStateTreeDelegateActiveListeners::RemoveAll(UE::StateTree::FActiveFrameID FrameID)
{
	check(BroadcastingLockCount == 0);
	Listeners.RemoveAllSwap([FrameID](const FActiveListener& Listener)
		{
			return Listener.FrameID == FrameID;
		});
}

void FStateTreeDelegateActiveListeners::RemoveAll(UE::StateTree::FActiveStateID StateID)
{
	check(BroadcastingLockCount == 0);
	Listeners.RemoveAllSwap([StateID](const FActiveListener& Listener)
		{
			return Listener.StateID == StateID;
		});
}

void FStateTreeDelegateActiveListeners::BroadcastDelegate(const FStateTreeDelegateDispatcher& Dispatcher, const FStateTreeExecutionState& Exec)
{
	check(Dispatcher.IsValid());

	++BroadcastingLockCount;

	const int32 NumListeners = Listeners.Num();
	for (int32 Index = 0; Index < NumListeners; ++Index)
	{
		FActiveListener& ActiveListener = Listeners[Index];
		if (ActiveListener.Listener.GetDispatcher() == Dispatcher
			&& ActiveListener.OwningNodeIndex.Get() <= Exec.EnterStateFailedTaskIndex.Get()
			&& ActiveListener.OwningNodeIndex.Get() < Exec.LastExitedNodeIndex.Get())
		{
			if (const FStateTreeExecutionFrame* ExectionFrame = Exec.FindActiveFrame(ActiveListener.FrameID))
			{
				if (!ActiveListener.StateID.IsValid() || ExectionFrame->ActiveStates.Contains(ActiveListener.StateID))
				{
					ActiveListener.Delegate.ExecuteIfBound();
				}
			}
		}
	}

	--BroadcastingLockCount;

	if (BroadcastingLockCount == 0)
	{
		RemoveUnbounds();
	}
}

void FStateTreeDelegateActiveListeners::RemoveUnbounds()
{
	check(BroadcastingLockCount == 0);
	if (!bContainsUnboundListeners)
	{
		return;
	}

	Listeners.RemoveAllSwap([](const FActiveListener& Listener) { return !Listener.IsValid(); });
	bContainsUnboundListeners = false;
}
