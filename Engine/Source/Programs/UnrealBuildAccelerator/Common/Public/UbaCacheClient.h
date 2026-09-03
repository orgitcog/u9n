// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaHash.h"
#include "UbaLogger.h"
#include "UbaProcessHandle.h"

namespace uba
{
	class CompactCasKeyTable;
	class CompactPathTable;
	class Config;
	class NetworkClient;
	class RootPaths;
	class Session;
	class StorageImpl;
	struct CacheStats;
	struct CasKey;
	struct ProcessStartInfo;
	struct StorageStats;
	struct StringView;
	struct TrackWorkScope;
	using RootsHandle = u64;

	struct CacheClientCreateInfo
	{
		CacheClientCreateInfo(LogWriter& w, StorageImpl& st, NetworkClient& c, Session& se) : writer(w), storage(st), client(c), session(se) {}
		LogWriter& writer;
		StorageImpl& storage;
		NetworkClient& client;
		Session& session;

		void Apply(Config& config, const tchar* tableName = TC("CacheClient"));

		bool reportCacheKey = false;
		bool reportMissReason = false; // Report the reason no matching cache entry was found
		bool useDirectoryPreparsing = true; // This is used to minimize syscalls. GetFileAttributes can be very expensive on cloud machines and we can enable this to minimize syscall count
		bool validateCacheWritesInput = false; // Set to true to validate cas of all input files before sent to cache
		bool validateCacheWritesOutput = false; // Set to true to validate cas of all output files before sent to cache
		bool useRoots = true; // Set this to false to allow paths that are not under roots and to not fix them up
		bool useCacheHit = true; // Set this to false to ignore found cache hits.. this is for debugging/testing only
		const tchar* hint = TC(""); // Hint will show up in cache server log
	};

	struct CacheResult
	{
		bool hit = false;
		Vector<ProcessLogLine> logLines;
	};

	class CacheClient
	{
	public:
		CacheClient(const CacheClientCreateInfo& info);
		~CacheClient();

		bool RegisterPathHash(const tchar* path, const CasKey& hash);

		bool WriteToCache(u32 bucketId, const ProcessStartInfo& info, const u8* inputs, u64 inputsSize, const u8* outputs, u64 outputsSize, const u8* logLines, u64 logLinesSize, u32 processId = 0);
		bool WriteToCache(const RootPaths& rootPaths, u32 bucketId, const ProcessStartInfo& info, const u8* inputs, u64 inputsSize, const u8* outputs, u64 outputsSize, const u8* logLines, u64 logLinesSize, u32 processId = 0);
		bool FetchFromCache(CacheResult& outResult, RootsHandle rootsHandle, u32 bucketId, const ProcessStartInfo& info);
		bool FetchFromCache(CacheResult& outResult, const RootPaths& rootPaths, u32 bucketId, const ProcessStartInfo& info);
		bool RequestServerShutdown(const tchar* reason);

		bool ExecuteCommand(Logger& logger, const tchar* command, const tchar* destinationFile = nullptr, const tchar* additionalInfo = nullptr);

		inline MutableLogger& GetLogger() { return m_logger; }
		inline NetworkClient& GetClient() { return m_client; }
		inline StorageImpl& GetStorage() { return m_storage; }
		inline Session& GetSession() { return m_session; }

	private:
		struct Bucket;
		u64 MakeId(u32 bucketId);

		// No inlining because they use lots of stack space
		UBA_NOINLINE bool SendPathTable(Bucket& bucket, u32 requiredPathTableSize);
		UBA_NOINLINE bool SendCasTable(Bucket& bucket, u32 requiredCasTableSize);
		UBA_NOINLINE bool SendCacheEntry(TrackWorkScope& tws, Bucket& bucket, const RootPaths& rootPaths, const CasKey& cmdKey, const Map<u32, u32>& inputsStringToCasKey, const Map<u32, u32>& outputsStringToCasKey, const u8* logLines, u64 logLinesSize, u64& outBytesSent);
		UBA_NOINLINE bool SendCacheEntryMessage(BinaryReader& out, Bucket& bucket, const CasKey& cmdKey, const Map<u32, u32>& inputsStringToCasKey, const Map<u32, u32>& outputsStringToCasKey, const u8* logLines, u64 logLinesSize);
		bool FetchCasTable(TrackWorkScope& tws, Bucket& bucket, CacheStats& stats, u32 requiredCasTableOffset);
		UBA_NOINLINE bool FetchCasTable2(TrackWorkScope& tws, Bucket& bucket, CacheStats& stats, u32 requiredCasTableOffset);
		UBA_NOINLINE bool FetchFile(Bucket& bucket, const RootPaths& rootPaths, const ProcessStartInfo& info, CacheStats& cacheStats, StorageStats& storageStats, u32 casKeyOffset);
		template<typename TableType> bool FetchCompactTable(u32 bucketId, TableType& table, u32 requiredTableOffset, u8 messageType);
		bool HasEnoughData(Bucket& bucket, u32 requiredCasTableOffset);
		bool HasEnoughCasData(Bucket& bucket, u32 requiredCasTableOffset, u32& outPathOffset);
		bool HasEnoughPathData(Bucket& bucket, u32 requiredPathTableOffset);
		UBA_NOINLINE bool ReportUsedEntry(Vector<ProcessLogLine>& outLogLines, bool ownedLogLines, Bucket& bucket, const CasKey& cmdKey, u32 entryId);
		bool PopulateLogLines(Vector<ProcessLogLine>& outLogLines, const u8* mem, u64 memLen);

		UBA_NOINLINE CasKey GetCmdKey(const RootPaths& rootPaths, const ProcessStartInfo& info, bool report, u32 bucketId);
		bool DevirtualizePath(StringBufferBase& inOut, RootsHandle rootsHandle);
		bool ShouldNormalize(const StringBufferBase& path);

		bool GetLocalPathAndCasKey(Bucket& bucket, const RootPaths& rootPaths, StringBufferBase& outPath, CasKey& outKey, CompactCasKeyTable& casKeyTable, CompactPathTable& pathTable, u32 offset);
		UBA_NOINLINE void PreparseDirectory(const StringKey& fileNameKey, const StringBufferBase& filePath);

		MutableLogger m_logger;
		StorageImpl& m_storage;
		NetworkClient& m_client;
		Session& m_session;
		bool m_reportCacheKey;
		bool m_reportMissReason;
		bool m_useDirectoryPreParsing;
		bool m_validateCacheWritesInput;
		bool m_validateCacheWritesOutput;
		bool m_useRoots;
		bool m_useCacheHit;

		Atomic<bool> m_connected;

		Futex m_bucketsLock;
		UnorderedMap<u32, Bucket> m_buckets;

		Futex m_sendOneAtTheTimeLock;

		Futex m_directoryPreparserLock;
		struct PreparedDir { Futex lock; Atomic<bool> done; };
		UnorderedMap<StringKey, PreparedDir> m_directoryPreparser;

		struct PathHash { TString path; CasKey hash; };
		Vector<PathHash> m_pathHashes;

		CacheClient(const CacheClient&) = delete;
		CacheClient& operator=(const CacheClient&) = delete;
		struct DowngradedLogger;
	};
}