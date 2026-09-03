// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if (defined(__AUTORTFM) && __AUTORTFM)

#include "AutoRTFM.h"
#include "BuildMacros.h"
#include "Utils.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdint.h>

namespace AutoRTFM
{
	struct FWriteLogEntry final
	{
		// Number of bits used by the FWriteLog to represent a write's size.
		static constexpr size_t SizeBits = 15;

		// The maximum size for a single write log entry.
		// Split into multiple entries if the write is too large.
		static constexpr size_t MaxSize = (1u << SizeBits) - 1;

		// The address of the write.
		std::byte* LogicalAddress = nullptr;
		
		// A pointer to the original data before the write occurred.
		std::byte* Data = nullptr;
		
		// The size of the write in bytes. Must be smaller than MaxSize.
		// If the write exceeds MaxSize, then the write must be split into
		// multiple entries.
		size_t Size = 0;
		
		// If true, then this write will not be considered by the AutoRTFM
		// memory validator.
		bool bNoMemoryValidation = false;
	};

	// FWriteLog holds an ordered list of write records which can be iterated
	// forwards and backwards.
	// Ensure changes to this class are kept in sync with Unreal.natvis.
	class FWriteLog final
	{
		struct FRecord
		{
			uintptr_t Address             : 48;
			uintptr_t bNoMemoryValidation : 1;
			uintptr_t Size                : 15;
		};
		static_assert(sizeof(uintptr_t) == 8, "assumption: a pointer is 8 bytes");
		static_assert(sizeof(FRecord) == 8);

		// Ensure changes to this structure are kept in sync with Unreal.natvis.
		struct FBlock final
		{
			// ┌────────┬────┬────┬────┬────┬────────────────┬────┬────┬────┬────┐
			// │ FBlock │ D₀ │ D₁ │ D₂ │ D₃ │->            <-│ R₃ │ R₂ │ R₁ │ R₀ │
			// └────────┴────┴────┴────┴────┴────────────────┴────┴────┴────┴────┘
			//          ^                   ^                ^              ^
			//      DataStart()          DataEnd         LastRecord    FirstRecord
			// Where:
			//   Dₙ = Data n, Rₙ = Record n

			// Size of a heap-allocated block, including the FBlock struct header.
			static constexpr size_t DefaultSize = 2048;

			// Constructor
			// TotalSize is the total size of the allocated memory for the block including
			// the FBlock header.
			explicit FBlock(size_t TotalSize)
			{ 
				AUTORTFM_ENSURE((TotalSize & (alignof(FRecord) - 1)) == 0);
				std::byte* End = reinterpret_cast<std::byte*>(this) + TotalSize;
				DataEnd = DataStart();
				// Note: The initial empty state has LastRecord pointing one
				// FRecord beyond the immutable FirstRecord.
				LastRecord = reinterpret_cast<FRecord*>(End);
				FirstRecord = LastRecord - 1;
			}

			// Allocate performs a heap allocation of a new block.
			// TotalSize is the total size of the allocated memory for the block including
			// the FBlock header.
			static FBlock* Allocate(size_t TotalSize)
			{
				AUTORTFM_ASSERT(TotalSize > (sizeof(FBlock) + sizeof(FRecord)));
				std::byte* Memory = new std::byte[TotalSize];
				// Disable false-positive warning C6386: Buffer overrun while writing to 'Memory'
				CA_SUPPRESS(6386)
				return new (Memory) FBlock(TotalSize);
			}

			// Free releases the heap-allocated memory for this block.
			// Note: This block must have been allocated with a call to Allocate().
			void Free()
			{
				delete [] reinterpret_cast<std::byte*>(this);
			}

			// Returns a pointer to the data for the first entry
			std::byte* DataStart()
			{
				return reinterpret_cast<std::byte*>(this) + sizeof(FBlock);
			}

			// Returns a pointer to the data for the last entry
			std::byte* LastData()
			{
				return DataEnd - LastRecord->Size;
			}

			// Returns true if the block holds no entries.
			bool IsEmpty() const
			{
				return LastRecord > FirstRecord;
			}

			// The result enumerator of Push()
			enum class EPushResult
			{
				// The block does not have enough capacity to fit the entry.
				Full,
				// The block added the entry as a new write.
				Added,
				// The block folded the result into the end of the last write.
				Folded,
			};

