// Copyright Epic Games, Inc. All Rights Reserved.

#include "WriteLog.h"
#include "Catch2Includes.h"
#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

TEST_CASE("FWriteLog")
{
	std::mt19937 Rand(0x1234);
	AutoRTFM::FWriteLog WriteLog;

	// Returns a vector with the given size filled with random bytes
	auto RandomBuffer = [&Rand](size_t Size) -> std::vector<std::byte>
	{
		std::vector<std::byte> Out(Size);
		for (size_t I = 0; I < Size; I++)
		{
			Out[I] = static_cast<std::byte>(Rand() & 0xff);
		}
		return Out;
	};

	// Returns a std::vector<size_t> with NumEntries elements all equal to
	// EntrySize
	auto FixedSize = [](size_t EntrySize, size_t NumEntries)
	{
		return std::vector<size_t>(NumEntries, EntrySize);
	};

	// Returns a std::vector<size_t> with NumEntries elements with random values
	// between 1..AutoRTFM::FWriteLogEntry::MaxSize
	auto RandomSize = [&Rand](size_t NumEntries)
	{
		std::vector<size_t> EntrySizes(NumEntries);
		for (size_t& EntrySize : EntrySizes)
		{
			EntrySize = std::max<size_t>(1, Rand() & AutoRTFM::FWriteLogEntry::MaxSize);
		}
		return EntrySizes;
	};

	// The order in which to add data to the write log.
	enum class EEntryOrder
	{
		Forwards,  // Sequentially forward
		Backwards, // Sequentially backwards
		Random,    // Random order
	};

	auto CheckWithOrder = [&](std::vector<size_t> EntrySizes, EEntryOrder EntryOrder)
	{
		// Total number of write entries to test
		const size_t NumEntries = EntrySizes.size();

		// Total number of bytes to add to the write log
		const size_t TotalBufferSize = std::reduce(EntrySizes.begin(), EntrySizes.end());

		// Buffer of random data
		std::vector<std::byte> Data = RandomBuffer(TotalBufferSize);

		// An interval in Data
		struct FDataSpan
		{
			size_t Offset;
			size_t Size;
		};

		// Produce a list of data spans on Data. These will be added to the write log in order.
		std::vector<FDataSpan> DataSpans(NumEntries);
		{
			size_t Offset = 0;
			for (size_t I = 0; I < NumEntries; I++)
			{
				FDataSpan& DataSpan = DataSpans[I];
				DataSpan.Offset = Offset;
				DataSpan.Size = EntrySizes[I];
				Offset += EntrySizes[I];
			}
			
			// Shuffle the spans
			switch (EntryOrder)
			{
				case EEntryOrder::Forwards:
					break; // Already forwards
				case EEntryOrder::Backwards:
					std::reverse(std::begin(DataSpans), std::end(DataSpans));
					break;
				case EEntryOrder::Random:
					std::shuffle(std::begin(DataSpans), std::end(DataSpans), Rand);
					break;
			}
		}
		
		// Populate the write log and Entries with the expected write log entries
		std::vector<AutoRTFM::FWriteLogEntry> Entries;
		for (FDataSpan& DataSpan : DataSpans)
		{
			AutoRTFM::FWriteLogEntry Entry;
			Entry.Data = &Data[DataSpan.Offset];
			Entry.Size = DataSpan.Size;
			Entry.LogicalAddress = reinterpret_cast<std::byte*>(static_cast<uintptr_t>(0x1234000 + DataSpan.Offset));
			Entry.bNoMemoryValidation = (static_cast<int>(Entry.Data[0]) & 0x10) != 0;
			Entries.push_back(Entry);
			WriteLog.Push(Entry);
		}

		REQUIRE(!WriteLog.IsEmpty());
		REQUIRE(WriteLog.Num() <= NumEntries); // WriteLog may append back-to-back entries.
		REQUIRE(WriteLog.TotalSize() == Data.size());

		SECTION("Forwards iterator")
		{
			size_t EntryIndex = 0;
			for (AutoRTFM::FWriteLogEntry Got : WriteLog)
			{
				size_t Offset = 0;
				while (Offset < Got.Size)
				{
					const AutoRTFM::FWriteLogEntry& Expect = Entries[EntryIndex];
					REQUIRE(Got.LogicalAddress + Offset == Expect.LogicalAddress);
					REQUIRE(Got.bNoMemoryValidation == Expect.bNoMemoryValidation);
					REQUIRE(Expect.Size + Offset <= Got.Size);
					REQUIRE(memcmp(Got.Data + Offset, Expect.Data, Expect.Size) == 0);
					EntryIndex++;
					Offset += Expect.Size;
				}
				REQUIRE(Offset == Got.Size);
			}
			REQUIRE(EntryIndex == Entries.size());
		}

		SECTION("Reverse iterator")
		{
			size_t EntryIndex = Entries.size() - 1;
			for (AutoRTFM::FWriteLog::ReverseIterator It = WriteLog.rbegin(); It != WriteLog.rend(); ++It)
			{
				const AutoRTFM::FWriteLogEntry Got = *It;
				size_t Offset = Got.Size;
				while (Offset > 0)
				{
					const AutoRTFM::FWriteLogEntry& Expect = Entries[EntryIndex];
					Offset -= Expect.Size;
					REQUIRE(Offset < Got.Size);
					REQUIRE(Got.LogicalAddress + Offset == Expect.LogicalAddress);
					REQUIRE(Got.bNoMemoryValidation == Expect.bNoMemoryValidation);
					REQUIRE(Expect.Size + Offset <= Got.Size);
					REQUIRE(memcmp(Got.Data + Offset, Expect.Data, Expect.Size) == 0);
					EntryIndex--;
				}
			}
			REQUIRE(EntryIndex + 1 == 0);
		}

		SECTION("Reset")
		{
			WriteLog.Reset();
			REQUIRE(WriteLog.IsEmpty());
			REQUIRE(WriteLog.Num() == 0u);
			REQUIRE(WriteLog.TotalSize() == 0u);
		}
	};
	
	auto Check = [&](std::vector<size_t> EntrySizes)
	{
		SECTION("Forwards")
		{
			CheckWithOrder(std::move(EntrySizes), EEntryOrder::Forwards);
		}
		SECTION("Backwards")
		{
			CheckWithOrder(std::move(EntrySizes), EEntryOrder::Backwards);
		}
		SECTION("Random")
		{
			CheckWithOrder(std::move(EntrySizes), EEntryOrder::Random);
		}
	};

	SECTION("Empty")
	{
		REQUIRE(WriteLog.IsEmpty());
		REQUIRE(WriteLog.Num() == 0u);
		REQUIRE(WriteLog.TotalSize() == 0u);
	}

	SECTION("EntrySize: 1, NumEntries: 32")
	{
		Check(FixedSize(/* EntrySize */ 1, /* NumEntries */ 32));
	}

	SECTION("EntrySize: 32, NumEntries: 65536")
	{
		Check(FixedSize(/* EntrySize */ 32, /* NumEntries */ 65536));
	}

	SECTION("EntrySize: 1024, NumEntries: 32")
	{
		Check(FixedSize(/* EntrySize */ 1024, /* NumEntries */ 32));
	}

	SECTION("EntrySize: FWriteLogEntry::MaxSize-1, NumEntries: 32")
	{
		Check(FixedSize(/* EntrySize */ AutoRTFM::FWriteLogEntry::MaxSize-1, /* NumEntries */ 32));
	}

	SECTION("EntrySize: FWriteLogEntry::MaxSize, NumEntries: 32")
	{
		Check(FixedSize(/* EntrySize */ AutoRTFM::FWriteLogEntry::MaxSize, /* NumEntries */ 32));
	}

	SECTION("EntrySize: random, NumEntries: 32")
	{
		Check(RandomSize(/* NumEntries */ 32));
	}
}

