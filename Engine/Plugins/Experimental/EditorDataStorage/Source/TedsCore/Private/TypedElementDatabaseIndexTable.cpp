// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseIndexTable.h"

#include "Async/ParallelFor.h"
#include "Elements/Common/TypedElementDataStorageLog.h"
#include "GenericPlatform/GenericPlatformTime.h"
#include "Hash/CityHash.h"
#include "TypedElementDatabase.h"
#include "TypedElementDataStorageProfilingMacros.h"

namespace UE::Editor::DataStorage
{
	const FTimespan FMappingTable::FCleanUpInfo::TargetDuration = FTimespan::FromMicroseconds(2000.0);
	const FTimespan FMappingTable::FCleanUpInfo::JobShrinkThreshold = FTimespan::FromMicroseconds(100);
	const FTimespan FMappingTable::FCleanUpInfo::JobGrowthThreshold = FTimespan::FromMicroseconds(500);

	const FTimespan FMappingTable::FCleanUpInfo::MaxBatchDuration = FTimespan::FromMicroseconds(500);
	const FTimespan FMappingTable::FCleanUpInfo::MinBatchDuration = FTimespan::FromMicroseconds(350);

	FMappingTable::FMappingTable(UEditorDataStorage& DataStorage)
		: DataStorage(DataStorage)
	{
	}

	RowHandle FMappingTable::Lookup(EGlobalLockScope LockScope, const FMapKeyView& Key) const
	{
		FScopedSharedLock Lock(LockScope);

		if (int32 Index = FindIndexUnguarded(Key); Index >= 0)
		{
			RowHandle Result = Rows[Index];
			if (IsDirty() && !DataStorage.IsRowAvailableUnsafe(Result))
			{
				Result = InvalidRowHandle;
			}
			return Result;
		}
		else
		{
			return InvalidRowHandle;
		}
	}

	void FMappingTable::Map(EGlobalLockScope LockScope, FMapKey Key, RowHandle Row)
	{
		FScopedExclusiveLock Lock(LockScope);
		IndexRowUnguarded(MoveTemp(Key), Row);
	}

	void FMappingTable::BatchMap(EGlobalLockScope LockScope, TArrayView<TPair<FMapKey, RowHandle>> MapRowPairs)
	{
		FScopedExclusiveLock Lock(LockScope);

		IndexLookupMap.Reserve(IndexLookupMap.Num() + MapRowPairs.Num());

		checkf(Rows.Num() >= FreeList.Num(),
			TEXT("There can't be less rows than there are rows stored in the free list as free list is a subset of rows."));
		int32 ArraySize = Rows.Num() - FreeList.Num() + MapRowPairs.Num();
		Rows.Reserve(ArraySize);
		Keys.Reserve(ArraySize);

		for (TPair<FMapKey, RowHandle>& IndexAndRow : MapRowPairs)
		{
			IndexRowUnguarded(MoveTemp(IndexAndRow.Key), IndexAndRow.Value);
		}
	}

	void FMappingTable::Remap(EGlobalLockScope LockScope, const FMapKeyView& OriginalKey, FMapKey NewKey)
	{
		FScopedExclusiveLock Lock(LockScope);

		uint64 Hash = OriginalKey.CalculateHash();
		for (IndexLookupMapType::TKeyIterator It = IndexLookupMap.CreateKeyIterator(Hash); It; ++It)
		{
			if (Keys[It->Value] == OriginalKey)
			{
				int32 Index = It->Value;
				It.RemoveCurrent();
				IndexLookupMap.Add(NewKey.CalculateHash(), Index);
				Keys[Index] = MoveTemp(NewKey);
			}
		}
	}

	void FMappingTable::Remove(EGlobalLockScope LockScope, const FMapKeyView& Key)
	{
		FScopedExclusiveLock Lock(LockScope);
		
		uint64 Hash = Key.CalculateHash();
		for (IndexLookupMapType::TKeyIterator It = IndexLookupMap.CreateKeyIterator(Hash); It; ++It)
		{
			if (Keys[It->Value] == Key)
			{
				int32 Index = It->Value;
				Rows[Index] = InvalidRowHandle;
				Keys[Index].Clear();
				FreeList.PushFirst(Index);
				It.RemoveCurrent();
			}
		}
	}

	void FMappingTable::MarkDirty()
	{
		DirtyDueToRemoval = true;
	}

