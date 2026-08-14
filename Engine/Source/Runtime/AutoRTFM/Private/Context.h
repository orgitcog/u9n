// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if (defined(__AUTORTFM) && __AUTORTFM)

#include "IntrusivePool.h"
#include "OpenHashThrottler.h"
#include "StackRange.h"
#include "TaskArray.h"
#include "ThreadID.h"
#include "Transaction.h"

namespace AutoRTFM
{

class FLineLock;
class FCallNest;

class FContext final
{
	using FTaskPool = typename TTaskArray<TTask<void()>>::FEntryPool;
	using FTransactionPool = TIntrusivePool<FTransaction, 16>;

public:
	static FContext* Create();
	static FContext* Get();

    // This is public API
    ETransactionResult Transact(void (*UninstrumentedFunction)(void*), void (*InstrumentedFunction)(void*), void* Arg);
    
	EContextStatus CallClosedNest(void (*ClosedFunction)(void* Arg), void* Arg);

	void AbortByRequestAndThrow();
	void AbortByRequestWithoutThrowing();

	// Open API - no throw
	void StartTransaction(EMemoryValidationLevel MemoryValidationLevel);

	// No throw
	ETransactionResult RollbackTransaction(EContextStatus NewStatus);

	ETransactionResult CommitTransaction();
	void AbortTransaction(EContextStatus NewStatus);
	void AbortTransactionWithPostAbortCallback(EContextStatus NewStatus, TTask<void()>&& InCallback);
	void ClearTransactionStatus();
	bool IsAborting() const;
	bool IsTransactional() const;
	bool IsCommittingOrAborting() const;

    // Record that a write is about to occur at the given LogicalAddress of Size bytes.
    void RecordWrite(void* LogicalAddress, size_t Size);
    template<unsigned SIZE> void RecordWrite(void* LogicalAddress);
	template<unsigned SIZE> void RecordWriteSlow(void* LogicalAddress);

    void DidAllocate(void* LogicalAddress, size_t Size);
    void DidFree(void* LogicalAddress);

    // The rest of this is internalish.
    [[noreturn]] void AbortByLanguageAndThrow();

	inline uint64_t GetNumDeferredTransactions() const
	{
		return NumDeferredTransactions;
	}
	inline bool MustMaterializeDeferredTransactions() const
	{
		return (AUTORTFM_UNLIKELY(0 < GetNumDeferredTransactions()));
	}
	inline FTransaction* GetCurrentTransaction()
	{
		if (MustMaterializeDeferredTransactions())
		{
			MaterializeDeferredTransactions();
		}
		return CurrentTransaction;
	}
	inline FTransaction* GetMaterializedTransaction()
	{
		return CurrentTransaction;
	}
	inline FTransaction* GetCurrentTransactionAssumingNoMaterialization()
	{
		AUTORTFM_ASSERT(!MustMaterializeDeferredTransactions());
		return CurrentTransaction;
	}

	inline FCallNest* GetCurrentNest() const { return CurrentNest; }
	inline EContextStatus GetStatus() const { return CurrentThreadId == FThreadID::GetCurrent() ? Status : EContextStatus::Idle; }
	[[noreturn]] void Throw();

	// Returns the starting stack address of the innermost call to Closed(), or
	// nullptr if there is no call to Closed. Used to assert that a stack memory
	// write is safe to record.
	// See FTransaction::ShouldRecordWrite()
	inline const void* GetClosedStackAddress() const { return ClosedStackAddress; }

	inline FOpenHashThrottler& GetOpenHashThrottler() { return OpenHashThrottler; }

    void DumpState() const;

	inline void EnteringStaticLocalInitializer()
	{
		if (AUTORTFM_LIKELY(GetStatus() == EContextStatus::Idle))
		{
			return;
		}

		if (Status == EContextStatus::OnTrack)
		{
			AUTORTFM_ASSERT(0 == StackLocalInitializerDepth);
			Status = EContextStatus::InStaticLocalInitializer;
			StackLocalInitializerDepth++;
		}
		else if (Status == EContextStatus::InStaticLocalInitializer)
		{
			StackLocalInitializerDepth++;
		}
	}

	inline void LeavingStaticLocalInitializer()
	{
		if (AUTORTFM_LIKELY(GetStatus() == EContextStatus::Idle))
		{
			return;
		}

		AUTORTFM_ASSERT(Status != EContextStatus::OnTrack);

		if (Status == EContextStatus::InStaticLocalInitializer)
		{
			StackLocalInitializerDepth--;

			if (0 == StackLocalInitializerDepth)
			{
				Status = EContextStatus::OnTrack;
			}
		}
	}

	inline FTaskPool& GetTaskPool() { return TaskPool; }
	inline FTransactionPool& GetTransactionPool() { return TransactionPool; }

private:
	static FContext* Instance;

	FContext() { Reset(); }
    FContext(const FContext&) = delete;

	void MaterializeDeferredTransactions();
	void StartNonDeferredTransaction(EMemoryValidationLevel MemoryValidationLevel);

	void PushCallNest(FCallNest* NewCallNest);
	void PopCallNest();

	FTransaction* PushTransaction(bool bClosed, bool bIsScoped, FStackRange, EMemoryValidationLevel);
	void PopTransaction();

	ETransactionResult ResolveNestedTransaction(FTransaction* NewTransaction);
	bool AttemptToCommitTransaction(FTransaction* const Transaction);
    
    // All of this other stuff ought to be private?
    void Reset();

	static FStackRange GetThreadStackRange();

	// We defer allocating FTransactions at the top of the transaction stack.
	// This allows us to make starting a transaction in the open a load, some math, and a store.
	uint64_t NumDeferredTransactions{0};
    FTransaction* CurrentTransaction{nullptr}; 
	FCallNest* CurrentNest{nullptr};

    FStackRange Stack;
	void* ClosedStackAddress = nullptr;
    EContextStatus Status{EContextStatus::Idle};
	FThreadID CurrentThreadId;
	uint32_t StackLocalInitializerDepth = 0;
	TTask<void()> PostAbortCallback;
	FTaskPool TaskPool;
	FTransactionPool TransactionPool;
	FOpenHashThrottler OpenHashThrottler{
		/* LogInterval            */ 10,   // Log every 10 seconds.
		/* AdjustThrottleInterval */ 0.5,  // Adjust throttling probabilities every 500ms.
		/* TargetFractionHashing  */ 0.1   // At most we want to spent 10% of the time hashing.
	};
};

} // namespace AutoRTFM

#endif // (defined(__AUTORTFM) && __AUTORTFM)
