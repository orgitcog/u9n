// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBottleneck.h"
#include "UbaDependencyCrawler.h"
#include "UbaFile.h"
#include "UbaDirectoryTable.h"
#include "UbaFileMapping.h"
#include "UbaProcessHandle.h"
#include "UbaProcessStartInfo.h"
#include "UbaRootPaths.h"
#include "UbaSessionCreateInfo.h"
#include "UbaStats.h"
#include "UbaTrace.h"
#include "UbaThread.h"

#define UBA_DEBUG_TRACK_MAPPING 0 // UBA_DEBUG_LOGGER

namespace uba
{
	class Process;
	class ProcessHandle;
	class ProcessImpl;
	class Storage;
	class WorkManager;
	struct ProcessStartInfo;
	struct ProcessStartInfoHolder;
	struct ProcessStats;
	struct InitMessage;
	struct InitResponse;
	struct CreateFileMessage;
	struct CreateFileResponse;
	struct CloseFileMessage;
	struct CloseFileResponse;
	struct DeleteFileMessage;
	struct DeleteFileResponse;
	struct CopyFileMessage;
	struct CopyFileResponse;
	struct MoveFileMessage;
	struct MoveFileResponse;
	struct ChmodResponse;
	struct ChmodMessage;
	struct GetFullFileNameMessage;
	struct GetFullFileNameResponse;
	struct GetLongPathNameMessage;
	struct GetLongPathNameResponse;
	struct CreateDirectoryMessage;
	struct CreateDirectoryResponse;
	struct RemoveDirectoryMessage;
	struct RemoveDirectoryResponse;
	struct ListDirectoryMessage;
	struct ListDirectoryResponse;
	struct WrittenFile;
	struct NextProcessInfo;

	class Session
	{
	public:
		ProcessHandle RunProcess(const ProcessStartInfo& startInfo, bool async = true, bool enableDetour = true); // Run process. if async is false it will not return until process is done
		void CancelAllProcessesAndWait(bool terminate = true); // Cancel all processes and wait for them to go away
		void CancelAllProcesses(); // Cancel all processes

		void PrintSummary(Logger& logger); // Print summary stats of session
		bool RefreshDirectory(const tchar* dirName, bool forceRegister = false); // Tell uba a directory on disk has been changed by some other system while session is running
		bool RegisterNewFile(const tchar* filePath); // Tell uba a new file on disk has been added by some other system while session is running
		bool RegisterVirtualFile(const tchar* filePath, const tchar* sourceFile, u64 sourceOffset, u64 sourceSize);
		bool CreateVirtualFile(const tchar* filePath, const void* memory, u64 memorySize);
		bool DeleteVirtualFile(const tchar* filePath);
		void RegisterDeleteFile(const tchar* filePath); // Tell uba a file on disk has been deleted by some other system while session is running
		bool RegisterNewDirectory(const tchar* directoryPath); // Tell uba a directory on disk has been added by some other system while session is running

		RootsHandle RegisterRoots(const void* rootsData, uba::u64 rootsDataSize);

		using CustomServiceFunction = Function<u32(Process& handle, const void* recv, u32 recvSize, void* send, u32 sendCapacity)>;
		void RegisterCustomService(CustomServiceFunction&& function); // Register a custom service (that can be communicated with from the remote agents)

		using GetNextProcessFunction = Function<bool(Process& handle, NextProcessInfo& outNextProcess, u32 prevExitCode)>;
		void RegisterGetNextProcess(GetNextProcessFunction&& function); // Register a custom service (that can be communicated with from the remote agents)

		bool GetOutputFileSize(u64& outSize, const tchar* filePath); // Available if ShouldWriteToDisk is false
		bool GetOutputFileData(void* outData, const tchar* filePath, bool deleteInternalMapping); // Available if ShouldWriteToDisk is false
		bool WriteOutputFile(const tchar* filePath, bool deleteInternalMapping); // Available if ShouldWriteToDisk is false


