// Copyright Epic Games, Inc. All Rights Reserved.

#if (defined(__AUTORTFM) && __AUTORTFM)

#include "Context.h"
#include "ContextInlines.h"

#include "AutoRTFMMetrics.h"
#include "CallNestInlines.h"
#include "ExternAPI.h"
#include "FunctionMap.h"
#include "ScopedGuard.h"
#include "StackRange.h"
#include "Stats.h"
#include "Transaction.h"
#include "TransactionInlines.h"
#include "Utils.h"

#if AUTORTFM_PLATFORM_WINDOWS
extern "C" __declspec(dllimport) void __stdcall GetCurrentThreadStackLimits(void**, void**);
#endif

namespace
{
AutoRTFM::FAutoRTFMMetrics GAutoRTFMMetrics;
}

namespace AutoRTFM
{

FContext* FContext::Instance = nullptr;

FContext* FContext::Create()
{
	AUTORTFM_ENSURE(Instance == nullptr);
	void* Memory = AutoRTFM::Allocate(sizeof(FContext), alignof(FContext));
	Instance = new (Memory) FContext();
	return Instance;
}

void ResetAutoRTFMMetrics()
{
	GAutoRTFMMetrics = FAutoRTFMMetrics{};
}

// get a snapshot of the current internal metrics
FAutoRTFMMetrics GetAutoRTFMMetrics()
{
	return GAutoRTFMMetrics;
}

bool FContext::IsTransactional() const
{
    return GetStatus() == EContextStatus::OnTrack;
}

bool FContext::IsCommittingOrAborting() const
{
	switch (GetStatus())
	{
	default:
		return true;
	case EContextStatus::Idle:
	case EContextStatus::OnTrack:
		return false;
	}
}

void FContext::MaterializeDeferredTransactions()
{
	uint64_t NumToAllocate = GetNumDeferredTransactions();
	NumDeferredTransactions = 0;
	for (uint64_t I = 0; I < NumToAllocate; ++I)
	{
		StartNonDeferredTransaction(EMemoryValidationLevel::Disabled);
	}
}

void FContext::StartTransaction(EMemoryValidationLevel MemoryValidationLevel)
{
	if (MemoryValidationLevel != EMemoryValidationLevel::Disabled)
	{
		MaterializeDeferredTransactions();
		StartNonDeferredTransaction(MemoryValidationLevel);
		return;
	}

	NumDeferredTransactions += 1;
	AUTORTFM_ENSURE_MSG(CurrentTransaction, "FContext::StartTransaction() can only be called within a scoped transaction");
	AUTORTFM_ASSERT(Status == EContextStatus::OnTrack);

	GAutoRTFMMetrics.NumTransactionsStarted++;
}

void FContext::StartNonDeferredTransaction(EMemoryValidationLevel MemoryValidationLevel)
{
	AUTORTFM_ASSERT(GetNumDeferredTransactions() == 0);
	AUTORTFM_ENSURE_MSG(CurrentTransaction, "FContext::StartNonDeferredTransaction() can only be called within a scoped transaction");

	PushTransaction(
		/* Closed */ false,
		/* bIsScoped */ false,
		/* StackRange */ CurrentTransaction->GetStackRange(),
		/* MemoryValidationLevel */ MemoryValidationLevel);

	// This form of transaction is always ultimately within a scoped Transact 
	AUTORTFM_ASSERT(Status == EContextStatus::OnTrack);

	GAutoRTFMMetrics.NumTransactionsStarted++;
}

ETransactionResult FContext::CommitTransaction()
{
	AUTORTFM_ASSERT(Status == EContextStatus::OnTrack);

	ETransactionResult Result = ETransactionResult::Committed;

	if (GetNumDeferredTransactions())
	{
		// The optimization worked! We didn't need to allocate an FTransaction for this.
		NumDeferredTransactions -= 1;
	}
	else
	{
		// Scoped transactions commit on return, so committing explicitly isn't allowed
		AUTORTFM_ASSERT(CurrentTransaction->IsScopedTransaction() == false);

		if (CurrentTransaction->IsNested())
		{
			Result = ResolveNestedTransaction(CurrentTransaction);
		}
		else
		{
			AUTORTFM_VERBOSE("About to commit; my state is:");
			DumpState();
			AUTORTFM_VERBOSE("Committing...");

			if (AttemptToCommitTransaction(CurrentTransaction))
			{
				Result = ETransactionResult::Committed;
			}
			else
			{
				AUTORTFM_VERBOSE("Commit failed!");
				AUTORTFM_ASSERT(Status != EContextStatus::OnTrack);
				AUTORTFM_ASSERT(Status != EContextStatus::Idle);
			}
		}

		// Parent transaction is now the current transaction
		PopTransaction();
	}

	GAutoRTFMMetrics.NumTransactionsCommitted++;

	return Result;
}

ETransactionResult FContext::RollbackTransaction(EContextStatus NewStatus)
{
	GAutoRTFMMetrics.NumTransactionsAborted++;

	AUTORTFM_ASSERT(Status == EContextStatus::OnTrack);
	AUTORTFM_ASSERT(NewStatus == EContextStatus::AbortedByRequest ||	   
		   NewStatus == EContextStatus::AbortedByCascadingAbort ||
		   NewStatus == EContextStatus::AbortedByCascadingRetry);

	Status = NewStatus;

	ETransactionResult Result = ETransactionResult::AbortedByRequest;

	if (GetNumDeferredTransactions())
	{
		// The optimization worked! We didn't need to allocate an FTransaction for this.
		NumDeferredTransactions -= 1;
	}
	else
	{
		AUTORTFM_ASSERT(nullptr != CurrentTransaction);

		// Sort out how aborts work
		CurrentTransaction->AbortWithoutThrowing();

		// Non-scoped transactions are ended immediately, but scoped need to get to the end scope before being popped
		if (!CurrentTransaction->IsScopedTransaction())
		{
			Result = ResolveNestedTransaction(CurrentTransaction);
			PopTransaction();
		}
	}

	// If this is a cascading abort, we should reflect that in the returned result.
	if (Result == ETransactionResult::AbortedByRequest &&
		(Status == EContextStatus::AbortedByCascadingAbort ||
		 Status == EContextStatus::AbortedByCascadingRetry))
	{
		Result = ETransactionResult::AbortedByCascade;
	}

	return Result;
}

void FContext::AbortTransaction(EContextStatus NewStatus)
{
	RollbackTransaction(NewStatus);
	Throw();
}

void FContext::AbortTransactionWithPostAbortCallback(EContextStatus NewStatus, TTask<void()>&& InCallback)
{
	// The callback parameter is only honored by the cascading aborts.
	AUTORTFM_ASSERT(NewStatus == EContextStatus::AbortedByCascadingAbort ||
		   NewStatus == EContextStatus::AbortedByCascadingRetry);

	// We must explicitly copy the passed-in callback here, because the original may have been
	// allocated within a transactional context; if so, its memory was allocated under
	// transactionalized conditions. By copying, we create an open copy of the callback.
	if (!PostAbortCallback.IsSet())
	{
		PostAbortCallback = InCallback;
	}
	else if (InCallback.IsSet())
	{
		AUTORTFM_FATAL("Only one post-abort callback at a time is supported");
	}

	AbortTransaction(NewStatus);
}

bool FContext::IsAborting() const
{
	switch (Status)
	{
	default:
		return true;
	case EContextStatus::OnTrack:
	case EContextStatus::Idle:
	case EContextStatus::Committing:
		return false;
	}
}

EContextStatus FContext::CallClosedNest(void (*ClosedFunction)(void* Arg), void* Arg)
{
	TScopedGuard<void*> ClosedStackAddressGuard(ClosedStackAddress, &ClosedStackAddressGuard);

	FTransaction* const Transaction = GetCurrentTransaction();
	AUTORTFM_ASSERT(Transaction != nullptr);
	AUTORTFM_ASSERT(FTransaction::EState::OpenActive == Transaction->State());
	EMemoryValidationLevel PreviousValidationLevel = Transaction->MemoryValidationLevel();
	const void* PreviousOpenReturnAddress = Transaction->OpenReturnAddress();
	Transaction->SetClosedActive();

	PushCallNest(new FCallNest(this));

	CurrentNest->Try([&]() { ClosedFunction(Arg); });

	PopCallNest();

	if (Transaction == CurrentTransaction && Transaction->IsClosedActive())  // Transaction may have been aborted.
	{
		Transaction->SetOpenActive(PreviousValidationLevel, PreviousOpenReturnAddress);
	}

	return GetStatus();
}

void FContext::PushCallNest(FCallNest* NewCallNest)
{
	AUTORTFM_ASSERT(NewCallNest != nullptr);
	AUTORTFM_ASSERT(NewCallNest->Parent == nullptr);

	NewCallNest->Parent = CurrentNest;
	CurrentNest = NewCallNest;
}

void FContext::PopCallNest()
{
	AUTORTFM_ASSERT(CurrentNest != nullptr);
	FCallNest* OldCallNest = CurrentNest;
	CurrentNest = CurrentNest->Parent;

	delete OldCallNest;
}

FTransaction* FContext::PushTransaction(
	bool bClosed,
	bool bIsScoped,
	FStackRange StackRange,
	EMemoryValidationLevel MemoryValidationLevel)
{
	AUTORTFM_ASSERT(!GetNumDeferredTransactions());

	if (CurrentTransaction != nullptr)
	{
		AUTORTFM_ASSERT(CurrentTransaction->IsActive());
		CurrentTransaction->SetInactive();
	}

	FTransaction* NewTransaction = TransactionPool.Take(this);
	NewTransaction->Initialize(
		/* Parent */ CurrentTransaction, 
		/* bIsScoped */ bIsScoped, 
		/* StackRange */ StackRange);

	if (bClosed)
	{
		NewTransaction->SetClosedActive();
	}
	else
	{
		NewTransaction->SetOpenActive(MemoryValidationLevel, /* ReturnAddress */ nullptr);
	}

	CurrentTransaction = NewTransaction;

	// Collect stats that we've got a new transaction.
	Stats.Collect<EStatsKind::Transaction>();

	return NewTransaction;
}

void FContext::PopTransaction()
{
	AUTORTFM_ASSERT(!GetNumDeferredTransactions());
	AUTORTFM_ASSERT(CurrentTransaction != nullptr);
	AUTORTFM_ASSERT(CurrentTransaction->IsDone());
	FTransaction* OldTransaction = CurrentTransaction;
	CurrentTransaction = CurrentTransaction->GetParent();
	if (CurrentTransaction != nullptr)
	{
		AUTORTFM_ASSERT(CurrentTransaction->IsInactive());
		CurrentTransaction->SetActive();
	}
	TransactionPool.Return(OldTransaction);
}

void FContext::ClearTransactionStatus()
{
	switch (Status)
	{
	case EContextStatus::OnTrack:
		break;
	case EContextStatus::AbortedByLanguage:
	case EContextStatus::AbortedByRequest:
	case EContextStatus::AbortedByCascadingAbort:
	case EContextStatus::AbortedByCascadingRetry:
	case EContextStatus::AbortedByFailedLockAcquisition:
		Status = EContextStatus::OnTrack;
		break;
	default:
		AutoRTFM::InternalUnreachable();
	}
}

ETransactionResult FContext::ResolveNestedTransaction(FTransaction* NewTransaction)
{
	if (Status == EContextStatus::OnTrack)
	{
		bool bCommitResult = AttemptToCommitTransaction(NewTransaction);
		AUTORTFM_ASSERT(bCommitResult);
		AUTORTFM_ASSERT(Status == EContextStatus::OnTrack);
		return ETransactionResult::Committed;
	}

	AUTORTFM_ASSERT(NewTransaction->IsDone());

	switch (Status)
	{
	case EContextStatus::AbortedByRequest:
		return ETransactionResult::AbortedByRequest;
	case EContextStatus::AbortedByLanguage:
		return ETransactionResult::AbortedByLanguage;
	case EContextStatus::AbortedByCascadingAbort:
	case EContextStatus::AbortedByCascadingRetry:
		return ETransactionResult::AbortedByCascade;
	default:
		AutoRTFM::InternalUnreachable();
	}
}

AutoRTFM::FStackRange FContext::GetThreadStackRange()
{
	// On some platforms, looking up the stack range is quite expensive, so caching it
	// is important for performance. Linux glibc is particularly bad--see
	// https://github.com/golang/go/issues/68587 for a deep dive.
	thread_local FStackRange CachedStackRange = []
	{
		FStackRange Stack;

#if AUTORTFM_PLATFORM_WINDOWS
		GetCurrentThreadStackLimits(&Stack.Low, &Stack.High);
#elif defined(__APPLE__)         
		Stack.High = pthread_get_stackaddr_np(pthread_self());
		size_t StackSize = pthread_get_stacksize_np(pthread_self());
		Stack.Low = static_cast<char*>(Stack.High) - StackSize;
#else
		pthread_attr_t Attr{};
		pthread_getattr_np(pthread_self(), &Attr);
		Stack.Low = 0;
		size_t StackSize = 0;
		pthread_attr_getstack(&Attr, &Stack.Low, &StackSize);
		Stack.High = static_cast<char*>(Stack.Low) + StackSize;
#endif

		AUTORTFM_ASSERT(Stack.High > Stack.Low);
		return Stack;
	}();

	return CachedStackRange;
}

ETransactionResult FContext::Transact(void (*UninstrumentedFunction)(void*), void (*InstrumentedFunction)(void*), void* Arg)
{
    if (AUTORTFM_UNLIKELY(EContextStatus::Committing == Status))
    {
    	return ETransactionResult::AbortedByTransactInOnCommit;
    }

    if (AUTORTFM_UNLIKELY(IsAborting()))
    {
    	return ETransactionResult::AbortedByTransactInOnAbort;
    }
    
    AUTORTFM_ASSERT(Status == EContextStatus::Idle || Status == EContextStatus::OnTrack);

    if (!InstrumentedFunction)
    {
		AUTORTFM_WARN("Could not find function in AutoRTFM::FContext::Transact");
        return ETransactionResult::AbortedByLanguage;
    }

	// TODO: We could do better if we ever need to. There is no fundamental
	// reason we can't have a "range" of deferred transactions in the middle
	// of the transaction stack.
	MaterializeDeferredTransactions();
	AUTORTFM_ASSERT(!GetNumDeferredTransactions());
    
	FCallNest* NewNest = new FCallNest(this);

	void* TransactStackStart = &NewNest;

	ETransactionResult Result = ETransactionResult::Committed; // Initialize to something to make the compiler happy.

	if (!CurrentTransaction)
	{
		AUTORTFM_ASSERT(Status == EContextStatus::Idle);

		AUTORTFM_ASSERT(CurrentThreadId == FThreadID::Invalid);
		CurrentThreadId = FThreadID::GetCurrent();

		AUTORTFM_ASSERT(Stack == FStackRange{});
		Stack = GetThreadStackRange();

		AUTORTFM_ASSERT(Stack.Contains(TransactStackStart));

		FTransaction* NewTransaction = PushTransaction(
			/* Closed */ true,
			/* bIsScoped */ true,
			/* StackRange */ {Stack.Low, &TransactStackStart},
			/* MemoryValidationLevel */ EMemoryValidationLevel::Disabled);

		PushCallNest(NewNest);

		bool bTriedToRunOnce = false;

        for (;;)
        {
            Status = EContextStatus::OnTrack;
            AUTORTFM_ASSERT(CurrentTransaction->IsFresh());
			CurrentNest->Try([&] () { InstrumentedFunction(Arg); });
			AUTORTFM_ASSERT(CurrentTransaction == NewTransaction); // The transaction lambda should have unwound any nested transactions.
            AUTORTFM_ASSERT(Status != EContextStatus::Idle);

			switch (Status)
			{
			case EContextStatus::OnTrack:
				AUTORTFM_VERBOSE("About to commit; my state is:");
				DumpState();
				AUTORTFM_VERBOSE("Committing...");

				if (AUTORTFM_UNLIKELY(!bTriedToRunOnce && AutoRTFM::ForTheRuntime::ShouldRetryNonNestedTransactions()))
				{
					// We skip trying to commit this time, and instead re-run the transaction.
					Status = EContextStatus::AbortedByFailedLockAcquisition;
					CurrentTransaction->AbortWithoutThrowing();
					ClearTransactionStatus();

					// We've tried to run at least once if we get here!
					CurrentTransaction->Reset();
					CurrentTransaction->SetClosedActive();
					bTriedToRunOnce = true;
					continue;
				}

				if (AttemptToCommitTransaction(CurrentTransaction))
				{
					Result = ETransactionResult::Committed;
					break;
				}

				AUTORTFM_VERBOSE("Commit failed!");

				AUTORTFM_ASSERT(Status != EContextStatus::OnTrack);
				AUTORTFM_ASSERT(Status != EContextStatus::Idle);
				break;

			case EContextStatus::AbortedByRequest:
				AUTORTFM_ASSERT(!PostAbortCallback.IsSet());
				Result = ETransactionResult::AbortedByRequest;
				break;

			case EContextStatus::AbortedByLanguage:
				Result = ETransactionResult::AbortedByLanguage;
				break;

			case EContextStatus::AbortedByCascadingAbort:
				if (PostAbortCallback.IsSet())
				{
					// Call the post-abort callback to do whatever work the user
					// required be done before throwing.
					Status = EContextStatus::InPostAbort;

					PostAbortCallback();
					PostAbortCallback.Reset();

					AUTORTFM_ASSERT(Status == EContextStatus::InPostAbort);
					Status = EContextStatus::AbortedByCascadingAbort;
				}

				Result = ETransactionResult::AbortedByCascade;
				break;

			case EContextStatus::AbortedByCascadingRetry:
				AUTORTFM_ASSERT(PostAbortCallback.IsSet());
			
				// Clean up the transaction to get it ready for re-execution.
				ClearTransactionStatus();
				CurrentTransaction->Reset();

				AUTORTFM_ASSERT(Status == EContextStatus::OnTrack);
				Status = EContextStatus::InPostAbort;

				// Call the post-abort callback to do whatever work the user
				// required be done before we attempt to re-execute.
				PostAbortCallback();
				PostAbortCallback.Reset();
				
				AUTORTFM_ASSERT(Status == EContextStatus::InPostAbort);
				Status = EContextStatus::OnTrack;

				// Then get rolling!
				CurrentTransaction->SetClosedActive();

				// Lastly check whether the AutoRTFM runtime was disabled during
				// the call to `PostAbortCallback`, and if so just execute the
				// function without AutoRTFM as a fallback.
				if (!ForTheRuntime::IsAutoRTFMRuntimeEnabled())
				{
					UninstrumentedFunction(Arg);
					Result = ETransactionResult::Committed;
					break;
				}

				continue;

			case EContextStatus::AbortedByFailedLockAcquisition:
				continue; // Retry the transaction

			default:
				Unreachable();
			}

			break;
		}

		if (!NewTransaction->IsDone())
		{
			NewTransaction->SetDone();
		}

		PopCallNest();
		PopTransaction();
		ClearTransactionStatus();

		AUTORTFM_ASSERT(CurrentNest == nullptr);
		AUTORTFM_ASSERT(CurrentTransaction == nullptr);

        Reset();
	}
    else
    {
		// This transaction is within another transaction
		AUTORTFM_ASSERT(Status == EContextStatus::OnTrack);

		AUTORTFM_ASSERT(CurrentThreadId == FThreadID::GetCurrent());

		AUTORTFM_ASSERT(Stack.Contains(TransactStackStart));

		FTransaction* NewTransaction = PushTransaction(
			/* Closed */ true,
			/* bIsScoped */ true,
			/* StackRange */ {Stack.Low, &TransactStackStart},
			/* MemoryValidationLevel */ EMemoryValidationLevel::Disabled);

		PushCallNest(NewNest);

		bool bTriedToRunOnce = false;

		for (;;)
		{
			CurrentNest->Try([&]() { InstrumentedFunction(Arg); });
			AUTORTFM_ASSERT(CurrentTransaction == NewTransaction);

			if (Status == EContextStatus::OnTrack)
			{
				if (AUTORTFM_UNLIKELY(!bTriedToRunOnce && AutoRTFM::ForTheRuntime::ShouldRetryNestedTransactionsToo()))
				{
					// We skip trying to commit this time, and instead re-run the transaction.
					Status = EContextStatus::AbortedByFailedLockAcquisition;
					NewTransaction->AbortWithoutThrowing();
					ClearTransactionStatus();

					// We've tried to run at least once if we get here!
					CurrentTransaction->Reset();
					CurrentTransaction->SetClosedActive();
					bTriedToRunOnce = true;
					continue;
				}
			}

			Result = ResolveNestedTransaction(NewTransaction);
			break;
		}

		PopCallNest();
		PopTransaction();

		AUTORTFM_ASSERT(CurrentNest != nullptr);
		AUTORTFM_ASSERT(CurrentTransaction != nullptr);

		// Cascading aborts should cause all transactions to abort!
		switch (Result)
		{
		default:
			break;
		case ETransactionResult::AbortedByCascade:
			CurrentTransaction->AbortAndThrow();
			break;
		}

		ClearTransactionStatus();
	}

	return Result;
}

void FContext::AbortByRequestAndThrow()
{
    AUTORTFM_ASSERT(Status == EContextStatus::OnTrack);
	GAutoRTFMMetrics.NumTransactionsAbortedByRequest++;
    Status = EContextStatus::AbortedByRequest;
    GetCurrentTransaction()->AbortAndThrow();
}

void FContext::AbortByRequestWithoutThrowing()
{
	AUTORTFM_ASSERT(Status == EContextStatus::OnTrack);
	GAutoRTFMMetrics.NumTransactionsAbortedByRequest++;
	Status = EContextStatus::AbortedByRequest;
	GetCurrentTransaction()->AbortWithoutThrowing();
}

void FContext::AbortByLanguageAndThrow()
{
    AUTORTFM_ASSERT(Status == EContextStatus::OnTrack);
	GAutoRTFMMetrics.NumTransactionsAbortedByLanguage++;
    Status = EContextStatus::AbortedByLanguage;
    GetCurrentTransaction()->AbortAndThrow();
}

void FContext::Reset()
{
	AUTORTFM_ASSERT(CurrentThreadId == FThreadID::GetCurrent() || CurrentThreadId == FThreadID::Invalid);

	CurrentThreadId = FThreadID::Invalid;
	Stack = {};
	CurrentTransaction = nullptr;
	CurrentNest = nullptr;
	Status = EContextStatus::Idle;
	StackLocalInitializerDepth = 0;
	TaskPool.Reset();
}

void FContext::Throw()
{
	GetCurrentNest()->AbortJump.Throw();
}

void FContext::DumpState() const
{
	AUTORTFM_VERBOSE("Context at %p", this);
}

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
