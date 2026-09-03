// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBottleneck.h"
#include "UbaFile.h"
#include "UbaFileMapping.h"
#include "UbaLogger.h"
#include "UbaMemory.h"
#include "UbaPathUtils.h"
#include "UbaStats.h"

#define UBA_TRACK_IS_EXECUTABLE !PLATFORM_WINDOWS

namespace uba
{
	class Config;
	class FileAccessor;
	class Trace;
	class WorkManager;
	struct DirectoryEntry;
	extern CasKey EmptyFileKey;
	
	class Storage
	{
	public:
		virtual ~Storage() {}
		virtual bool StoreCompressed() const = 0;
		virtual void PrintSummary(Logger& logger) = 0;
		virtual bool Reset() = 0;
		virtual bool SaveCasTable(bool deleteIsRunningfile, bool deleteDropped = true) = 0;
		virtual u64 GetStorageCapacity() = 0;
		virtual u64 GetStorageUsed() = 0;
		virtual bool GetZone(StringBufferBase& out) = 0;
		virtual bool HasProxy(u32 clientId) { return false; }

		virtual bool DecompressFileToMemory(const tchar* fileName, FileHandle fileHandle, u8* dest, u64 decompressedSize, const tchar* writeHint, u64 fileStartOffset) = 0;
		virtual bool DecompressMemoryToMemory(const u8* compressedData, u64 compressedSize, u8* writeData, u64 decompressedSize, const tchar* readHint, const tchar* writeHint) = 0;
		virtual bool CreateDirectory(const tchar* dir, bool* outAlreadyExists = nullptr) = 0;
		virtual bool DeleteCasForFile(const tchar* file) = 0;
		virtual bool GetFileSize(u64& outSize, const CasKey& casKey, const tchar* hint) = 0;
		virtual bool GetFileData(void* outData, const CasKey& casKey, const tchar* hint) = 0;

		struct RetrieveResult { CasKey casKey; u64 size = 0; MappedView view; };
		virtual bool RetrieveCasFile(RetrieveResult& out, const CasKey& casKey, const tchar* hint, FileMappingBuffer* mappingBuffer = nullptr, u64 memoryMapAlignment = 1, bool allowProxy = true, u32 clientId = 0) = 0;

		struct CachedFileInfo { CasKey casKey; };
		virtual bool VerifyAndGetCachedFileInfo(CachedFileInfo& out, StringKey fileNameKey, u64 verifiedLastWriteTime, u64 verifiedSize) = 0;
		virtual bool InvalidateCachedFileInfo(StringKey fileNameKey) = 0;

		virtual bool StoreCasFile(CasKey& out, const tchar* fileName, const CasKey& casKeyOverride, bool deferCreation) = 0;
		virtual bool StoreCasFileClient(CasKey& out, StringKey fileNameKey, const tchar* fileName, FileMappingHandle mappingHandle, u64 mappingOffset, u64 fileSize, const tchar* hint, bool keepMappingInMemory = false, bool storeCompressed = true) = 0;
		virtual bool DropCasFile(const CasKey& casKey, bool forceDelete, const tchar* hint) = 0;
		virtual bool ReportBadCasFile(const CasKey& casKey) = 0;
		virtual bool CalculateCasKey(CasKey& out, const tchar* fileName) = 0;

		using FormattingFunc = Function<bool(MemoryBlock& destData, const void* sourceData, u64 sourceSize, const tchar* hint)>;
		virtual bool CopyOrLink(const CasKey& casKey, const tchar* destination, u32 fileAttributes, bool writeCompressed = false, const FormattingFunc& formattingFunc = {}, bool isTemp = false, bool allowHardLink = true) = 0;
		virtual bool FakeCopy(const CasKey& casKey, const tchar* destination, u64 size, u64 lastWritten, u32 fileAttributes, bool deleteExisting = true) = 0;
		virtual bool GetCasFileName(StringBufferBase& out, const CasKey& casKey) = 0;

		virtual MappedView MapView(const CasKey& casKey, const tchar* hint) = 0;
		virtual void UnmapView(const MappedView& view, const tchar* hint) = 0;

		virtual void ReportFileWrite(StringKey fileNameKey, const tchar* fileName) = 0;
		
		virtual StorageStats& Stats() = 0;
		virtual void AddStats(StorageStats& stats) = 0;

		static void GetMappingString(StringBufferBase& out, FileMappingHandle mappingHandle, u64 offset);