		const tchar* GetId(); // Id for session. Will be "yymmdd_hhmmss" unless SessionCreateInfo.useUniqueId is set to false
		u32 GetActiveProcessCount(); // Current active processes running inside session
		Storage& GetStorage(); // Storage (only used when remote machines are connected)
		MutableLogger& GetLogger(); // Logger used for logging 
		LogWriter& GetLogWriter(); // LogWriter used by logger
		Trace& GetTrace(); // Trace written to be session
		const ApplicationRules* GetRules(const ProcessStartInfo& si); // Get application rules used for process
		const tchar* GetTempPath(); // Path for temp files used for current session
		const tchar* GetRootDir(); // Root dir for session files
		const tchar* GetSessionDir(); // Session dir
		bool ShouldStoreIntermediateFilesCompressed() const { return m_storeIntermediateFilesCompressed; }
		bool HasDetailedtrace() const { return m_detailedTrace; }

		// Will write current state of trace directly to disk
		bool SaveSnapshotOfTrace();

		virtual ~Session();

		u32 CreateProcessId();

		bool VirtualizePath(StringBufferBase& inOut, RootsHandle rootsHandle);
		bool DevirtualizePath(StringBufferBase& inOut, RootsHandle rootsHandle, bool reportError = true);
		bool DevirtualizeString(TString& inOut, RootsHandle rootsHandle, bool allowPathsWithoutRoot, const tchar* hint = TC(""));
		bool PopulateLocalToIndexRoots(RootPaths& out, RootsHandle rootsHandle);

		// For testing
		const u8* GetDirectoryTableMemory() { return m_directoryTableMem; }
		u32 GetDirectoryTableSize();

	protected:
		Session(const SessionCreateInfo& info, const tchar* logPrefix, bool runningRemote, WorkManager& workManager);
		bool Create(const SessionCreateInfo& info);

		void ValidateStartInfo(const ProcessStartInfo& startInfo);
		ProcessHandle InternalRunProcess(const ProcessStartInfo& startInfo, bool async, ProcessImpl* parent, bool enableDetour);
		void ProcessAdded(Process& process, u32 sessionId);
		void ProcessExited(ProcessImpl& process, u64 executionTime);
		void FlushDeadProcesses();
		void PrintProcessStats(ProcessStats& stats, const tchar* logName);
		void StartTrace(const tchar* traceName, u32 traceReserveSizeMb);
		bool StopTrace(const tchar* writeFile);
		void StartTraceThread();
		void StopTraceThread();
		u32 GetFileMappingSize();
		u32 GetMemoryMapAlignment(const StringView& fileName) const;
		u32 GetMemoryMapAlignment(const StringView& fileName, bool runningRemote) const;

		SessionStats& Stats();

		struct BinaryModule { TString name; TString path; u32 fileAttributes = 0; bool isSystem = false; u32 minOsVersion = 0; };
		bool GetBinaryModules(Vector<BinaryModule>& out, const tchar* application);
		void Free(Vector<BinaryModule>& v);
		bool IsRarelyRead(ProcessImpl& process, const StringView& fileName) const;
		bool IsRarelyReadAfterWritten(ProcessImpl& process, const StringView& fileName) const;
		bool IsKnownSystemFile(const tchar* applicationName);
		bool ShouldWriteToDisk(const StringView& fileName);
		u32 WriteDirectoryEntries(const StringKey& dirKey, const StringView& dirPath, u32* outTableOffset = nullptr);
		u32 AddFileMapping(StringKey fileNameKey, const tchar* fileName, const tchar* newFileName, u64 fileSize = InvalidValue);
		
		struct MemoryMap { StringBuffer<128> name; u64 size = 0; };
		struct FileMappingEntry;
		bool GetOrCreateMemoryMapFromFile(MemoryMap& out, StringKey fileNameKey, const tchar* fileName, bool isCompressedCas, u64 alignment, const tchar* hint, ProcessImpl* requestingProcess, bool canBeFreed);
		bool GetOrCreateMemoryMapFromStorage(MemoryMap& out, StringKey fileNameKey, const tchar* fileName, const CasKey& casKey, u64 alignment);

		bool RegisterCreateFileForWrite(StringKey fileNameKey, const StringView& fileName, bool registerRealFile, u64 fileSize = 0, u64 lastWriteTime = 0, bool invalidateStorage = true);
		u32 RegisterDeleteFile(StringKey fileNameKey, const StringView& fileName);

		virtual bool PrepareProcess(ProcessImpl& process, bool isChild, StringBufferBase& outRealApplication, const tchar*& outRealWorkingDir);
		virtual void* GetProcessEnvironmentVariables();
		virtual void PrintSessionStats(Logger& logger);