TEST_CASE("FWriteLog.Hash")
{
	struct FMemorySpan
	{
		std::byte* Data;
		size_t Size;
	};

	// Populates Buffer with some pseudo-random data.
	auto FillBuffer = [](FMemorySpan Buffer)
	{
		uint32_t Value = 976187;
		for (size_t Offset = 0; Offset < Buffer.Size; Offset++)
		{
			Value = ((Value >> 3) * 921563) ^ ((Value << 1) * 743917);
			Buffer.Data[Offset] = std::byte(Value & 255);
		}
	};

	// Returns a FWriteLog populated with write log records of size WriteSize,
	// and with a mis-alignment of AlignmentOffset. Records will have at least
	// one byte between them.
	// Writes is populated with all the memory spans for the write records.
	// Gaps is populated with all the memory spans between the write records.
	auto CreateWriteLog = [](
		FMemorySpan Buffer,
		size_t WriteSize,
		size_t AlignmentOffset,
		std::vector<FMemorySpan>& Writes,
		std::vector<FMemorySpan>& Gaps)
	{
		AutoRTFM::FWriteLog WriteLog;
		size_t Offset = AlignmentOffset;
		while (Offset + WriteSize < Buffer.Size)
		{
			AutoRTFM::FWriteLogEntry Entry{};
			Entry.LogicalAddress = Buffer.Data + Offset;
			Entry.Data = Entry.LogicalAddress;
			Entry.Size = WriteSize;
			WriteLog.Push(Entry);

			Writes.push_back(FMemorySpan{Buffer.Data + Offset, WriteSize});

			Offset += WriteSize;

			const size_t GapStart = Offset;
			if (AlignmentOffset == 0)
			{
				// Round up offset to a multiple of WriteSize, but ensuring
				// there's a gap of at least one byte between write log records.
				// This ensures that records are not folded together.
				Offset = AutoRTFM::RoundUp(Offset + 1, WriteSize);
			}
			else
			{
				// Round up offset to a multiple of WriteSize, then add the
				// alignment offset to make the offset misaligned.
				Offset = AutoRTFM::RoundUp(Offset, WriteSize) + AlignmentOffset;
			}
			const size_t GapEnd = std::min(Offset, Buffer.Size);
			Gaps.push_back(FMemorySpan{Buffer.Data + GapStart, GapEnd - GapStart});
		}
		return WriteLog;
	};

	// Tests
	{
		constexpr size_t BufferSize = 1 << 13; // 8KiB
		auto BufferData = std::unique_ptr<std::byte[]>(new std::byte[BufferSize]);
		REQUIRE((reinterpret_cast<uintptr_t>(BufferData.get()) & 15) == 0);
		FMemorySpan Buffer{BufferData.get(), BufferSize};
		
		FillBuffer(Buffer);

		auto Check = [&](size_t WriteSize, size_t AlignmentOffset)
		{
			std::vector<FMemorySpan> Writes, Gaps;
			AutoRTFM::FWriteLog WriteLog = CreateWriteLog(Buffer, WriteSize, AlignmentOffset, Writes, Gaps);

			const AutoRTFM::FWriteLog::FHash OriginalHash = WriteLog.Hash(WriteLog.Num());

			constexpr size_t NumChecks = 100;
			size_t NumChanged = 0;
			for (size_t I = 0; I < NumChecks; I++)
			{
				// Pick a random write.
				FMemorySpan& Write = Writes[(I * 16831) % Writes.size()];
				// Pick a random byte within the write, change it and check the hash changes.
				size_t Offset = (I * 838483) % Write.Size;
				const std::byte OriginalByte = Write.Data[Offset];
				Write.Data[Offset] = ~Write.Data[Offset];
				const AutoRTFM::FWriteLog::FHash NewHash = WriteLog.Hash(WriteLog.Num());
				NumChanged += (OriginalHash != NewHash) ? 1 : 0;
				Write.Data[Offset] = OriginalByte;
			}

			// Expect at least 95% of the byte changes to have affected the hash.
			REQUIRE(NumChanged > (NumChecks * 95 / 100));
			REQUIRE(OriginalHash == WriteLog.Hash(WriteLog.Num()));
			
			// Modify all the padding bytes between write records. These should
			// not affect the hash.
			for (FMemorySpan& Gap : Gaps)
			{
				for (size_t I = 0; I < Gap.Size; I++)
				{
					Gap.Data[I] = ~Gap.Data[I];
				}
			}
			REQUIRE(OriginalHash == WriteLog.Hash(WriteLog.Num()));
		};

		Check(/* WriteSize */ 1, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 2, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 2, /* AlignmentOffset */ 1);
		Check(/* WriteSize */ 3, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 3, /* AlignmentOffset */ 1);
		Check(/* WriteSize */ 3, /* AlignmentOffset */ 2);
		Check(/* WriteSize */ 4, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 4, /* AlignmentOffset */ 1);
		Check(/* WriteSize */ 4, /* AlignmentOffset */ 2);
		Check(/* WriteSize */ 4, /* AlignmentOffset */ 3);
		Check(/* WriteSize */ 5, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 5, /* AlignmentOffset */ 1);
		Check(/* WriteSize */ 5, /* AlignmentOffset */ 3);
		Check(/* WriteSize */ 5, /* AlignmentOffset */ 4);
		Check(/* WriteSize */ 6, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 6, /* AlignmentOffset */ 2);
		Check(/* WriteSize */ 6, /* AlignmentOffset */ 3);
		Check(/* WriteSize */ 6, /* AlignmentOffset */ 5);
		Check(/* WriteSize */ 7, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 7, /* AlignmentOffset */ 1);
		Check(/* WriteSize */ 7, /* AlignmentOffset */ 4);
		Check(/* WriteSize */ 7, /* AlignmentOffset */ 6);
		Check(/* WriteSize */ 8, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 8, /* AlignmentOffset */ 2);
		Check(/* WriteSize */ 8, /* AlignmentOffset */ 3);
		Check(/* WriteSize */ 8, /* AlignmentOffset */ 7);
		Check(/* WriteSize */ 9, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 9, /* AlignmentOffset */ 1);
		Check(/* WriteSize */ 9, /* AlignmentOffset */ 5);
		Check(/* WriteSize */ 9, /* AlignmentOffset */ 8);
		Check(/* WriteSize */ 10, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 10, /* AlignmentOffset */ 3);
		Check(/* WriteSize */ 10, /* AlignmentOffset */ 5);
		Check(/* WriteSize */ 10, /* AlignmentOffset */ 9);
		Check(/* WriteSize */ 15, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 15, /* AlignmentOffset */ 6);
		Check(/* WriteSize */ 15, /* AlignmentOffset */ 9);
		Check(/* WriteSize */ 15, /* AlignmentOffset */ 14);
		Check(/* WriteSize */ 16, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 16, /* AlignmentOffset */ 8);
		Check(/* WriteSize */ 16, /* AlignmentOffset */ 9);
		Check(/* WriteSize */ 16, /* AlignmentOffset */ 15);
		Check(/* WriteSize */ 32, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 32, /* AlignmentOffset */ 8);
		Check(/* WriteSize */ 32, /* AlignmentOffset */ 16);
		Check(/* WriteSize */ 32, /* AlignmentOffset */ 31);
		Check(/* WriteSize */ 64, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 64, /* AlignmentOffset */ 7);
		Check(/* WriteSize */ 64, /* AlignmentOffset */ 30);
		Check(/* WriteSize */ 64, /* AlignmentOffset */ 63);
		Check(/* WriteSize */ 100, /* AlignmentOffset */ 0);
		Check(/* WriteSize */ 100, /* AlignmentOffset */ 1);
		Check(/* WriteSize */ 100, /* AlignmentOffset */ 10);
		Check(/* WriteSize */ 100, /* AlignmentOffset */ 99);
	}

	{
		constexpr size_t BufferSize = 1 << 20; // 1MiB
		auto BufferData = std::unique_ptr<std::byte[]>(new std::byte[BufferSize]);
		REQUIRE((reinterpret_cast<uintptr_t>(BufferData.get()) & 15) == 0);
		FMemorySpan Buffer{BufferData.get(), BufferSize};

		FillBuffer(Buffer);

		auto Benchmark = [&](Catch::Benchmark::Chronometer Meter, size_t WriteSize, size_t AlignmentOffset)
		{
			std::vector<FMemorySpan> Writes, Gaps;
			AutoRTFM::FWriteLog WriteLog = CreateWriteLog(Buffer, WriteSize, AlignmentOffset, Writes, Gaps);
			Meter.measure([&]
			{
				return WriteLog.Hash(WriteLog.Num());
			});
		};

		SECTION("Aligned")
		{
#define BENCH(WRITE_SIZE) \
			BENCHMARK_ADVANCED("WriteSize: " #WRITE_SIZE)(Catch::Benchmark::Chronometer Meter) \
			{ \
				Benchmark(Meter, /* WriteSize */ WRITE_SIZE, /* AlignmentOffset */ 0); \
			};
			BENCH(1);
			BENCH(2);
			BENCH(3);
			BENCH(4);
			BENCH(5);
			BENCH(6);
			BENCH(7);
			BENCH(8);
			BENCH(9);
			BENCH(10);
			BENCH(15);
			BENCH(16);
			BENCH(32);
			BENCH(64);
			BENCH(128);
#undef BENCH
		}
		SECTION("Unaligned")
		{
#define BENCH(WRITE_SIZE) \
			BENCHMARK_ADVANCED("WriteSize: " #WRITE_SIZE)(Catch::Benchmark::Chronometer Meter) \
			{ \
				Benchmark(Meter, /* WriteSize */ WRITE_SIZE, /* AlignmentOffset */ 1); \
			};
			BENCH(1);
			BENCH(2);
			BENCH(3);
			BENCH(4);
			BENCH(5);
			BENCH(6);
			BENCH(7);
			BENCH(8);
			BENCH(9);
			BENCH(10);
			BENCH(15);
			BENCH(16);
			BENCH(32);
			BENCH(64);
			BENCH(128);
#undef BENCH
		}
	}
}