		virtual void SetTrace(Trace* trace, bool detailed) {}
		virtual void Ping() {}

		struct WriteResult
		{
			FileMappingHandle mappingHandle;
			u64 mappingOffset = InvalidValue;
			u64 storedSize = InvalidValue; // Stored size of the destination
			u64 sourceStoredSize = InvalidValue; // Stored size of the source used to write compressed file
			bool isIndependent = false;
		};
		virtual bool WriteCompressedFile(WriteResult& out, const tchar* from, FileHandle readHandle, u8* readMem, u64 fileSize, const tchar* toFile, const void* header, u64 headerSize, u64 lastWriteTime = 0) = 0;

		struct ExternalFileMapping
		{
			FileMappingHandle handle;
			u64 offset = 0;
			u64 size = 0;
			u64 lastWriteTime = 0;
			u64 storedSize = InvalidValue;
			bool createIndependentMapping = false;
		};
		using ExternalFileMappingsProvider = Function<bool(ExternalFileMapping& out, StringKey fileNameKey, const tchar* fileName)>;
		virtual void RegisterExternalFileMappingsProvider(ExternalFileMappingsProvider&& provider) {}
	};

	struct StorageCreateInfo
	{
		StorageCreateInfo(const tchar* rootDir_, LogWriter& w, WorkManager& wm);

		void Apply(const Config& config);

		LogWriter& writer;
		WorkManager& workManager;
		const tchar* rootDir;
		u64 casCapacityBytes = 20llu * 1024 * 1024 * 1024;
		u32 fileEntryTableWipeThreshold = 1'000'000;
		u32 maxParallelCopyOrLink = 1000;
		bool storeCompressed = true;
		bool manuallyHandleOverflow = false;
		bool asyncUnmapViewOfFile = true;
		bool writeToDisk = true;
		bool keepTransientDataMapped = true;	// Will keep the transient data mapped in instead of doing map/unmap everytime we access it
		bool allowDeleteVerified = false;
		MutexHandle exclusiveMutex = InvalidMutexHandle; // Set this to hand over pre-exclusive access to storage. Storage will release mutex on shutdown
		u8 casCompressor = 0;
		u8 casCompressionLevel = 0;
	};

	struct BufferSlots
	{
		u8* Pop();
		void Push(u8* slot);

		~BufferSlots();

		Futex m_slotsLock;
		Vector<u8*> m_slots;
	};

	static constexpr u64 BufferSlotSize = 16*1024*1024;
	static constexpr u64 BufferSlotHalfSize = BufferSlotSize/2; // This must be three times a msg size or more.

	class StorageImpl : public Storage
	{
	public:
		StorageImpl(const StorageCreateInfo& info, const tchar* logPrefix = TC("UbaStorage"));
		virtual ~StorageImpl();

		bool LoadCasTable(bool logStats = true, bool alwaysCheckAllFiles = false, bool* outWasTerminated = nullptr);
		bool CheckCasContent();
		bool CheckFileTable(const tchar* searchPath);
		const tchar* GetTempPath();

		virtual bool SaveCasTable(bool deleteIsRunningfile, bool deleteDropped = true) override;
		virtual u64 GetStorageCapacity() override;
		virtual u64 GetStorageUsed() override;
		virtual bool GetZone(StringBufferBase& out) override;
		virtual bool Reset() override;
		bool DeleteAllCas();

		virtual bool StoreCompressed() const final { return m_storeCompressed; }
		virtual void PrintSummary(Logger& logger) override;

