// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaCacheClient.h"
#include "UbaCacheBucket.h"
#include "UbaCacheServer.h"
#include "UbaConfig.h"
#include "UbaCompactTables.h"
#include "UbaFileAccessor.h"
#include "UbaHashMap.h"
#include "UbaNetworkBackendTcp.h"
#include "UbaNetworkClient.h"
#include "UbaNetworkServer.h"
#include "UbaRootPaths.h"
#include "UbaSessionServer.h"
#include "UbaStorageServer.h"
#include "UbaTest.h"

namespace uba
{
	struct TestRecord
	{
		Set<u32> offsets;
		u32 expectedRangeCount;
		u32 expectedExtraCount;
	};

	bool CheckCacheEntry(CacheEntries& entries, CacheEntry& entry, const TestRecord* rec, bool checkOffsets, bool checkExpectedCounts)
	{
		Set<u32> sharedOffsets;
		{
			BinaryReader sharedReader(entries.sharedInputCasKeyOffsets);
			while (sharedReader.GetLeft())
				if (!sharedOffsets.insert(u32(sharedReader.Read7BitEncoded())).second)
					return false;
		}

		Set<u32> entryOffsets;

		u32 rangeCount = 0;
		BinaryReader rangeReader(entry.sharedInputCasKeyOffsetRanges);
		while (rangeReader.GetLeft())
		{
			++rangeCount;
			u64 begin = rangeReader.Read7BitEncoded();
			u64 end = rangeReader.Read7BitEncoded();
			BinaryReader sharedReader(entries.sharedInputCasKeyOffsets.data(), begin, end);
			while (sharedReader.GetLeft())
			{
				u64 offset;
				if (!sharedReader.TryRead7BitEncoded(offset))
					return false;
				if (!entryOffsets.insert(u32(offset)).second)
					return false;
			}
		}

		u32 extraCount = 0;
		BinaryReader extraReader(entry.extraInputCasKeyOffsets);
		while (extraReader.GetLeft())
		{
			++extraCount;
			u32 offset = u32(extraReader.Read7BitEncoded());
			if (sharedOffsets.find(offset) != sharedOffsets.end())
				return false;
			if (!entryOffsets.insert(offset).second)
				return false;
		}

		if (rec)
		{
			if (checkExpectedCounts)
			{
				if (rangeCount != rec->expectedRangeCount)
					return false;
				if (extraCount != rec->expectedExtraCount)
					return false;
			}

			if (checkOffsets)
			{
				if (rec->offsets != entryOffsets)
					return false;
			}
			else
			{
				if (rec->offsets.size() != entryOffsets.size())
					return false;
			}
		}
		return true;
	}

	void UpdateCacheEntries(LoggerWithWriter& logger, CacheEntries& entries, u32 multiplier)
	{
		HashMap2<u32, u32> oldToNewCasKeyOffset;
		MemoryBlock memory(64*1024);
		oldToNewCasKeyOffset.Init(memory, 100);
		if (multiplier)
			for (u32 i=0; i!=200; ++i)
				oldToNewCasKeyOffset.Insert(i) = i * multiplier;
		Vector<u32> temp;
		Vector<u8> temp2;
		Vector<u8> temp3;
		entries.UpdateEntries(logger, oldToNewCasKeyOffset, temp, temp2, temp3);
	}