	void FMappingTable::RemoveInvalidRows()
	{
		TEDS_EVENT_SCOPE("Index Table clean up");

		FScopedExclusiveLock Lock(EGlobalLockScope::Public);

		FTimespan RemainingTime = FCleanUpInfo::TargetDuration;
		uint64 StartTime = FPlatformTime::Cycles64();

		// If there's no work from a previous pass, then start a new pass if needed.
		if (CleanUpInfo.RemainingJobs == 0 && CleanUpInfo.DeletionQueue.IsEmpty())
		{
			if (DirtyDueToRemoval.load())
			{
				DirtyDueToRemoval = false;
				
				CleanUpInfo.JobCount = (Rows.Num() / CleanUpInfo.BatchSize) + 1;
				CleanUpInfo.RemainingJobs = CleanUpInfo.JobCount;
			}
			else
			{
				return;
			}
		}

		// If there are still batches left from the previous frame or a new pass was just started, then collect invalid rows.
		if (CleanUpInfo.RemainingJobs > 0)
		{
			int32 NumBatches = FMath::Min(CleanUpInfo.RemainingJobs, CleanUpInfo.MaxNumJobs);
			ParallelForTemplate(NumBatches, [this](int32 Block)
				{
					InspectRowBlockForCleanUp(Block);
				}, EParallelForFlags::Unbalanced);

			CleanUpInfo.RemainingJobs -= NumBatches;

			// Follow up with any adjustments needed.
			RemainingTime -= FTimespan(FPlatformTime::Cycles64() - StartTime);
			if (NumBatches == CleanUpInfo.MaxNumJobs) // Only adjust if a full set of batches was used to avoid skewing.
			{
				AdjustJobCount(RemainingTime);
			}
			if (CleanUpInfo.RemainingJobs == 0) // Don't adjust the batches when there are more to be run.
			{
				AdjustBatchSize(CleanUpInfo.JobCount);
			}
		}

		// If there's time left and there are rows to remove, start removing rows.
		if (CleanUpInfo.RemainingJobs == 0 && RemainingTime > FTimespan::Zero() && !CleanUpInfo.DeletionQueue.IsEmpty())
		{
			DrainDeletionQueue(RemainingTime);
		}
		
		UE_LOG(LogEditorDataStorage, Verbose,
			TEXT("TEDS Index Table cleanup - %7.2fms - Has%sremaining rows, Batch size: %i, Job count: %i, Remaining jobs: %i, Max batches: %i"),
			FTimespan(FPlatformTime::Cycles64() - StartTime).GetTotalMilliseconds(),
			CleanUpInfo.DeletionQueue.IsEmpty() ? TEXT(" no ") : TEXT(" "),
			CleanUpInfo.BatchSize,
			CleanUpInfo.JobCount,
			CleanUpInfo.RemainingJobs,
			CleanUpInfo.MaxNumJobs);
	}

	int32 FMappingTable::FindIndexUnguarded(const FMapKey& Key) const
	{
		return FindIndexUnguarded(Key.CalculateHash(), Key);
	}

	int32 FMappingTable::FindIndexUnguarded(uint64 Hash, const FMapKey& Key) const
	{
		for (IndexLookupMapType::TConstKeyIterator It = IndexLookupMap.CreateConstKeyIterator(Hash); It; ++It)
		{
			if (Keys[It->Value] == Key)
			{
				return It->Value;
			}
		}
		return -1;
	}

	int32 FMappingTable::FindIndexUnguarded(const FMapKeyView& Key) const
	{
		return FindIndexUnguarded(Key.CalculateHash(), Key);
	}

	int32 FMappingTable::FindIndexUnguarded(uint64 Hash, const FMapKeyView& Key) const
	{
		for (IndexLookupMapType::TConstKeyIterator It = IndexLookupMap.CreateConstKeyIterator(Hash); It; ++It)
		{
			if (Keys[It->Value] == Key)
			{
				return It->Value;
			}
		}
		return -1;
	}

	bool FMappingTable::IsDirty() const
	{
		return CleanUpInfo.RemainingJobs > 0 || DirtyDueToRemoval || !CleanUpInfo.DeletionQueue.IsEmpty();
	}

	void FMappingTable::IndexRowUnguarded(FMapKey&& Key, RowHandle Row)
	{
		uint64 Hash = Key.CalculateHash();
		if (int32 Index = FindIndexUnguarded(Hash, Key); Index >= 0)
		{
			bool bUpdatingAllowed = IsDirty() && !DataStorage.IsRowAvailableUnsafe(Rows[Index]);
			if (ensureMsgf(bUpdatingAllowed || Row == Rows[Index], TEXT("Another row has already been registered under key '%s'."), *Key.ToString()))
			{
				Rows[Index] = Row; // Update the stored row to the new row.
				return;
			}
		}
		
		// There's no existing row stored under the given key, so create a new one.
		int32 RowIndex;
		if (FreeList.IsEmpty())
		{
			RowIndex = Rows.Add(Row);
			Keys.Add(MoveTemp(Key));
		}
		else
		{
			RowIndex = FreeList.Last();
			FreeList.PopLast();
			Rows[RowIndex] = Row;
			Keys[RowIndex] = MoveTemp(Key);
		}
		IndexLookupMap.Add(Hash, RowIndex);
	}