		virtual bool DecompressFileToMemory(const tchar* fileName, FileHandle fileHandle, u8* dest, u64 decompressedSize, const tchar* writeHint, u64 fileStartOffset) override;
		virtual bool CreateDirectory(const tchar* dir, bool* outAlreadyExists = nullptr) override;
		virtual bool DeleteCasForFile(const tchar* file) override;
		virtual bool GetFileSize(u64& outSize, const CasKey& casKey, const tchar* hint) override;
		virtual bool GetFileData(void* outData, const CasKey& casKey, const tchar* hint) override;
		virtual bool RetrieveCasFile(RetrieveResult& out, const CasKey& casKey, const tchar* hint, FileMappingBuffer* mappingBuffer = nullptr, u64 memoryMapAlignment = 1, bool allowProxy = true, u32 clientId = 0) override;
		virtual bool VerifyAndGetCachedFileInfo(CachedFileInfo& out, StringKey fileNameKey, u64 verifiedLastWriteTime, u64 verifiedSize) override;
		virtual bool InvalidateCachedFileInfo(StringKey fileNameKey) override;
		virtual bool StoreCasFile(CasKey& out, const tchar* fileName, const CasKey& casKeyOverride, bool deferCreation) override;
		virtual bool StoreCasFileClient(CasKey& out, StringKey fileNameKey, const tchar* fileName, FileMappingHandle mappingHandle, u64 mappingOffset, u64 fileSize, const tchar* hint, bool keepMappingInMemory = false, bool storeCompressed = true) override;
		virtual bool DropCasFile(const CasKey& casKey, bool forceDelete, const tchar* hint) override;
		virtual bool ReportBadCasFile(const CasKey& casKey) override;
		virtual bool CalculateCasKey(CasKey& out, const tchar* fileName) override;
		virtual bool CopyOrLink(const CasKey& casKey, const tchar* destination, u32 fileAttributes, bool writeCompressed = false, const FormattingFunc& formattingFunc = {}, bool isTemp = false, bool allowHardLink = true) override;
		virtual bool FakeCopy(const CasKey& casKey, const tchar* destination, u64 size, u64 lastWritten, u32 fileAttributes, bool deleteExisting = true) override;
		virtual bool GetCasFileName(StringBufferBase& out, const CasKey& casKey) override;
		virtual MappedView MapView(const CasKey& casKey, const tchar* hint) override;
		virtual void UnmapView(const MappedView& view, const tchar* hint) override;

		virtual void ReportFileWrite(StringKey fileNameKey, const tchar* fileName) override;
		
		virtual StorageStats& Stats() final;
		virtual void AddStats(StorageStats& stats) override;

		struct CasEntry;

		virtual bool HasCasFile(const CasKey& casKey, CasEntry** out = nullptr);
		bool EnsureCasFile(const CasKey& casKey, const tchar* fileName);
		CasKey CalculateCasKey(const tchar* fileName, FileHandle fileHandle, u64 fileSize, bool storeCompressed);
		CasKey CalculateCasKey(u8* fileMem, u64 fileSize, bool storeCompressed);
		bool StoreCasKey(CasKey& out, const tchar* fileName, const CasKey& casKeyOverride);
		bool StoreCasKey(CasKey& out, const StringKey& fileNameKey, const tchar* fileName, const CasKey& casKeyOverride);
		bool IsFileVerified(const StringKey& fileNameKey);
		void ReportFileInfoWeak(const StringKey& fileNameKey, u64 verifiedLastWriteTime, u64 verifiedSize);
		bool HasBeenSeen(const CasKey& casKey);

		virtual void RegisterExternalFileMappingsProvider(ExternalFileMappingsProvider&& provider) override;
		virtual bool WriteCompressedFile(WriteResult& out, const tchar* from, FileHandle readHandle, u8* readMem, u64 fileSize, const tchar* toFile, const void* header, u64 headerSize, u64 lastWriteTime = 0) final;
		bool WriteCasFileNoCheck(WriteResult& out, const StringKey& fileNameKey, const tchar* fileName, const CasKey& casKey, const tchar* casFile, bool storeCompressed);
		bool WriteCasFile(WriteResult& out, const tchar* fileName, const CasKey& casKey);
		bool VerifyExisting(bool& outReturnValue, ScopedWriteLock& entryLock, const CasKey& casKey, CasEntry& casEntry, StringBufferBase& casFile);
		bool AddCasFile(StringKey fileNameKey, const tchar* fileName, const CasKey& casKey, bool deferCreation, u64 fileSize = InvalidValue, u64 lastWriteTime = 0);
		void CasEntryAccessed(const CasKey& casKey);
		virtual bool IsDisallowedPath(const tchar* fileName);
		virtual bool DecompressMemoryToMemory(const u8* compressedData, u64 compressedSize, u8* writeData, u64 decompressedSize, const tchar* readHint, const tchar* writeHint) override;

		void CasEntryAccessed(CasEntry& entry);
		void CasEntryWritten(CasEntry& entry, u64 size);
		void CasEntryDeleted(CasEntry& entry, u64 size);
		void AttachEntry(CasEntry& entry);
		void DetachEntry(CasEntry& entry);
		void TraverseAllCasFiles(const tchar* dir, const Function<void(const StringBufferBase& fullPath, const DirectoryEntry& e)>& func, bool allowParallel = false);
		void TraverseAllCasFiles(const Function<void(const CasKey& key, u64 size)>& func, bool allowParallel = false);
		bool CheckAllCasFiles(u64 checkContentOfFilesNewerThanTime = ~u64(0));
		void HandleOverflow(UnorderedSet<CasKey>* outDeletedFiles);