			// Attempts to add the entry into this block by copying the entry's data and creating a
			// new record.
			// Returns true if the entry was added, or false if the block does not have the capacity
			// for the entry.
			UE_AUTORTFM_FORCEINLINE EPushResult Push(FWriteLogEntry Entry)
			{
				EPushResult Result = EPushResult::Full;

				if (!IsEmpty() &&
					reinterpret_cast<uintptr_t>(Entry.LogicalAddress) == LastRecord->Address + LastRecord->Size &&
					LastRecord->bNoMemoryValidation == Entry.bNoMemoryValidation)
				{
					if (DataEnd + Entry.Size > reinterpret_cast<std::byte*>(LastRecord))
					{
						// Entry's data does not fit in the block's remaining space.
						return EPushResult::Full;
					}

					LastRecord->Size += Entry.Size;
					Result = EPushResult::Folded;
				}
				else
				{
					if (DataEnd + Entry.Size > reinterpret_cast<std::byte*>(LastRecord - 1))
					{
						// Entry's data + new record does not fit in the block's remaining space.
						return EPushResult::Full;
					}
					LastRecord--;
					LastRecord->Address = reinterpret_cast<uintptr_t>(Entry.LogicalAddress);
					LastRecord->Size = Entry.Size;
					LastRecord->bNoMemoryValidation = Entry.bNoMemoryValidation;
					Result = EPushResult::Added;
				}

				memcpy(DataEnd, Entry.Data, Entry.Size);

				DataEnd += Entry.Size;

#if AUTORTFM_BUILD_DEBUG
				AUTORTFM_ASSERT(DataEnd <= reinterpret_cast<std::byte*>(LastRecord));
#endif

				return Result;
			}

			// The next block in the linked list.
			FBlock* NextBlock = nullptr;
			// The previous block in the linked list.
			FBlock* PrevBlock = nullptr;
			// The pointer to the first entry's record
			FRecord* FirstRecord = nullptr;
			// The pointer to the last entry's record
			FRecord* LastRecord = nullptr;
			// One byte beyond the end of the last entry's data
			std::byte* DataEnd = nullptr;
		private:
			~FBlock() = delete;
		};

	public:
		// Constructor
		FWriteLog()
		{
			new(HeadBlockMemory) FBlock(HeadBlockSize);
		}

		// Destructor
		~FWriteLog()
		{
			Reset();
		}

