// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "AutoRTFM.h"
#include "Templates/TypeHash.h"
#include "VVMCell.h"
#include "VVMContext.h"
#include "VVMLog.h"
#include "VVMPtrVariant.h"
#include "VVMWriteBarrier.h"
#include <Containers/Array.h>
#include <Containers/Set.h>

namespace Verse
{

struct FMarkStack;

struct FTransactionLog
{
	struct FEntry
	{
		uintptr_t Key() { return Slot.RawPtr(); }

		using FSlot = TPtrVariant<TWriteBarrier<VValue>*, TWriteBarrier<TAux<void>>*>;

		FSlot Slot;      // The memory location we write OldValue to into on abort.
		uint64 OldValue; // VValue or TAux<void> depending on how Slot is encoded.
		static_assert(sizeof(OldValue) == sizeof(VValue));
		static_assert(sizeof(OldValue) == sizeof(TAux<void>));

		FEntry(TWriteBarrier<VValue>& InSlot, VValue OldValue)
			: Slot(&InSlot)
			, OldValue(OldValue.GetEncodedBits())
		{
		}

		FEntry(TWriteBarrier<TAux<void>>& InSlot, TAux<void> OldValue)
			: Slot(&InSlot)
			, OldValue(BitCast<uint64>(OldValue.GetPtr()))
		{
		}

		void Abort(FAccessContext Context)
		{
			if (Slot.Is<TWriteBarrier<TAux<void>>*>())
			{
				TWriteBarrier<TAux<void>>* AuxSlot = Slot.As<TWriteBarrier<TAux<void>>*>();
				AuxSlot->Set(Context, TAux<void>(BitCast<void*>(OldValue)));
			}
			else
			{
				TWriteBarrier<VValue>* ValueSlot = Slot.As<TWriteBarrier<VValue>*>();
				ValueSlot->Set(Context, VValue::Decode(OldValue));
			}
		}

		void MarkReferencedCells(FMarkStack&);
	};

	FTransactionLog(const FTransactionLog&) = delete;
	FTransactionLog& operator=(const FTransactionLog&) = delete;

	static constexpr uint32 InitialCapacity = 4;

	uint64* Table = InlineTable;
	FEntry* Log = BitCast<FEntry*>(static_cast<char*>(InlineLog));

	uint64 InlineTable[InitialCapacity];
	alignas(alignof(FEntry)) char InlineLog[InitialCapacity * sizeof(FEntry)];

	uint32 Num = 0;
	uint32 TableCapacity = InitialCapacity;
	// TODO: It's conceivable we could make LogCapacity a function of TableCapacity.
	// But we're just doing the simple thing for now.
	uint32 LogCapacity = InitialCapacity;

private:
	FORCEINLINE bool IsInline() { return Table == InlineTable; }

	FORCEINLINE bool ShouldGrowTable()
	{
		return 2 * Num > TableCapacity;
	}

	static uint64* FindBucket(uint64 Entry, uint64* Table, uint32 Capacity, bool& bIsNewEntry)
	{
		checkSlow(Capacity
				  && (Capacity & (Capacity - 1)) == 0);
		// We use a simple linear probing hash table.
		uint32 Mask = Capacity - 1;
		uint32 Index = ::GetTypeHash(Entry) & Mask;
		while (1)
		{
			if (!Table[Index] || Table[Index] == Entry)
			{
				bIsNewEntry = !Table[Index];
				return &Table[Index];
			}
			Index = (Index + 1) & Mask;
		}
	}

	FORCENOINLINE void GrowTable(FAllocationContext Context)
	{
		uint32 NewCapacity = TableCapacity * 2;
		if (TableCapacity == InitialCapacity)
		{
			NewCapacity *= 2;
		}

		size_t AllocationSize = sizeof(uint64) * NewCapacity;
		uint64* NewTable = BitCast<uint64*>(Context.AllocateAuxCell(AllocationSize));
		memset(NewTable, 0, AllocationSize);

		for (uint32 I = 0; I < Num; ++I)
		{
			bool bIsNewEntry;
			*FindBucket(Log[I].Key(), NewTable, NewCapacity, bIsNewEntry) = Log[I].Key();
		}

		TableCapacity = NewCapacity;
		Table = NewTable;
	}

