// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <atomic>
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Deque.h"
#include "Containers/MpscQueue.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Common/TypedElementMapKey.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "GlobalLock.h"
#include "Misc/Timespan.h"

class UEditorDataStorage;

namespace UE::Editor::DataStorage
{
	/**
	 * Storage for a key to row mapping.
	 * Access to the mapping table is thread safe and guarded by the global lock.
	 */

	class FMappingTable final
	{
	public:
		explicit FMappingTable(UEditorDataStorage& DataStorage);

		RowHandle Lookup(EGlobalLockScope LockScope, const FMapKeyView& Key) const;
		void Map(EGlobalLockScope LockScope, FMapKey Key, RowHandle Row);
		void BatchMap(EGlobalLockScope LockScope, TArrayView<TPair<FMapKey, RowHandle>> MapRowPairs);
		void Remap(EGlobalLockScope LockScope, const FMapKeyView& OriginalKey, FMapKey NewKey);
		void Remove(EGlobalLockScope LockScope, const FMapKeyView& Key);

		void MarkDirty();
		void RemoveInvalidRows();

	private:
		struct FCleanUpInfo final
		{
			// The number of row indices to store on the stack before allocating memory on the heap.
			static constexpr uint32 StackReservationCount = 1024;

			using InvalidRowContainer = TArray<int32, TInlineAllocator<StackReservationCount>>;

			// The target amount of time that should be spend per frame on cleaning up old rows.
			static const FTimespan TargetDuration;

			// If less than this time is available after running jobs in a frame, reduce the number of jobs.
			static const FTimespan JobShrinkThreshold;
			// If more than this time is left per frame, increase the number of jobs.
			static const FTimespan JobGrowthThreshold;
			// The rate at which to grow the number of jobs run per frame.
			static constexpr float JobShrinkFactor = 0.7f;
			// The rate at which to shrink the number of jobs run per frame.
			static constexpr float JobGrowthFactor = 1.2f;
			// The minimum required number of jobs.
			static constexpr int32 MinJobCount = 4;
			// The maximum number of allowed jobs.
			static constexpr int32 MaxJobCount = 1024;
			
			// If enough batches exceed this limit, the batch size for jobs is reduced. Aim for 4 batches per thread. Increasing this number
			// increases the spikes that can happen when batches are too big. Decreasing this number increases the overhead of scheduling jobs
			// and makes it more expensive overall.
			static const FTimespan MaxBatchDuration;
			// If enough batches are under this limit, the batch size for jobs is increased.
			static const FTimespan MinBatchDuration;
			// The absolute minimum number of rows to check for validity per batch.
			static constexpr int32 MinBatchSize = 1000;
			// The absolute maximum number of rows to check for validity per batch.
			static constexpr int32 MaxBatchSize = 100'000;
			// The number of jobs that need to take longer than MaxBatchDuration before the batch size gets shrunken down. This is
			// used to mitigate the occasional spike due to for instance the OS handing off work to other programs.
			static constexpr uint32 BatchShrinkThreshold = 2;
			// The portion of jobs that need to be below the MinBatchDuration threshold before the batch size gets increased.
			static constexpr float BatchIncreaseThreshold = 0.9f;
			// The rate at which the batch size gets reduced if enough batches went over time.
			static constexpr float BatchShrinkFactor = 0.8f;
			// The rate at which the batch size gets increased if there's time left.
			static constexpr float BatchGrowthFactor = 1.1f;

			// Queue containing arrays with removed row indices.
			TMpscQueue<InvalidRowContainer> DeletionQueue;
			
			// The maximum number of rows that a single job will process.
			int32 BatchSize = 4096;
			// The number of jobs that had too many batches to process within MaxBatchDuration.
			std::atomic<uint32> BatchWentOverTime = 0;
			// The number of jobs that processed their batches faster than MinBatchDuration.
			std::atomic<uint32> BatchWentUnderTime = 0;
			
			// The maximum number of jobs to run per frame.
			int32 MaxNumJobs = 8;
			// The total number of jobs needed to inspect all indexed rows.
			int32 JobCount = 0;
			// The number of jobs that were not completed in the previous frame.
			int32 RemainingJobs = 0;
		};
		
		using IndexLookupMapType = TMultiMap<uint64, int32>;
		IndexLookupMapType IndexLookupMap;
		TArray<RowHandle> Rows;
		TArray<FMapKey> Keys;
		TDeque<int32> FreeList;

		FCleanUpInfo CleanUpInfo;
		
		UEditorDataStorage& DataStorage;
		std::atomic<bool> DirtyDueToRemoval = false;

		int32 FindIndexUnguarded(const FMapKey& Key) const;
		int32 FindIndexUnguarded(uint64 Hash, const FMapKey& Key) const;
		int32 FindIndexUnguarded(const FMapKeyView& Key) const;
		int32 FindIndexUnguarded(uint64 Hash, const FMapKeyView& Key) const;

		bool IsDirty() const;
		void IndexRowUnguarded(FMapKey&& Key, RowHandle Row);
		
		void InspectRowBlockForCleanUp(int32 Block);
		void DrainDeletionQueue(FTimespan RemainingFrameTime);
		void ClearRow(int32 Index);
		void AdjustJobCount(FTimespan RemainingFrameTime);
		void AdjustBatchSize(int32 JobCount);
	};
} // namespace UE::Editor::DataStorage