	void FMappingTable::InspectRowBlockForCleanUp(int32 Block)
	{
		TEDS_EVENT_SCOPE("Index Table inspect rows");

		int32 Index = ((CleanUpInfo.JobCount - CleanUpInfo.RemainingJobs + Block) * CleanUpInfo.BatchSize);

		const RowHandle* Begin = Rows.GetData();
		const RowHandle* Front = Rows.GetData() + Index;
		const RowHandle* End = FMath::Min(Begin + Rows.Num(), Front + CleanUpInfo.BatchSize);
		bool bIsFullBatch = (End - Front) == CleanUpInfo.BatchSize;

		uint64 StartTime = FPlatformTime::Cycles64();
		
		FCleanUpInfo::InvalidRowContainer Result;
		for (; Front != End; ++Front)
		{
			// Skip rows with an invalid handle as those are already freed.
			if (*Front != InvalidRowHandle && !DataStorage.IsRowAvailableUnsafe(*Front))
			{
				Result.Add(static_cast<int32>(Front - Begin));
			}
		}

		if (!Result.IsEmpty())
		{
			CleanUpInfo.DeletionQueue.Enqueue(MoveTemp(Result));
		}

		// Only adjust if this is a full batch, otherwise partial batches skew performance stats.
		if (bIsFullBatch)
		{
			FTimespan Duration(FPlatformTime::Cycles64() - StartTime);
			if (Duration >= FCleanUpInfo::MaxBatchDuration)
			{
				++CleanUpInfo.BatchWentOverTime;
			}
			else if (Duration <= FCleanUpInfo::MinBatchDuration)
			{
				++CleanUpInfo.BatchWentUnderTime;
			}
		}
	}

	void FMappingTable::DrainDeletionQueue(FTimespan RemainingFrameTime)
	{
		TEDS_EVENT_SCOPE("Index Table drain deletion queue");

		uint64 StartTime = FPlatformTime::Cycles64();

		while (!CleanUpInfo.DeletionQueue.IsEmpty() && FTimespan(FPlatformTime::Cycles64() - StartTime) < RemainingFrameTime)
		{
			TOptional<FCleanUpInfo::InvalidRowContainer> OptionalContainer = CleanUpInfo.DeletionQueue.Dequeue();
			checkf(OptionalContainer.IsSet(),
				TEXT("Retrieved an invalid row container from a non-empty deletion queue, but no result was returned"));
			
			FCleanUpInfo::InvalidRowContainer& Container = OptionalContainer.GetValue();
			for (int32 RowIndex : Container)
			{
				ClearRow(RowIndex);
			}
		}
	}

	void FMappingTable::ClearRow(int32 Index)
	{
		FMapKey& Key = Keys[Index];
		uint64 Hash = Key.CalculateHash();
		IndexLookupMap.RemoveSingle(Hash, Index);
		
		Rows[Index] = InvalidRowHandle;
		Keys[Index].Clear();
		FreeList.PushFirst(Index);
	}

	void FMappingTable::AdjustJobCount(FTimespan RemainingFrameTime)
	{
		if (RemainingFrameTime >= FCleanUpInfo::JobGrowthThreshold)
		{
			CleanUpInfo.MaxNumJobs = FMath::RoundToInt32(static_cast<float>(CleanUpInfo.MaxNumJobs) * FCleanUpInfo::JobGrowthFactor);
		}
		else if (RemainingFrameTime <= FCleanUpInfo::JobShrinkThreshold)
		{
			CleanUpInfo.MaxNumJobs = FMath::RoundToInt32(static_cast<float>(CleanUpInfo.MaxNumJobs) * FCleanUpInfo::JobShrinkFactor);
		}

		CleanUpInfo.MaxNumJobs = FMath::Clamp(CleanUpInfo.MaxNumJobs, FCleanUpInfo::MinJobCount, FCleanUpInfo::MaxJobCount);
	}

	void FMappingTable::AdjustBatchSize(int32 JobCount)
	{
		if (CleanUpInfo.BatchWentOverTime.load() >= FCleanUpInfo::BatchShrinkThreshold)
		{
			// The batch size was too big to complete in the allotted time so take a sizable chunk off.
			CleanUpInfo.BatchSize = FMath::RoundToInt32(static_cast<float>(CleanUpInfo.BatchSize) * FCleanUpInfo::BatchShrinkFactor);
		}
		// Check if more than the minimum percentage of jobs took less time than currently allocated.
		else if (CleanUpInfo.BatchWentUnderTime.load() >= 
			static_cast<uint32>(FMath::RoundToInt32(static_cast<float>(JobCount) * FCleanUpInfo::BatchIncreaseThreshold)))
		{
			// The batch size fitted within the allotted time so slightly increase it.
			CleanUpInfo.BatchSize = FMath::RoundToInt32(static_cast<float>(CleanUpInfo.BatchSize) * FCleanUpInfo::BatchGrowthFactor);
		}
		// Clamp the job batch size within reasonable sizes to avoid extremes.
		CleanUpInfo.BatchSize = FMath::Clamp(CleanUpInfo.BatchSize, FCleanUpInfo::MinBatchSize, FCleanUpInfo::MaxBatchSize);
		CleanUpInfo.BatchWentOverTime = 0;
		CleanUpInfo.BatchWentUnderTime = 0;
	}
} // namespace UE::Editor::DataStorage