		virtual bool RegisterVirtualFileInternal(const StringKey& fileNameKey, const StringView& filePath, const tchar* sourceFile, u64 sourceOffset, u64 sourceSize);
		virtual bool CreateVirtualFileInternal(const StringKey& fileNameKey, const StringView& filePath, const void* memory, u64 memorySize);
		virtual bool RegisterVirtualFileInternal(const StringKey& fileNameKey, const StringView& filePath, FileMappingHandle mappingHandle, u64 mappingSize, u64 mappingOffset);
		virtual bool GetOutputFileSizeInternal(u64& outSize, const StringKey& fileNameKey, StringView filePath);
		virtual bool GetOutputFileDataInternal(void* outData, const StringKey& fileNameKey, StringView filePath, bool deleteInternalMapping);
		virtual bool WriteOutputFileInternal(const StringKey& fileNameKey, StringView filePath, bool deleteInternalMapping);

		virtual bool ProcessThreadStart(ProcessImpl& process);
		virtual bool GetInitResponse(InitResponse& out, const InitMessage& msg);
		virtual bool CreateFile(CreateFileResponse& out, const CreateFileMessage& msg);
		virtual bool CreateFileForRead(CreateFileResponse& out, TrackWorkScope& tws, const StringView& fileName, const StringKey& fileNameKey, ProcessImpl& process, const ApplicationRules& rules);
		virtual bool CloseFile(CloseFileResponse& out, const CloseFileMessage& msg);
		virtual bool DeleteFile(DeleteFileResponse& out, const DeleteFileMessage& msg);
		virtual bool CopyFile(CopyFileResponse& out, const CopyFileMessage& msg);
		virtual bool MoveFile(MoveFileResponse& out, const MoveFileMessage& msg);
		virtual bool Chmod(ChmodResponse& out, const ChmodMessage& msg);
		virtual bool CreateDirectory(CreateDirectoryResponse& out, const CreateDirectoryMessage& msg);
		virtual bool RemoveDirectory(RemoveDirectoryResponse& out, const RemoveDirectoryMessage& msg);
		virtual bool GetFullFileName(GetFullFileNameResponse& out, const GetFullFileNameMessage& msg);
		virtual bool GetLongPathName(GetLongPathNameResponse& out, const GetLongPathNameMessage& msg);
		virtual bool GetListDirectoryInfo(ListDirectoryResponse& out, const StringView& dirName, const StringKey& dirKey);
		virtual bool WriteFilesToDisk(ProcessImpl& process, WrittenFile** files, u32 fileCount);
		virtual bool AllocFailed(Process& process, const tchar* allocType, u32 error);
		virtual bool GetNextProcess(Process& process, bool& outNewProcess, NextProcessInfo& outNextProcess, u32 prevExitCode, BinaryReader& statsReader);
		virtual bool CustomMessage(Process& process, BinaryReader& reader, BinaryWriter& writer);
		virtual bool SHGetKnownFolderPath(Process& process, BinaryReader& reader, BinaryWriter& writer);
		virtual bool HostRun(BinaryReader& reader, BinaryWriter& writer);
		virtual bool GetSymbols(const tchar* application, bool isArm, BinaryReader& reader, BinaryWriter& writer);
		virtual bool CheckRemapping(ProcessImpl& process, BinaryReader& reader, BinaryWriter& writer);
		virtual bool RunSpecialProgram(ProcessImpl& process, BinaryReader& reader, BinaryWriter& writer);
		virtual void FileEntryAdded(StringKey fileNameKey, u64 lastWritten, u64 size);
		virtual bool FlushWrittenFiles(ProcessImpl& process);
		virtual bool UpdateEnvironment(ProcessImpl& process, const StringView& reason, bool resetStats);
		virtual bool LogLine(ProcessImpl& process, const tchar* line, LogEntryType logType);

		static constexpr CasKey CasKeyIsDirectory = { ~u64(0), ~u64(0), ~u32(0) };