		// Adds the write log entry to the log.
		// The log will make a copy of the FWriteLogEntry's data.
		void Push(FWriteLogEntry Entry)
		{
			AUTORTFM_ASSERT(Entry.Size <= FWriteLogEntry::MaxSize);
			AUTORTFM_ASSERT((reinterpret_cast<uintptr_t>(Entry.LogicalAddress) & 0xffff0000'00000000) == 0);

			FBlock::EPushResult PushResult = TailBlock->Push(Entry);
			if (PushResult == FBlock::EPushResult::Added)
			{
				NumEntries++;
			}
			else if (AUTORTFM_UNLIKELY(PushResult == FBlock::EPushResult::Full))
			{
				const size_t RequiredSize = AlignUp(sizeof(FBlock) + Entry.Size, alignof(FRecord)) + sizeof(FRecord);
				FBlock* NewBlock = FBlock::Allocate(std::max(RequiredSize, FBlock::DefaultSize));
				NewBlock->PrevBlock = TailBlock;
				TailBlock->NextBlock = NewBlock;
				TailBlock = NewBlock;

				PushResult = NewBlock->Push(Entry);
				AUTORTFM_ASSERT(PushResult == FBlock::EPushResult::Added);
				NumEntries++;
			}

			TotalSizeBytes += Entry.Size;
		}

		// Iterator for enumerating the writes of the log.
		template<bool IS_FORWARD>
		struct TIterator final
		{
			TIterator() = default;

			TIterator(FBlock* StartBlock) : Block(StartBlock)
			{
				if constexpr (IS_FORWARD)
				{
					if (Block->IsEmpty())
					{
						// First block is fixed size and may be empty if the
						// first write is larger than its fixed size.
						Block = Block->NextBlock;
					}
				}

				Data = IS_FORWARD ? Block->DataStart() : Block->LastData();
				Record = IS_FORWARD ? Block->FirstRecord : Block->LastRecord;
			}

			// Returns the entry at the current iterator's position.
			FWriteLogEntry operator*() const
			{
				FWriteLogEntry Entry;
				Entry.LogicalAddress = reinterpret_cast<std::byte*>(Record->Address);
				Entry.Data = Data;
				Entry.Size = Record->Size;
				Entry.bNoMemoryValidation = Record->bNoMemoryValidation;
				return Entry;
			}

			// Progresses the iterator to the next entry
			void operator++()
			{
				if constexpr (IS_FORWARD)
				{
					if (Record == Block->LastRecord)
					{
						Block = Block->NextBlock;
						if (!Block)
						{
							Reset();
							return;
						}
						Data = Block->DataStart();
						Record = Block->FirstRecord;
					}
					else
					{
						Data += Record->Size;
						Record--;
					}
				}
				else
				{
					if (Record == Block->FirstRecord)
					{
						Block = Block->PrevBlock;
						if (!Block || Block->IsEmpty())
						{
							Reset();
							return;
						}
						Data = Block->LastData();
						Record = Block->LastRecord;
					}
					else
					{
						Record++;
						Data -= Record->Size;
					}
				}
			}

			// Inequality operator
			bool operator!=(const TIterator& Other) const
			{
				return (Other.Block != Block) || (Other.Record != Record);
			}

		private:
			// Resets the iterator (compares equal to the write log's end())
			UE_AUTORTFM_FORCEINLINE void Reset()
			{
				Block = nullptr;
				Data = nullptr;
				Record = nullptr;
			}

			FBlock* Block = nullptr;
			std::byte* Data = nullptr;
			FRecord* Record = nullptr;
		};

		using Iterator = TIterator</* IS_FORWARD */ true>;
		using ReverseIterator = TIterator</* IS_FORWARD */ false>;

		Iterator begin() const
		{
			return (NumEntries > 0) ? Iterator(HeadBlock) : Iterator{};
		}
		ReverseIterator rbegin() const
		{
			return (NumEntries > 0) ? ReverseIterator(TailBlock) : ReverseIterator{};
		}
		Iterator end() const { return Iterator{}; }
		ReverseIterator rend() const { return ReverseIterator{}; }

		// Resets the write log to its initial state, freeing any allocated memory.
		void Reset()
		{
			// Skip HeadBlock, which is held as part of this structure.
			FBlock* Block = HeadBlock->NextBlock; 
			while (nullptr != Block)
			{
				FBlock* const Next = Block->NextBlock;
				Block->Free();
				Block = Next;
			}
			new (HeadBlockMemory) FBlock(HeadBlockSize - sizeof(FBlock));
			HeadBlock = reinterpret_cast<FBlock*>(HeadBlockMemory);
			TailBlock = reinterpret_cast<FBlock*>(HeadBlockMemory);
			NumEntries = 0;
			TotalSizeBytes = 0;
		}

		// Returns true if the log holds no entries.
		UE_AUTORTFM_FORCEINLINE bool IsEmpty() const { return 0 == NumEntries; }

		// Return the number of entries in the log.
		UE_AUTORTFM_FORCEINLINE size_t Num() const { return NumEntries; }

		// Return the total size in bytes for all entries in the log.
		UE_AUTORTFM_FORCEINLINE size_t TotalSize() const { return TotalSizeBytes; }

		// Returns a hash of the first NumWriteEntries entries' logical memory
		// tracked by the write log. This is the memory post-write, not the
		// original memory that would be restored on abort.
		using FHash = uint64_t;
		FHash Hash(size_t NumWriteEntries) const;

	private:
		template<size_t SIZE>
		static constexpr bool IsAlignedForTRecord = (SIZE & (alignof(FRecord) - 1)) == 0;

		static constexpr size_t DefaultBlockSize = sizeof(FBlock) + FBlock::DefaultSize;
		static constexpr size_t HeadBlockSize = 256;
		static_assert(IsAlignedForTRecord<HeadBlockSize>);
		static_assert(IsAlignedForTRecord<DefaultBlockSize>);

		FHash HashAVX2(size_t NumWriteEntries) const;

		FBlock* HeadBlock = reinterpret_cast<FBlock*>(HeadBlockMemory);
		FBlock* TailBlock = reinterpret_cast<FBlock*>(HeadBlockMemory);
		size_t NumEntries = 0;
		size_t TotalSizeBytes = 0;
		alignas(alignof(FBlock)) std::byte HeadBlockMemory[HeadBlockSize];
	};
}

#endif // (defined(__AUTORTFM) && __AUTORTFM)