	bool TestCacheEntry(LoggerWithWriter& logger, const StringBufferBase& rootDir)
	{
		CacheEntries entries;

		using Record = TestRecord;
		auto CheckEntry = [&](CacheEntry& entry, const Record* rec, bool checkOffsets, bool checkExpectedCounts) { return CheckCacheEntry(entries, entry, rec, checkOffsets, checkExpectedCounts); };

		auto AddEntry = [&](const Record& rec, bool checkExpectedCount)
			{
				Vector<u8> inputOffsets;
				u64 bytes = 0;
				for (u32 i : rec.offsets)
					bytes += Get7BitEncodedCount(i);
				inputOffsets.resize(bytes);
				BinaryWriter writer(inputOffsets.data(), 0, inputOffsets.size());
				for (u32 i : rec.offsets)
					writer.Write7BitEncoded(i);

				CacheEntry entry;
				entries.BuildInputs(entry, rec.offsets);
				entries.entries.push_back(entry);
				return CheckEntry(entry, &rec, true, checkExpectedCount);
			};

		auto UpdateEntries = [&](u32 multiplier) { UpdateCacheEntries(logger, entries, multiplier); };

		auto Clear = [&]()
			{
				entries.entries.clear();
				entries.sharedInputCasKeyOffsets.clear();
				entries.sharedLogLines.clear();
				entries.idCounter = 0;
				entries.primaryId = ~0u; // Id of entry that shared offsets was made from
				entries.inputsThatAreOutputs.clear();
			};

		#if 0
		StorageImpl* storageImpl  = nullptr;
		CompactCasKeyTable* cckt = nullptr;
		#if PLATFORM_WINDOWS
		FileAccessor fa(logger, TC("e:\\temp\\CacheEntry.bin"));
		#else
		FileAccessor fa(logger, TC("/mnt/e/temp/CacheEntry.bin"));
		#endif
		fa.OpenMemoryRead();
		BinaryReader fileReader(fa.GetData(), 0, fa.GetSize());
		entries.ReadFromDisk(logger, fileReader, 9, *storageImpl, *cckt);
		fa.Close();
		//UpdateEntries(0);
		for (auto& entry : entries.entries)
			CHECK_TRUE(CheckEntry(entry, nullptr, false, false));
		return true;
		#endif

		Vector<Record> records0 = 
		{
			{ { 1 }, 1, 0 },
			{ { 0 }, 0, 1 },
		};

		Vector<Record> records1 = 
		{
			{ { 1, 4 }, 1, 0 },
			{ { 1, 2, 3, 4 }, 1, 2 },
			{ { 1, 2, 3, 4, 5 }, 1, 3 },
			{ { 1, 2, 3, 4, 5, 6 }, 1, 4 },
		};

		Vector<Record> records2 = 
		{
			{ { 1, 4, 6 }, 1, 0 },
			{ { 0, 4, 6 }, 1, 1 },
			{ { 2, 4, 6 }, 1, 1 },
			{ { 1, 4, 5 }, 1, 1 },
			{ { 1, 4, 7 }, 1, 1 },
			{ { 1, 3, 6 }, 2, 1 },
			{ { 1, 5, 6 }, 2, 1 },
			{ { 1, 5, 7 }, 1, 2 },
			{ { 1, 3, 5, 7 }, 1, 3 },
			{ { 1, 3, 5, 7, 8 }, 1, 4 },
			{ { 1, 4, 6, 7 }, 1, 1 },
			{ { 1, 4, 6, 7, 8 }, 1, 2 },
			{ { 0, 1, 4, 6 }, 1, 1 },
			{ { 1, 4, 5, 6 }, 1, 1 },
			{ { 0, 1 }, 1, 1 },
			{ { 0, 2 }, 0, 2 },
			{ { 0, 4 }, 1, 1 },
			{ { 0, 5 }, 0, 2 },
			{ { 0, 6 }, 1, 1 },
			{ { 0, 7 }, 0, 2 },
			{ { 1, 2 }, 1, 1 },
			{ { 1, 4 }, 1, 0 },
			{ { 1, 5 }, 1, 1 },
			{ { 1, 6 }, 2, 0 },
			{ { 1, 7 }, 1, 1 },
			{ { 2, 4 }, 1, 1 },
			{ { 2, 3 }, 0, 2 },
			{ { 7, 8 }, 0, 2 },
			{ { 0 }, 0, 1 },
			{ { 1 }, 1, 0 },
			{ { 2 }, 0, 1 },
			{ { 3 }, 0, 1 },
			{ { 4 }, 1, 0 },
			{ { 5 }, 0, 1 },
			{ { 6 }, 1, 0 },
			{ { 7 }, 0, 1 },
		};

		Vector<Record> records3 = 
		{
			{ { 2, 4, 6, 10, 14, 18 }, 1, 0 },
			{ { 2, 4, 6, 10, 14, 18 }, 1, 0 },
			{ { 2, 4, 5, 10, 15, 18 }, 3, 2 },
			{ { 2, 4, 6, 10, 19, 20 }, 1, 2 },
			{ { 0, 1 }, 0, 2 },
			{ { 4, 10, 18 }, 3, 0 },
			{ { 7, 8 }, 0, 2 },
			{ { 6, 7, 8 }, 1, 2 },
			{ { 5, 6, 7, 8 }, 1, 3 },
			{ { 2, 4, 6, 7, 8, 10, 14 }, 1, 2 },
			{ { 2, 4, 6, 7, 8, 10, 14, 18 }, 1, 2 },
			{ { 7, 8, 10 }, 1, 2 },
			{ { 7, 8, 10, 14, 18 }, 1, 2 },
			{ { 4, 7, 14, 18 }, 2, 1 },
		};

		Vector<Record> records4 = 
		{
			{ { 1, 4, 7 }, 1, 0 },
			{ { 1, 5, 6 }, 1, 2 },
		};

		Vector<Record> records5 = 
		{
			{ { 1, 3, 6 }, 1, 0 },
			{ { 1, 3, 5, 7 }, 1, 2 },
		};

		Vector<Record>* recordGroups[] =
		{
			&records0,
			&records1,
			&records2,
			&records3,
			&records4,
			&records5,
		};

		for (auto recordsPtr : recordGroups)
		{
			auto& records = *recordsPtr;
			Clear();
			for (auto& rec : records)
				CHECK_TRUE(AddEntry(rec, true));
			for (u32 i=0; i!=4; ++i)
			{
				UpdateEntries(i);
				u32 index = 0;
				for (auto& entry : entries.entries)
					CHECK_TRUE(CheckEntry(entry, &records[index++], i < 2, true));
			}
		}

		for (auto recordsPtr : recordGroups)
		{
			auto& records = *recordsPtr;

			for (u32 i=0;i!=records.size(); ++i)
			{
				Clear();
				CHECK_TRUE(AddEntry(records[i], false));

				for (u32 j=0;j!=records.size(); ++j)
				{
					u32 index = (i + j + 1) % records.size();
					CHECK_TRUE(AddEntry(records[index], false));
				}
			}

		}

		return true;
	}