		void RemoveWrittenFile(ProcessImpl& process, const StringKey& fileKey);
		bool WriteFileToDisk(ProcessImpl& process, WrittenFile& file);
		bool WriteMemoryToDisk(FileAccessor& destinationFile, const void* fileMem, u64 fileSize);
		bool GetFileMemory(const Function<bool(const void*, u64)>& func, const StringKey& fileNameKey, StringView filePath, bool deleteInternalMapping);
		void AddEnvironmentVariableNoLock(const tchar* key, const tchar* value);
		bool WriteDirectoryEntriesInternal(DirectoryTable::Directory& dir, const StringKey& dirKey, StringView dirPath, bool isRefresh, u32& outTableOffset);
		void WriteDirectoryEntriesRecursive(const StringKey& dirKey, StringView dirPath, u32& outTableOffset);
		bool CopyImports(Vector<BinaryModule>& out, const tchar* library, tchar* applicationDir, tchar* applicationDirEnd, UnorderedSet<TString>& handledImports, const char* const* loaderPaths);
		bool CreateProcessJobObject();
		void EnsureDirectoryTableMemory(u64 neededSize);
		void GetSystemInfo(StringBufferBase& out);
		bool GetMemoryInfo(u64& outAvailable, u64& outTotal);
		void WriteSummary(BinaryWriter& writer, const Function<void(Logger& logger)>& summaryFunc);
		float UpdateCpuLoad();

		bool HasVfs(RootsHandle handle) const;
		RootsHandle WithVfs(RootsHandle key, bool vfs) const;
		struct RootsEntry { Vector<u8> memory; RootPaths roots; Vector<TString> locals; Vector<TString> vfs; Futex lock; bool handled = false; };
		const RootsEntry* GetRootsEntry(RootsHandle rootsHandle);
		void PopulateRootsEntry(RootsEntry& entry, const void* rootsData, uba::u64 rootsDataSize);

		bool ExtractSymbolsFromObjectFile(const CloseFileMessage& msg, const tchar* fileName, u64 fileSize);
		bool DevirtualizeDepsFile(RootsHandle rootsHandle, MemoryBlock& destData, const void* sourceData, u64 sourceSize, bool escapeSpaces, const tchar* hint);
		
		void ThreadTraceLoop();
		void TraceWrittenFile(u32 processId, const StringView& file, u64 size = 0);
		virtual void TraceSessionUpdate();

		void RunDependencyCrawler(ProcessImpl& process);

		Storage& m_storage;
		MutableLogger m_logger;
		WorkManager& m_workManager;

		StringBuffer<32> m_id;
		StringBuffer<MaxPath> m_rootDir;
		StringBuffer<MaxPath> m_sessionDir;
		StringBuffer<MaxPath> m_sessionBinDir;
		StringBuffer<MaxPath> m_sessionOutputDir;
		StringBuffer<MaxPath> m_sessionLogDir;
		StringBuffer<MaxPath> m_systemPath;
		StringBuffer<MaxPath> m_tempPath;


		bool m_runningRemote;
		bool m_allowCustomAllocator;
		bool m_allowMemoryMaps;
		bool m_allowKeepFilesInMemory;
		bool m_allowOutputFiles;
		bool m_allowSpecialApplications;
		bool m_suppressLogging;
		bool m_shouldWriteToDisk;
		bool m_detailedTrace;
		bool m_traceChildProcesses;
		bool m_traceWrittenFiles;
		bool m_logToFile;
		bool m_storeIntermediateFilesCompressed;
		bool m_readIntermediateFilesCompressed;
		bool m_allowLocalDetour;
		bool m_extractObjFilesSymbols;
		bool m_allowLinkDependencyCrawler;

		u64 m_keepOutputFileMemoryMapsThreshold;

		Atomic<u32> m_processIdCounter;

		MemoryBlock m_directoryTableMemory;

		FileMappingHandle m_directoryTableHandle;
		u8* m_directoryTableMem;
		u64 m_directoryTableMemCommitted = 0;
		DirectoryTable m_directoryTable;
		StringKey m_directoryForcedEmpty;
	
		FileMappingHandle m_fileMappingTableHandle;
		FileMappingBuffer m_fileMappingBuffer;