	void AddToInlineHashTable(FAllocationContext Context, FEntry Entry)
	{
		for (uint32 I = 0; I < InitialCapacity; ++I)
		{
			if (!Table[I])
			{
				Table[I] = Entry.Key();
				AppendToLog(Context, Entry);
				return;
			}
			if (Entry.Key() == Table[I])
			{
				return;
			}
		}

		GrowTable(Context);
		AddToHashTable(Context, Entry);
	}

	void AddToHashTable(FAllocationContext Context, FEntry Entry)
	{
		bool bIsNewEntry;
		uint64* Bucket = FindBucket(Entry.Key(), Table, TableCapacity, bIsNewEntry);
		if (bIsNewEntry)
		{
			*Bucket = Entry.Key();
			AppendToLog(Context, Entry);
			if (ShouldGrowTable())
			{
				GrowTable(Context);
			}
		}
	}

	void AppendToLog(FAllocationContext Context, FEntry Entry)
	{
		if (Num == LogCapacity)
		{
			uint32 NewCapacity = LogCapacity * 2;
			FEntry* NewLog = BitCast<FEntry*>(Context.AllocateAuxCell(NewCapacity * sizeof(FEntry)));
			memcpy(NewLog, Log, Num * sizeof(FEntry));
			LogCapacity = NewCapacity;
			Log = NewLog;
		}

		Log[Num] = Entry;
		++Num;
	}

	void AddImpl(FAllocationContext Context, FEntry Entry)
	{
		checkSlow(Entry.Key() != 0);
		if (IsInline())
		{
			AddToInlineHashTable(Context, Entry);
		}
		else
		{
			AddToHashTable(Context, Entry);
		}
	}

public:
	FTransactionLog()
	{
		checkSlow(IsInline());
		memset(Table, 0, InitialCapacity * sizeof(uint64));
	}

	template <typename T>
	void Add(FAllocationContext Context, TWriteBarrier<T>& Slot)
	{
		AddImpl(Context, FEntry{Slot, Slot.Get()});
	}

	void Join(FAllocationContext Context, FTransactionLog& Child)
	{
		for (uint32 I = 0; I < Child.Num; ++I)
		{
			AddImpl(Context, Child.Log[I]);
		}
	}

	void Abort(FAccessContext Context)
	{
		for (uint32 I = 0; I < Num; ++I)
		{
			Log[I].Abort(Context);
		}
	}
};

struct FTransaction
{
	FTransactionLog Log;
	FTransaction* Parent{nullptr};
	bool bHasStarted{false};
	bool bHasCommitted{false};
	bool bHasAborted{false};

	// Note: We can Abort before we Start because of how leniency works. For example, we can't
	// Start the transaction until the effect token is concrete, but the effect token may become
	// concrete after failure occurs.
	void Start(FRunningContext Context)
	{
		V_DIE_IF(bHasCommitted);
		V_DIE_IF(bHasStarted);
		V_DIE_IF(Parent);
		bHasStarted = true;

		if (!bHasAborted)
		{
			AutoRTFM::ForTheRuntime::StartTransaction();
			Parent = Context.CurrentTransaction();
			Context.SetCurrentTransaction(this);
		}
	}

	// We can't call Commit before we Start because we serialize Start then Commit via the effect token.
	void Commit(FRunningContext Context)
	{
		V_DIE_UNLESS(bHasStarted);
		V_DIE_IF(bHasAborted);
		V_DIE_IF(bHasCommitted);
		bHasCommitted = true;
		AutoRTFM::ForTheRuntime::CommitTransaction();
		if (Parent)
		{
			Parent->Log.Join(Context, Log);
		}
		Context.SetCurrentTransaction(Parent);
	}

	// See above comment as to why we might Abort before we start.
	void Abort(FRunningContext Context)
	{
		V_DIE_IF(bHasCommitted);
		V_DIE_IF(bHasAborted);
		bHasAborted = true;
		if (bHasStarted)
		{
			V_DIE_UNLESS(Context.CurrentTransaction() == this);
			AutoRTFM::ForTheRuntime::RollbackTransaction();
			AutoRTFM::ForTheRuntime::ClearTransactionStatus();
			Log.Abort(Context);
			Context.SetCurrentTransaction(Parent);
		}
		else
		{
			V_DIE_IF(Parent);
		}
	}

	template <typename T>
	void LogBeforeWrite(FAllocationContext Context, TWriteBarrier<T>& Slot)
	{
		Log.Add(Context, Slot);
	}
};

} // namespace Verse
#endif // WITH_VERSE_VM