	void GetTestAppPath(LoggerWithWriter& logger, StringBufferBase& out);
	StringKey GetKeyAndFixedName(StringBuffer<>& fixedFilePath, const tchar* filePath);
	void InvalidateCachedInfo(StorageImpl& storage, StringBufferBase& fileName)
	{
		StringBuffer<> fixedFilePath;
		storage.InvalidateCachedFileInfo(GetKeyAndFixedName(fixedFilePath, fileName.data));
	}

	bool TestCompactPathTable(LoggerWithWriter& logger, const StringBufferBase& rootDir)
	{
		for (u32 useCommon=0; useCommon<=1; ++useCommon)
		{
			const tchar* pathsStr[] =
			{
				TC("Foo/Bar/Meh.h"),
				TC("Foo/Bar/Meh.cpp"),
				TC("Foo/Bar/Moo.h"),
				TC("Foo/Boo/Rud.h"),
				TC("Foo/Boo/Rud.cpp"),
				TC(")/Boo/Rud.cpp"),
				TC("%/cl.cpp"),
				TC(")/Boo/Rud.inl"),
			};
			constexpr u32 PathCount = sizeof_array(pathsStr);

			StringBuffer<128> paths[PathCount];
			for (u32 i=0;i!=PathCount; ++i)
				paths[i].Append(pathsStr[i]).FixPathSeparators();

			CompactPathTable table(CaseInsensitiveFs, 0, 0, 3);
			table.InitMem();
			if (useCommon)
				table.AddCommonStringSegments();

			u32 offsets[PathCount];
			for (u32 i=0;i!=PathCount; ++i)
				offsets[i] = table.AddNoLock(paths[i].data, paths[i].count);

			CompactPathTable table2(CaseInsensitiveFs, 0, 0, 3);
			table2.InitMem();
			if (useCommon)
				table2.AddCommonStringSegments();

			CompactPathTable::AddContext context{table};
			for (u32 i=0;i!=PathCount; ++i)
			{
				u32 offsets2 = table2.AddNoLock(context, offsets[i]);

				StringBuffer<> temp;
				if (!table2.GetString(temp, offsets2))
					return logger.Error(TC("Error getting offset %u from table2"), offsets2);
				if (!temp.Equals(paths[i].data))
					return logger.Error(TC("Error adding %s to table2. Found %s"), paths[i].data, temp.data);
			}
		}

		for (u32 version=0; version<=CacheBucketVersion; ++version)
		{
			CompactPathTable table(CaseInsensitiveFs, 0, 0, version);

			StringBuffer<> str;
			u32 offset;

			auto testStr = [&](const StringView& str)
				{
					StringBuffer<> str2;
					offset = table.Add(str.data, str.count);
					table.GetString(str2, offset);
					return str.Equals(str2.data);
				};

			if (!testStr(str.Append("foo")))
				return false;
			if (!testStr(str.Clear().Append("foo").EnsureEndsWithSlash().Append("bar.h")))
				return false;
			if (!testStr(str.Clear().Append(PathSeparator).Append("foo").Append(PathSeparator).Append("bar.h")))
				return false;

			CompactPathTable table2(CaseInsensitiveFs, 0, 0, version);
			BinaryReader reader(table.GetMemory(), 0, table.GetSize());
			table2.ReadMem(reader, true);
			u32 offset2 = table2.Add(str.data, str.count);
			if (offset != offset2)
				return false;
		}
		return true;
	}