		ReaderWriterLock m_fileMappingTableMemLock;
		u8* m_fileMappingTableMem;
		u32 m_fileMappingTableSize = 0;
		Futex m_fileMappingTableLookupLock;
		struct FileMappingEntry
		{
			Futex lock;
			FileMappingHandle mapping;
			u64 mappingOffset = 0;
			u64 contentSize = 0; // Size of uncompressed file or mapping
			u64 lastWriteTime = 0;
			u64 storedSize = InvalidValue; // Size stored on disk. Can be compressed or uncompressed
			u16 refCount = 0;
			u8 usedCount = 0;
			u8 usedCountBeforeFree = 0;

			u8 isDir:1 = false;
			u8 handled:1 = false;
			u8 success:1 = false;
			u8 canBeFreed:1 = false;
			u8 createIndependentMapping:1 = false;
			u8 isInvisible:1 = false;

			#if UBA_DEBUG_TRACK_MAPPING
			TString name;
			#endif
		};
		UnorderedMap<StringKey, FileMappingEntry> m_fileMappingTableLookup;

		static constexpr u64 NameToHashMemSize = 48*1024*1024;
		MemoryBlock m_nameToHashTableMem;

		Atomic<u64> m_fileIndexCounter = 8000000000;

		FileMappingAllocator m_processCommunicationAllocator;
		std::string m_detoursLibrary[2]; // x64 and arm64

		Futex m_processStatsLock;
		ProcessStats m_processStats;

		Futex m_processesLock;
		UnorderedMap<u32, ProcessHandle> m_processes;
		Vector<ProcessHandle> m_deadProcesses;
		UnorderedMap<TString, Timer> m_applicationStats;

		Futex m_outputFilesLock;
		UnorderedMap<TString, TString> m_outputFiles;

		Futex m_activeFilesLock;
		struct ActiveFile { TString name; StringKey nameKey; };
		UnorderedMap<u32, ActiveFile> m_activeFiles;

		Futex m_virtualSourceFilesLock;
		struct VirtualSourceFile { FileMappingHandle mappingHandle; u64 size; };
		UnorderedMap<StringKey, VirtualSourceFile> m_virtualSourceFiles;

		Futex m_rootsLookupLock;
		UnorderedMap<RootsHandle, RootsEntry> m_rootsLookup;

		u32 m_wantsOnCloseIdCounter = 1;

		SessionStats m_stats;
		Trace m_trace;
		Event m_traceThreadEvent;
		Thread m_traceThread;
		StringBuffer<256> m_traceOutputFile;
		TString m_extraInfo;
		u64 m_maxPageSize = ~u64(0);
		u64 m_previousTotalCpuTime = 0;
		u64 m_previousIdleCpuTime = 0;
		float m_cpuLoad = 0;

		#if PLATFORM_WINDOWS
		Futex m_processJobObjectLock;
		HANDLE m_processJobObject = NULL;
		#endif

		Vector<u8> m_environmentMemory;

		Futex m_environmentVariablesLock;
		Vector<tchar> m_environmentVariables;
		UnorderedSet<const tchar*, HashStringNoCase, EqualStringNoCase> m_localEnvironmentVariables;

		GetNextProcessFunction m_getNextProcessFunction;
		CustomServiceFunction m_customServiceFunction;

		Bottleneck m_writeFilesBottleneck;
		u64 m_writeFilesFileMapMax;
		u64 m_writeFilesNoBufferingMin;

		#if UBA_DEBUG_LOGGER
		Logger* m_debugLogger = nullptr;
		#endif

		#if PLATFORM_WINDOWS
		Futex m_isX64ApplicationLock;
		UnorderedMap<TString, bool> m_isX64Application;
		#endif

		VolumeCache m_volumeCache;

		DependencyCrawler m_dependencyCrawler;

		friend class ProcessImpl;
	};

	void GenerateNameForProcess(StringBufferBase& out, const tchar* arguments, u32 counterSuffix);
	bool GetZone(StringBufferBase& outZone);


	using FileAccess = u8;

	enum : u8
	{
		FileAccess_Read = 1,
		FileAccess_Write = 2,
		FileAccess_ReadWrite = 3
	};

	struct InitMessage
	{
		ProcessImpl& process;
	};

	struct InitResponse
	{
		u64 directoryTableHandle = 0;
		u32 directoryTableSize = 0;
		u32 directoryTableCount = 0;
		u64 mappedFileTableHandle = 0;
		u32 mappedFileTableSize = 0;
		u32 mappedFileTableCount = 0;
	};