		static MutexHandle GetExclusiveAccess(Logger& logger, const StringView& rootDir, bool reportError);
		bool DeleteIsRunningFile();
		bool TouchIsRunningFile();

		struct FileEntry;
		FileEntry& GetOrCreateFileEntry(const StringKey& fileNameKey);
		CasEntry& GetOrCreateCasEntry(const CasKey& casKey);
		bool GetFileEntry(FileEntry*& out, const tchar* file);

		MutableLogger m_logger;
		WorkManager& m_workManager;
		BufferSlots m_bufferSlots;

		StringBuffer<> m_rootDir;
		StringBuffer<> m_tempPath;

		ReaderWriterLock m_fileTableLookupLock;
		struct FileEntry
		{
			Futex lock;
			CasKey casKey;
			u64 size = 0;
			u64 lastWritten = 0;
			bool verified = false;
			bool isTemp = false;

			#if UBA_TRACK_IS_EXECUTABLE
			bool isExecutable = false;
			#endif
		};
		UnorderedMap<StringKey, FileEntry> m_fileTableLookup;

		ReaderWriterLock m_casLookupLock;
		struct CasEntry
		{
			CasEntry(const CasKey& k) : key(k) {}
			ReaderWriterLock lock;
			CasKey key;
			CasEntry* prevAccessed = nullptr;
			CasEntry* nextAccessed = nullptr;
			u64 size = 0; // Stored size of cas entry. Can be both compressed or uncompressed
			u16 readCount = 0; // This is set while file is being read over network
			bool verified = false; // This flag needs to be set for below flags to be reliable. if this is false below flags are assumptions
			bool exists = false; // File exists on disk
			bool dropped = false; // This file is not seen anymore. will be deleted during shutdown
			bool beingWritten = false; // This is set while file is being written (when coming from network)..
			bool disallowed = false; // This is set if cas is created from disallowed file

			bool mappingIsIndependent = false; // if true, mappingHandle is stand-alone and must be manually deleted from the outside using DropCasFile
			FileMappingHandle mappingHandle;
			u64 mappingOffset = 0;
			u64 mappingSize = 0;
		};

		UnorderedMap<CasKey, CasEntry> m_casLookup;
		UnorderedSet<CasKey>* m_trackedDeletes = nullptr;
		Futex m_accessLock;
		CasEntry* m_newestAccessed = nullptr;
		CasEntry* m_oldestAccessed = nullptr;
		u64 m_casTotalBytes = 0;
		u64 m_casMaxBytes = 0;
		u64 m_casCapacityBytes = 0;
		u64 m_casEvictedBytes = 0;
		u32 m_casEvictedCount = 0;
		u64 m_casDroppedBytes = 0;
		u32 m_casDroppedCount = 0;
		u32 m_fileEntryTableWipeThreshold = 0;
		bool m_overflowReported = false;
		bool m_storeCompressed = false;
		bool m_manuallyHandleOverflow = false;
		bool m_asyncUnmapViewOfFile = true;
		bool m_allowDeleteVerified = false;
		bool m_writeToDisk;

		MutexHandle m_exclusiveMutex = InvalidMutexHandle;

		ExternalFileMappingsProvider m_externalFileMappingsProvider;

		Bottleneck m_maxParallelCopyOrLinkBottleneck;

		Futex m_casTableLoadSaveLock;
		bool m_casTableLoaded = false;
		u32 m_casTableVersion = 0;

		FileMappingBuffer m_casDataBuffer;

		Futex m_deferredCasCreationLookupLock;
		using NameToCas = UnorderedMap<StringKey, CasKey>;
		struct DeferedCasCreation { StringKey fileNameKey; TString fileName; List<NameToCas::iterator> names; u64 fileSize = InvalidValue; u64 lastWriteTime = 0;};
		UnorderedMap<CasKey, DeferedCasCreation> m_deferredCasCreationLookup;
		NameToCas m_deferredCasCreationLookupByName;

		DirectoryCache m_dirCache;

		u8 m_casCompressor;
		u8 m_casCompressionLevel;

		StorageStats m_stats;

		StorageImpl(const StorageImpl&) = delete;
		void operator=(const StorageImpl&) = delete;
	};
}