	bool TestCompactCasKeyTable(LoggerWithWriter& logger, const StringBufferBase& rootDir)
	{
		CompactCasKeyTable table;
		u32 offset0 = table.AddNoLock(CasKeyZero, 0);
		u32 offset1 = table.AddNoLock(CasKeyZero, 1);
		u32 offset2 = table.AddNoLock(CasKeyZero, 2);

		CompactCasKeyTable table2;
		BinaryReader reader(table.GetMemory(), 0, table.GetSize());
		table2.ReadMem(reader, true);
		if (table2.AddNoLock(CasKeyZero, 0) != offset0)
			return false;
		if (table2.AddNoLock(CasKeyZero, 1) != offset1)
			return false;
		if (table2.AddNoLock(CasKeyZero, 2) != offset2)
			return false;

		for (u32 i=0;i!=32;++i)
		{
			CompactCasKeyTable table3(table.GetKeyCount());
			for (u32 j=0;j!=i;++j)
				table3.AddNoLock(CasKeyZero, j);
		}
		return true;
	}

	bool TestHashTable(LoggerWithWriter& logger, const StringBufferBase& rootDir)
	{
		{
			MemoryBlock memoryBlock(1024*1024);
			HashMap<u32, u32> casMap;
			casMap.Init(memoryBlock, 3);
			if (casMap.Find(1))
				return false;
			casMap.Insert(1) = 2;
			if (*casMap.Find(1) != 2)
				return false;
			casMap.Insert(1) = 3;
			if (*casMap.Find(1) != 3)
				return false;

			HashMap<u32, u32, true> casMap2;
			casMap2.Init(4);
			for (u32 i=0;i!=4; ++i)
			{
				casMap2.Insert(i) = i;
				if (*casMap2.Find(i) != i)
					return false;
			}
			for (u32 i=0;i!=4; ++i)
				if (*casMap2.Find(i) != i)
					return false;

			casMap2.Insert(4) = 4;
			for (u32 i=0;i!=5; ++i)
				if (*casMap2.Find(i) != i)
					return false;

			for (u32 i=5;i!=1000; ++i)
			{
				if (casMap2.Find(i))
					return false;
				casMap2.Insert(i) = i;
				if (*casMap2.Find(i) != i)
					return false;
			}
		}

#if 0
		struct CasFileInfo { CasFileInfo(u32 s = 0) : size(s) {} u32 size; bool isUsed; }; // These are compressed cas, should never be over 4gb
		constexpr u64 memoryReserveSize = 192*1024*1024;
		struct ProfileScope
		{
			ProfileScope() : startTime(GetTime()) {}
			~ProfileScope() { u64 duration = GetTime() - startTime; LoggerWithWriter(g_consoleLogWriter, TC("")).Info(TC("Time: %s"), TimeToText(duration).str); }
			u64 startTime;
		};

		u32 totalCasCount = 1'800'000;

		{
			ProfileScope _;
			MemoryBlock memoryBlock;
			if (!memoryBlock.Init(memoryReserveSize, nullptr, true))
				memoryBlock.Init(memoryReserveSize);

			HashMap<CasKey, CasFileInfo> casMap;
			casMap.Init(memoryBlock, totalCasCount);

			CasKey key;

			for (u32 i=0; i!=totalCasCount; ++i)
			{
				key.a = i;
				casMap.Insert(key);
			}

			for (u32 i=0; i!=totalCasCount; ++i)
			{
				key.a = i;
				casMap.Find(key);
			}
		}

		{
			ProfileScope _;
			MemoryBlock memoryBlock;
			if (!memoryBlock.Init(memoryReserveSize, nullptr, true))
				memoryBlock.Init(memoryReserveSize);

			GrowingNoLockUnorderedMap<CasKey, CasFileInfo> casMap(&memoryBlock);
			casMap.reserve(totalCasCount);

			CasKey key;

			for (u32 i=0; i!=totalCasCount; ++i)
			{
				key.a = i;
				casMap.try_emplace(key);
			}

			for (u32 i=0; i!=totalCasCount; ++i)
			{
				key.a = i;
				auto it = casMap.find(key);
			}
		}
#endif
		return true;
	}