	struct CreateFileMessage
	{
		ProcessImpl& process;
		StringBuffer<> fileName;
		StringKey fileNameKey;
		FileAccess access;
	};

	struct CreateFileResponse
	{
		StringBuffer<> fileName;
		StringBuffer<> virtualFileName;
		u64 size = InvalidValue;
		u32 closeId = 0;
		u32 mappedFileTableSize = 0;
		u32 directoryTableSize = 0;
	};

	struct CloseFileMessage
	{
		ProcessImpl& process;
		StringBuffer<> fileName;
		StringKey newNameKey;
		StringBuffer<> newName;
		u32 closeId = 0;
		u32 attributes = 0;
		bool deleteOnClose = false;
		bool success = true;
		FileMappingHandle mappingHandle;
		u64 mappingWritten = 0;
	};

	struct CloseFileResponse
	{
		u32 directoryTableSize = 0;
	};

	struct DeleteFileMessage
	{
		ProcessImpl& process;
		StringBuffer<> fileName;
		StringKey fileNameKey;
		u32 closeId = 0;
	};

	struct DeleteFileResponse
	{
		bool result = false;
		u32 errorCode = ~0u;
		u32 directoryTableSize = 0;
	};

	struct CopyFileMessage
	{
		ProcessImpl& process;
		StringKey fromKey;
		StringBuffer<> fromName;
		StringKey toKey;
		StringBuffer<> toName;
	};

	struct CopyFileResponse
	{
		StringBuffer<> fromName;
		StringBuffer<> toName;
		u32 closeId = 0;
		u32 errorCode = ~0u;
		u32 directoryTableSize = 0;
	};

	struct MoveFileMessage
	{
		ProcessImpl& process;
		StringKey fromKey;
		StringBuffer<> fromName;

		StringKey toKey;
		StringBuffer<> toName;

		u32 flags = 0;
	};

	struct MoveFileResponse
	{
		bool result = false;
		u32 errorCode = ~0u;
		u32 directoryTableSize = 0;
	};

	struct ChmodMessage
	{
		ProcessImpl& process;
		StringKey fileNameKey;
		StringBuffer<> fileName;
		u32 fileMode = 0;
	};

	struct ChmodResponse
	{
		u32 errorCode = ~0u;
	};

	struct GetFullFileNameMessage
	{
		ProcessImpl& process;
		StringBuffer<> fileName;
		StringKey fileNameKey;
		const u8* loaderPaths = nullptr;
		u32 loaderPathsSize = 0;
	};

	struct GetFullFileNameResponse
	{
		StringBuffer<> fileName;
		StringBuffer<> virtualFileName;
		u32 mappedFileTableSize = 0;
	};

	struct GetLongPathNameMessage
	{
		ProcessImpl& process;
		StringBuffer<> fileName;
	};

	struct GetLongPathNameResponse
	{
		StringBuffer<> fileName;
		u32 errorCode = ~0u;
	};

	struct CreateDirectoryMessage
	{
		StringKey nameKey;
		StringBuffer<> name;
	};

	struct CreateDirectoryResponse
	{
		bool result = false;
		u32 errorCode = 0;
		u32 directoryTableSize = 0;
	};

	struct RemoveDirectoryMessage
	{
		StringKey nameKey;
		StringBuffer<> name;
	};

	struct RemoveDirectoryResponse
	{
		bool result = false;
		u32 errorCode = 0;
		u32 directoryTableSize = 0;
	};

	struct ListDirectoryMessage
	{
		StringBuffer<> directoryName;
		StringKey directoryNameKey;
	};

	struct ListDirectoryResponse
	{
		u32 tableOffset = 0;
		u32 tableSize = 0;
	};

	struct WrittenFile
	{
		ProcessImpl* owner = nullptr;
		StringKey key;
		TString name;
		TString backedName;
		FileMappingHandle mappingHandle;
		u64 mappingWritten = 0;
		FileMappingHandle originalMappingHandle;
		u64 lastWriteTime = 0;
		u32 attributes = 0;
	};

	struct NextProcessInfo
	{
		TString arguments;
		TString workingDir;
		TString description;
		TString logFile;
		TString breadcrumbs;
	};
}

template<> struct std::hash<uba::ProcessHandle> { size_t operator()(const uba::ProcessHandle& g) const { return g.GetHash(); } };
