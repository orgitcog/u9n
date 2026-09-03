// Copyright Epic Games, Inc. All Rights Reserved.

#if (defined(__AUTORTFM) && __AUTORTFM)

#include "Transaction.h"
#include "TransactionInlines.h"

#include "CallNestInlines.h"
#include "ContextStatus.h"
#include "HitSet.h"
#include "Utils.h"

#include <cstddef>
#include <cstdint>

namespace AutoRTFM
{
	
void FTransaction::Resurrect(FContext* const InContext)
{
	AUTORTFM_ASSERT(Context == InContext)
	Parent = nullptr;
	StatDepth = 1;
	RecordedWriteHash = 0;
	NumWriteLogsHashed = 0;
	CurrentMemoryValidationLevel = EMemoryValidationLevel::Disabled;
	CurrentOpenReturnAddress = nullptr;
	CurrentState = EState::Uninitialized;
	bIsStackScoped = false;
	bIsInAllocateFn = false;
}

void FTransaction::Suppress()
{
	CurrentState = EState::Done;
	Reset();
}

FTransaction** FTransaction::GetIntrusiveAddress()
{
	return &Parent;
}

FTransaction::FTransaction(FContext* Context)
    : Context(Context)
	, CommitTasks(Context->GetTaskPool())
	, AbortTasks(Context->GetTaskPool())
{
	Resurrect(Context);
}

FTransaction::~FTransaction()
{
	Suppress();
}

void FTransaction::Initialize(FTransaction* Parent_, bool bIsStackScoped_, FStackRange StackRange_)
{
	AUTORTFM_ASSERT(CurrentState == EState::Uninitialized);

	Parent = Parent_;
	bIsStackScoped = bIsStackScoped_;
	StackRange = StackRange_;

	// For stats, record the nested depth of the transaction.
	if (Parent)
	{
		StatDepth = Parent->StatDepth + 1;
	}

	Stats.Collect<EStatsKind::AverageTransactionDepth>(StatDepth);
	Stats.Collect<EStatsKind::MaximumTransactionDepth>(StatDepth);
}

void FTransaction::Reset()
{
	AUTORTFM_ASSERT(IsDone());

	CommitTasks.Reset();
	AbortTasks.Reset();
	HitSet.Reset();
	NewMemoryTracker.Reset();
	WriteLog.Reset();
	CurrentMemoryValidationLevel = EMemoryValidationLevel::Disabled;

	// Reset to the initial state.
	CurrentState = EState::Uninitialized;

	DeferredPopOnCommitHandlers.Reset();
	DeferredPopOnAbortHandlers.Reset();
	DeferredPopAllOnCommitHandlers.Reset();
	DeferredPopAllOnAbortHandlers.Reset();

	AUTORTFM_ASSERT(IsFresh());
}

bool FTransaction::IsFresh() const
{
    return HitSet.IsEmpty()
        && NewMemoryTracker.IsEmpty()
        && WriteLog.IsEmpty()
        && CommitTasks.IsEmpty()
        && AbortTasks.IsEmpty()
        && !IsDone()
		&& DeferredPopOnCommitHandlers.IsEmpty()
		&& DeferredPopOnAbortHandlers.IsEmpty()
		&& DeferredPopAllOnCommitHandlers.IsEmpty()
		&& DeferredPopAllOnAbortHandlers.IsEmpty();
}

void FTransaction::AbortWithoutThrowing()
{
	AUTORTFM_VERBOSE("Aborting '%hs'!", GetContextStatusName(Context->GetStatus()));

    AUTORTFM_ASSERT(Context->IsAborting());

    Stats.Collect<EStatsKind::Abort>();
    CollectStats();

	// Ensure that we enter the done state before applying the commit, as this
	// will ensure the open-memory validation is performed before the write log
	// is cleared.
	SetDone();

    // Call the destructors of all the OnCommit functors before undoing the transactional memory and
    // calling the OnAbort callbacks. This is important as the callback functions may have captured
    // variables that are depending on the allocated memory.
    CommitTasks.Reset();

    Undo();
	AbortTasks.RemoveEachBackward([&](TTask<void()>& Task)
    { 
        Task();
    });

    if (IsNested())
    {
		AUTORTFM_ASSERT(Parent);
    }
    else
    {
		AUTORTFM_ASSERT(Context->IsAborting());
    }
}

void FTransaction::AbortAndThrow()
{
    AbortWithoutThrowing();
	Context->Throw();
}

bool FTransaction::AttemptToCommit()
{
    AUTORTFM_ASSERT(Context->GetStatus() == EContextStatus::Committing);
    AUTORTFM_ASSERT(Context->GetCurrentTransaction() == this);

    Stats.Collect<EStatsKind::Commit>();
    CollectStats();

	// Ensure that we enter the done state before applying the commit, as this
	// will ensure the open-memory validation is performed before the write log
	// is cleared.
	SetDone();

    bool bResult;
    if (IsNested())
    {
        CommitNested();
        bResult = true;
    }
    else
    {
        bResult = AttemptToCommitOuterNest();
    }

    return bResult;
}

void FTransaction::Undo()
{
	AUTORTFM_VERBOSE("Undoing a transaction...");
	AUTORTFM_ASSERT(IsDone());

	for(auto Iter = WriteLog.rbegin(); Iter != WriteLog.rend(); ++Iter)
    {
		FWriteLogEntry Entry = *Iter;
		// No write records should be within the transaction's stack range.
		AUTORTFM_ENSURE(!IsOnStack(Entry.LogicalAddress));

		memcpy(Entry.LogicalAddress, Entry.Data, Entry.Size);
    }

	AUTORTFM_VERBOSE("Undone a transaction!");
}

void FTransaction::CommitNested()
{
    AUTORTFM_ASSERT(Parent);

	// We need to pass our write log to our parent transaction, but with care!
	// We need to discard any writes if the memory location is on the parent
	// transaction's stack range.
	for (FWriteLogEntry Write : WriteLog)
	{
		if (Parent->IsOnStack(Write.LogicalAddress))
		{
			continue;
		}

		if (Write.Size <= FHitSet::MaxSize)
		{
			FHitSetEntry HitSetEntry{};
			HitSetEntry.Address = reinterpret_cast<uintptr_t>(Write.LogicalAddress);
			HitSetEntry.Size = static_cast<uint16_t>(Write.Size);
			HitSetEntry.bNoMemoryValidation = Write.bNoMemoryValidation;

			if (Parent->HitSet.FindOrTryInsert(HitSetEntry))
			{
				continue; // Don't duplicate the write-log entry.
			}
		}

		Parent->WriteLog.Push(Write);
	}

	// For all the deferred calls to `PopOnCommitHandler` that we couldn't
	// process (because our transaction nest didn't `PushOnCommitHandler`)
	// we need to move these to the parent now to handle them.
	for (const void* Key : DeferredPopOnCommitHandlers)
	{
		Parent->PopDeferUntilCommitHandler(Key);
	}
	DeferredPopOnCommitHandlers.Reset();

	// For all the deferred calls to `PopOnAbortHandler` that we couldn't
	// process (because our transaction nest didn't `PushOnAbortHandler`)
	// we need to move these to the parent now to handle them.
	for (const void* Key : DeferredPopOnAbortHandlers)
	{
		Parent->PopDeferUntilAbortHandler(Key);
	}
	DeferredPopOnAbortHandlers.Reset();

	// For all the calls to `PopAllOnCommitHandlers` we need to run these
	// again on parent now to handle them there too.
	for (const void* Key : DeferredPopAllOnCommitHandlers)
	{
		Parent->PopAllDeferUntilCommitHandlers(Key);
	}
	DeferredPopAllOnCommitHandlers.Reset();

	// For all the calls to `PopAllOnAbortHandlers` we need to run these
	// again on parent now to handle them there too.
	for (const void* Key : DeferredPopAllOnAbortHandlers)
	{
		Parent->PopAllDeferUntilAbortHandlers(Key);
	}
	DeferredPopAllOnAbortHandlers.Reset();

    Parent->CommitTasks.AddAll(std::move(CommitTasks));
    Parent->AbortTasks.AddAll(std::move(AbortTasks));

    Parent->NewMemoryTracker.Merge(NewMemoryTracker);
}

bool FTransaction::AttemptToCommitOuterNest()
{
    AUTORTFM_ASSERT(!Parent);

	AUTORTFM_VERBOSE("About to run commit tasks!");
	Context->DumpState();
	AUTORTFM_VERBOSE("Running commit tasks...");

    AbortTasks.Reset();

    CommitTasks.RemoveEachForward([] (TTask<void()>& Task)
    { 
        Task();
    });

    return true;
}

void FTransaction::SetOpenActiveValidatorEnabled(EMemoryValidationLevel NewMemoryValidationLevel, const void* ReturnAddress)
{
	AUTORTFM_ASSERT(NewMemoryValidationLevel != EMemoryValidationLevel::Disabled);
	CurrentMemoryValidationLevel = NewMemoryValidationLevel;
	CurrentOpenReturnAddress = ReturnAddress;

	if (AutoRTFM::ForTheRuntime::GetMemoryValidationThrottlingEnabled())
	{
		FOpenHashThrottler& Throttler = Context->GetOpenHashThrottler();
		if (!Throttler.ShouldHashFor(ReturnAddress))
		{
			CurrentMemoryValidationLevel = EMemoryValidationLevel::Disabled;
		}
		Throttler.Update();
	}

	SetState<EState::OpenActive>();
}

void FTransaction::SetOpenActive(EMemoryValidationLevel NewMemoryValidationLevel, const void* ReturnAddress)
{
	if (AUTORTFM_UNLIKELY(NewMemoryValidationLevel != EMemoryValidationLevel::Disabled))
	{
		AUTORTFM_MUST_TAIL return SetOpenActiveValidatorEnabled(NewMemoryValidationLevel, ReturnAddress);
	}

	CurrentMemoryValidationLevel = NewMemoryValidationLevel;
	CurrentOpenReturnAddress = ReturnAddress;
	
	// TODO: Validate if open -> open with different validation levels.
	RecordedWriteHash = 0;
	NumWriteLogsHashed = 0;
	CurrentOpenReturnAddress = nullptr;

	SetState<EState::OpenActive>();
}

void FTransaction::SetClosedActive()
{
	SetState<EState::ClosedActive>();
}

void FTransaction::SetOpenInactive()
{
	SetState<EState::OpenInactive>();
}

void FTransaction::SetClosedInactive()
{
	SetState<EState::ClosedInactive>();
}

void FTransaction::SetActive()
{
	switch (CurrentState)
	{
		case EState::OpenActive:
		case EState::ClosedActive:
			break;
		case EState::OpenInactive:
			SetState<EState::OpenActive>();
			break;
		case EState::ClosedInactive:
			SetState<EState::ClosedActive>();
			break;
		default:
			AUTORTFM_FATAL("Invalid state");
	}
}

void FTransaction::SetInactive()
{
	switch (CurrentState)
	{
		case EState::OpenInactive:
		case EState::ClosedInactive:
			break;
		case EState::OpenActive:
			SetState<EState::OpenInactive>();
			break;
		case EState::ClosedActive:
			SetState<EState::ClosedInactive>();
			break;
		default:
			AUTORTFM_FATAL("Invalid state");
	}
}

void FTransaction::SetDone()
{
	SetState<EState::Done>();
}

template<FTransaction::EState NewState>
void FTransaction::SetState()
{
	AUTORTFM_ASSERT(NewState != CurrentState);

	switch (CurrentState)
	{
		case EState::Uninitialized:
			AUTORTFM_ASSERT(NewState == EState::OpenActive || NewState == EState::ClosedActive);
			break;

		// OpenActive -> OpenInactive, ClosedActive or Done
		case EState::OpenActive:
			AUTORTFM_ASSERT(NewState == EState::OpenInactive || NewState == EState::ClosedActive || NewState == EState::Done);
			if (CurrentMemoryValidationLevel != EMemoryValidationLevel::Disabled)
			{
				ValidateWriteHash();
				RecordedWriteHash = 0;
				NumWriteLogsHashed = 0;
			}
			else
			{
				AUTORTFM_ASSERT(RecordedWriteHash == 0 && NumWriteLogsHashed == 0);
			}
			break;

		// ClosedActive -> ClosedInactive, OpenActive or Done
		case EState::ClosedActive:
			AUTORTFM_ASSERT(NewState == EState::ClosedInactive || NewState == EState::OpenActive || NewState == EState::Done);
			break;

		// ClosedActive -> OpenActive
		case EState::OpenInactive:
			AUTORTFM_ASSERT(NewState == EState::OpenActive);
			break;

		// ClosedInactive -> ClosedActive
		case EState::ClosedInactive:
			AUTORTFM_ASSERT(NewState == EState::ClosedActive);
			break;

		case EState::Done:
			AUTORTFM_FATAL("Once Done, the transaction cannot change state without a call to Reset()");
			break;

		default:
			AUTORTFM_FATAL("Invalid state");
			break;
	}

	// OpenInactive, ClosedActive or Done -> OpenActive
	if (NewState == EState::OpenActive) {
		if (CurrentMemoryValidationLevel != EMemoryValidationLevel::Disabled)
		{
			AUTORTFM_ASSERT(RecordedWriteHash == 0 && NumWriteLogsHashed == 0);
			RecordWriteHash();
		}
	}

	CurrentState = NewState;
}

void FTransaction::DebugBreakIfMemoryValidationFails()
{
	if (CurrentMemoryValidationLevel != EMemoryValidationLevel::Disabled)
	{
		FWriteHash OldHash = RecordedWriteHash;
		FWriteHash NewHash = CalculateNestedWriteHash();
		if (OldHash != NewHash)
		{
			AUTORTFM_WARN("DebugBreakIfInvalidMemoryHash() detected a change in hash");
			__builtin_debugtrap();
		}
	}
}

void FTransaction::RecordWriteHash()
{
	NumWriteLogsHashed = WriteLog.Num();
	RecordedWriteHash = CalculateNestedWriteHash();
	Context->GetOpenHashThrottler().Update();
}

void FTransaction::ValidateWriteHash() const
{
	FWriteHash OldHash = RecordedWriteHash;
	FWriteHash NewHash = CalculateNestedWriteHash();
	Context->GetOpenHashThrottler().Update();

	static constexpr const char Message[] =
		"Memory modified in a transaction was also modified in an call to AutoRTFM::Open(). "
		"This may lead to memory corruption if the transaction is aborted.";
	if (AUTORTFM_UNLIKELY(OldHash != NewHash))
	{
		if (CurrentMemoryValidationLevel == EMemoryValidationLevel::Warn)
		{
			AUTORTFM_WARN(Message);
		}
		else
		{
			if (!ForTheRuntime::GetEnsureOnInternalAbort())
			{
				AUTORTFM_FATAL(Message);
			}
			else
			{
				AUTORTFM_ENSURE_MSG(OldHash == NewHash, Message);
			}
		}
	}
}

FTransaction::FWriteHash FTransaction::CalculateNestedWriteHash() const
{
	return CalculateNestedWriteHashWithLimit(NumWriteLogsHashed, CurrentOpenReturnAddress);
}

FTransaction::FWriteHash FTransaction::CalculateNestedWriteHashWithLimit(size_t NumWriteEntries, const void *OpenReturnAddress) const
{
	FWriteHash Hash = 0;
	if (nullptr != Parent)
	{
		Hash = 31 * Parent->CalculateNestedWriteHashWithLimit(Parent->WriteLog.Num(), OpenReturnAddress);
	}
	{
		FOpenHashThrottler::FHashScope Profile(Context->GetOpenHashThrottler(), OpenReturnAddress, WriteLog);
		Hash ^= WriteLog.Hash(NumWriteEntries);
	}
	return Hash;
}

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