	bool TestCacheClientAndServer(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		LogWriter& logWriter = logger.m_writer;
		NetworkBackendTcp tcpBackend(logWriter);

		StringBuffer<MaxPath> rootDir;
		rootDir.Append(testRootDir).Append(TCV("Uba"));

		StringBuffer<MaxPath> inputFile;
		StringBuffer<MaxPath> outputFile;

		{
			bool ctorSuccess = true;
			NetworkServer server(ctorSuccess, { logWriter });

			CHECK_TRUE(DeleteAllFiles(logger, rootDir.data));

			StorageServerCreateInfo storageServerInfo(server, rootDir.data, logWriter);
			storageServerInfo.casCapacityBytes = 1024ull * 1024 * 1024;
			auto& storageServer = *new StorageServer(storageServerInfo);
			auto ssg = MakeGuard([&]() { delete &storageServer; });

			CacheServerCreateInfo csci(storageServer, rootDir.data, logWriter);
			CacheServer cacheServer(csci);
			CHECK_TRUE(cacheServer.Load(false));

			SessionServerCreateInfo sessionInfo(storageServer, server, logWriter);
			sessionInfo.rootDir = rootDir.data;
			auto& session = *new SessionServer(sessionInfo);
			auto sg = MakeGuard([&]() { delete &session; });

			u16 port = 1356;
			CHECK_TRUE(server.StartListen(tcpBackend, port));
			auto disconnectServer = MakeGuard([&]() { server.DisconnectClients(); });

			StringBuffer<MaxPath> workingDir;
			workingDir.Append(testRootDir).Append(TCV("WorkingDir"));
			CHECK_TRUE(DeleteAllFiles(logger, workingDir.data));
			CHECK_TRUE(storageServer.CreateDirectory(workingDir.data));
			CHECK_TRUE(DeleteAllFiles(logger, workingDir.data, false));

			StringBuffer<> testApp;
			GetTestAppPath(logger, testApp);

			CHECK_TRUE(CreateTestFile(inputFile, logger, workingDir, TCV("Input.txt"), TCV("Foo")));
			CHECK_TRUE(CreateTestFile(outputFile, logger, workingDir, TCV("Output.exe"), TCV("Foo"), DefaultAttributes(true)));

			StackBinaryWriter<256> inputs;
			inputs.WriteString(inputFile);

			StackBinaryWriter<256> outputs;
			outputs.WriteString(outputFile);

			StackBinaryWriter<256> logLines;
			logLines.WriteString(TC("Hello"));
			logLines.WriteByte(1);

			ProcessStartInfo psi;
			psi.application = testApp.data;

			{
				NetworkClient client(ctorSuccess, { logWriter });
				CacheClientCreateInfo ccci(logWriter, storageServer, client, session);
				ccci.useRoots = false;
				CacheClient cacheClient(ccci);

				CHECK_TRUE(client.Connect(tcpBackend, TC("127.0.0.1"), port));
				auto disconnectClient = MakeGuard([&]() { client.Disconnect(); });

				//cacheClient.RegisterPathHash(workingDir.data, CasKey(1, 2, 3));

				{
					CacheResult result;
					CHECK_TRUE(!cacheClient.FetchFromCache(result, RootPaths(), 0, psi) && !result.hit);
					CHECK_TRUE(cacheClient.WriteToCache(RootPaths(), 0, psi, inputs.GetData(), inputs.GetPosition(), outputs.GetData(), outputs.GetPosition(), logLines.GetData(), logLines.GetPosition()));
					CHECK_TRUE(DeleteFileW(outputFile.data));
					CHECK_TRUE(!FileExists(logger, outputFile.data));
					CHECK_TRUE(cacheClient.FetchFromCache(result, RootPaths(), 0, psi));
					CHECK_TRUE(FileExists(logger, outputFile.data));
					CHECK_TRUE(result.logLines.size() == 1);
					CHECK_TRUE(result.logLines[0].text == TC("Hello"));
				}

				{
					CHECK_TRUE(DeleteFileW(inputFile.data));
					CHECK_TRUE(CreateTestFile(inputFile, logger, workingDir, TCV("Input.txt"), TCV("Bar")));
					InvalidateCachedInfo(storageServer, inputFile);

					CacheResult result;
					CHECK_TRUE(!cacheClient.FetchFromCache(result, RootPaths(), 0, psi) && !result.hit);

					CHECK_TRUE(cacheClient.WriteToCache(RootPaths(), 0, psi, inputs.GetData(), inputs.GetPosition(), outputs.GetData(), outputs.GetPosition(), logLines.GetData(), logLines.GetPosition()));

					CHECK_TRUE(DeleteFileW(outputFile.data));
					CHECK_TRUE(!FileExists(logger, outputFile.data));
					CHECK_TRUE(cacheClient.FetchFromCache(result, RootPaths(), 0, psi));
					CHECK_TRUE(FileExists(logger, outputFile.data));

					#if !PLATFORM_WINDOWS
					CHECK_TRUE(IsExecutable(GetFileAttributesW(outputFile.data)));
					#endif

					CHECK_TRUE(result.logLines.size() == 1);
					CHECK_TRUE(result.logLines[0].text == TC("Hello"));
				}
			}

			CHECK_TRUE(cacheServer.RunMaintenance(true, true, []() { return false; }));

			{
				NetworkClient client(ctorSuccess, { logWriter });
				CacheClientCreateInfo ccci(logWriter, storageServer, client, session);
				ccci.useRoots = false;
				CacheClient cacheClient(ccci);

				CHECK_TRUE(client.Connect(tcpBackend, TC("127.0.0.1"), port));
				auto disconnectClient = MakeGuard([&]() { client.Disconnect(); });

				{
					CacheResult result;
					CHECK_TRUE(cacheClient.FetchFromCache(result, RootPaths(), 0, psi));
					CHECK_TRUE(result.logLines.size() == 1);
					CHECK_TRUE(result.logLines[0].text == TC("Hello"));
				}
			}

			CHECK_TRUE(cacheServer.Save());
			CHECK_TRUE(storageServer.SaveCasTable(true));
		}

		{
			bool ctorSuccess = true;
			NetworkServer server(ctorSuccess, { logWriter });
			StorageServerCreateInfo storageServerInfo(server, rootDir.data, logWriter);
			storageServerInfo.casCapacityBytes = 1024ull * 1024 * 1024;
			StorageServer storageServer(storageServerInfo);
			storageServer.LoadCasTable();

			StorageImpl::FileEntry* inputFileEntry;
			StorageImpl::FileEntry* outputFileEntry;
			CHECK_TRUE(storageServer.GetFileEntry(inputFileEntry, inputFile.data));
			CHECK_TRUE(storageServer.GetFileEntry(outputFileEntry, outputFile.data));

			#if UBA_TRACK_IS_EXECUTABLE
			CHECK_TRUE(!inputFileEntry->isExecutable);
			CHECK_TRUE(outputFileEntry->isExecutable);
			#endif

			CacheServerCreateInfo csci(storageServer, rootDir.data, logWriter);
			CacheServer cacheServer(csci);
			CHECK_TRUE(cacheServer.Load(false));
		}
		return true;
	}

#if 0
	bool TestLoadCache(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		#if PLATFORM_WINDOWS
		const tchar* fileName = TC("e:\\temp\\uploads\\114629205754");
		#else
		const tchar* fileName = TC("/mnt/e/temp/uploads/114629205754");
		#endif

		FileAccessor bucketFile(logger, fileName);
		if (!bucketFile.OpenMemoryRead())
			return false;
		BinaryReader reader(bucketFile.GetData(), 0, bucketFile.GetSize());
		u32 bucketVersion = reader.ReadU32();
		CacheBucket::LoadStats stats;
		StorageServer* storage = nullptr;

		CacheBucket bucket(0, 0);
		if (!bucket.Load(logger, reader, bucketVersion, stats, *storage))
			return false;
		bucketFile.Close();

		LoggerWithWriter lg(g_consoleLogWriter);
		u32 entriesIndex = 0;
		for (auto& kv : bucket.m_cacheEntryLookup)
		{
			lg.Info(TC("Entries %u"), entriesIndex++);
			auto& entries = kv.second;

			if (entriesIndex == 358)
			{
				FileAccessor fa(logger, TC("e:\\temp\\CacheEntry.bin"));
				fa.CreateMemoryWrite(false, DefaultAttributes(), entries.GetTotalSize(CacheNetworkVersion, true));
				BinaryWriter w(fa.GetData(), 0, fa.GetSize());
				entries.Write(w, CacheNetworkVersion, true);
				fa.Close();
			}

			u32 entryIndex = 0;
			for (auto& entry : entries.entries)
				CHECK_TRUE(CheckCacheEntry(entries, entry, nullptr, false, false));
		}
		return true;
	}
#endif
}