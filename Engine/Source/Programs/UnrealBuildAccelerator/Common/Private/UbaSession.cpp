// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaSession.h"
#include "UbaBinaryParser.h"
#include "UbaCompressedFileHeader.h"
#include "UbaConfig.h"
#include "UbaFileAccessor.h"
#include "UbaObjectFile.h"
#include "UbaProcess.h"
#include "UbaStorage.h"
#include "UbaDirectoryIterator.h"
#include "UbaApplicationRules.h"
#include "UbaPathUtils.h"
#include "UbaProtocol.h"
#include "UbaStorageUtils.h"
#include "UbaWorkManager.h"
#include <span>

#if PLATFORM_WINDOWS
#include <powerbase.h>
#pragma comment(lib, "Powrprof.lib")
#elif PLATFORM_MAC
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
extern char **environ;
#endif

#define UBA_DEBUG_TRACK_DIR 0 // UBA_DEBUG_LOGGER

//////////////////////////////////////////////////////////////////////////////

#if PLATFORM_WINDOWS
typedef struct _PROCESSOR_POWER_INFORMATION {
    ULONG  Number;
    ULONG  MaxMhz;
    ULONG  CurrentMhz;
    ULONG  MhzLimit;
    ULONG  MaxIdleState;
    ULONG  CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;
#endif

namespace uba
{
	bool g_dummy;

	ProcessStartInfo::ProcessStartInfo() = default;
	ProcessStartInfo::~ProcessStartInfo() = default;
	ProcessStartInfo::ProcessStartInfo(const ProcessStartInfo&) = default;

	const tchar* ProcessStartInfo::GetDescription() const
	{
		if (description && *description)
			return description;
		const tchar* d = application;
		if (const tchar* lps = TStrrchr(d, PathSeparator))
			d = lps + 1;
		if (const tchar* lps2 = TStrrchr(d, NonPathSeparator))
			d = lps2 + 1;
		return d;
	}

	ProcessHandle::ProcessHandle()
	:	m_process(nullptr)
	{
	}
	ProcessHandle::~ProcessHandle()
	{
		if (m_process)
			m_process->Release();
	}

	ProcessHandle::ProcessHandle(const ProcessHandle& o)
	{
		m_process = o.m_process;
		if (m_process)
			m_process->AddRef();
	}

	ProcessHandle::ProcessHandle(ProcessHandle&& o) noexcept
	{
		m_process = o.m_process;
		o.m_process = nullptr;
	}

	ProcessHandle& ProcessHandle::operator=(const ProcessHandle& o)
	{
		if (&o == this)
			return *this;
		if (o.m_process)
			o.m_process->AddRef();
		if (m_process)
			m_process->Release();
		m_process = o.m_process;
		return *this;
	}

	ProcessHandle& ProcessHandle::operator=(ProcessHandle&& o) noexcept
	{
		if (&o == this)
			return *this;
		if (o.m_process == m_process)
		{
			if (o.m_process)
			{
				o.m_process->Release();
				o.m_process = nullptr;
			}
			return *this;
		}
		if (m_process)
			m_process->Release();
		m_process = o.m_process;
		o.m_process = nullptr;
		return *this;
	}

	bool ProcessHandle::IsValid() const
	{
		return m_process != nullptr;
	}

	const ProcessStartInfo& ProcessHandle::GetStartInfo() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetStartInfo();
	}
	u32 ProcessHandle::GetId() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetId();
	}
	u32 ProcessHandle::GetExitCode() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetExitCode();
	}
	bool ProcessHandle::HasExited() const
	{
		UBA_ASSERT(m_process);
		return m_process->HasExited();
	}
	bool ProcessHandle::WaitForExit(u32 millisecondsTimeout) const
	{
		UBA_ASSERT(m_process);
		return m_process->WaitForExit(millisecondsTimeout);
	}
	const Vector<ProcessLogLine>& ProcessHandle::GetLogLines() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetLogLines();
	}
	const Vector<u8>& ProcessHandle::GetTrackedInputs() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetTrackedInputs();
	}
	const Vector<u8>& ProcessHandle::GetTrackedOutputs() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetTrackedOutputs();
	}
	u64 ProcessHandle::GetTotalProcessorTime() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetTotalProcessorTime();
	}
	u64 ProcessHandle::GetTotalWallTime() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetTotalWallTime();
	}
	void ProcessHandle::Cancel(bool terminate) const
	{
		UBA_ASSERT(m_process);
		return m_process->Cancel(terminate);
	}
	const tchar* ProcessHandle::GetExecutingHost() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetExecutingHost();
	}
	bool ProcessHandle::IsRemote() const
	{
		UBA_ASSERT(m_process);
		return m_process->IsRemote();
	}
	ProcessExecutionType ProcessHandle::GetExecutionType() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetExecutionType();
	}
	void ProcessHandle::TraverseOutputFiles(const Function<void(StringView file)>& func) const
	{
		UBA_ASSERT(m_process);
		m_process->TraverseOutputFiles(func);
	}
	ProcessHandle::ProcessHandle(Process* process)
	{
		m_process = process;
		process->AddRef();
	}

	void SessionCreateInfo::Apply(const Config& config)
	{
		const ConfigTable* tablePtr = config.GetTable(TC("Session"));
		if (!tablePtr)
			return;
		const ConfigTable& table = *tablePtr;
		table.GetValueAsString(rootDir, TC("RootDir"));
		table.GetValueAsString(traceName, TC("TraceName"));
		table.GetValueAsString(traceOutputFile, TC("TraceOutputFile"));
		table.GetValueAsString(extraInfo, TC("ExtraInfo"));
		table.GetValueAsBool(logToFile, TC("LogToFile"));
		table.GetValueAsBool(useUniqueId, TC("UseUniqueId"));
		table.GetValueAsBool(allowCustomAllocator, TC("AllowCustomAllocator"));
		table.GetValueAsBool(launchVisualizer, TC("LaunchVisualizer"));
		table.GetValueAsBool(allowMemoryMaps, TC("AllowMemoryMaps"));
		table.GetValueAsBool(allowKeepFilesInMemory, TC("AllowKeepFilesInMemory"));
		table.GetValueAsBool(allowOutputFiles, TC("AllowOutputFiles"));
		table.GetValueAsBool(allowSpecialApplications, TC("AllowSpecialApplications"));
		table.GetValueAsBool(suppressLogging, TC("SuppressLogging"));
		table.GetValueAsBool(shouldWriteToDisk, TC("ShouldWriteToDisk"));
		table.GetValueAsBool(traceEnabled, TC("TraceEnabled"));
		table.GetValueAsBool(detailedTrace, TC("DetailedTrace"));
		table.GetValueAsBool(traceChildProcesses, TC("TraceChildProcesses"));
		table.GetValueAsBool(traceWrittenFiles, TC("TraceWrittenFiles"));
		table.GetValueAsBool(storeIntermediateFilesCompressed, TC("StoreIntermediateFilesCompressed"));
		table.GetValueAsBool(allowLocalDetour, TC("AllowLocalDetour"));
		table.GetValueAsBool(extractObjFilesSymbols, TC("ExtractObjFilesSymbols"));
		table.GetValueAsBool(useFakeVolumeSerial, TC("UseFakeVolumeSerial"));
		table.GetValueAsBool(keepTransientDataMapped, TC("KeepTransientDataMapped"));
		table.GetValueAsBool(allowLinkDependencyCrawler, TC("AllowLinkDependencyCrawler"));
		table.GetValueAsU32(traceReserveSizeMb, TC("TraceReserveSizeMb"));
		table.GetValueAsU32(writeFilesBottleneck, TC("WriteFilesBottleneck"));
		table.GetValueAsU32(writeFilesFileMapMaxMb, TC("WriteFilesFileMapMaxMB"));
		table.GetValueAsU32(writeFilesNoBufferingMinMb, TC("WriteFilesNoBufferingMinMB"));

	}

	bool Session::WriteFileToDisk(ProcessImpl& process, WrittenFile& file)
	{
		auto& rules = *process.m_startInfo.rules;

		if (StringView(file.name).StartsWith(m_tempPath) && !rules.KeepTempOutputFile(StringView(file.name)))
		{
			CloseFileMapping(m_logger, file.mappingHandle, file.backedName.c_str());
			file.mappingHandle = {};
			#if UBA_DEBUG
			m_logger.Info(TC("Skipping writing temp file %s to disk"), file.name.c_str());
			#endif
			return true;
		}

		bool shouldEvictFromMemory = IsRarelyReadAfterWritten(process, file.name) || file.mappingWritten > m_keepOutputFileMemoryMapsThreshold;

		u64 writtenSize = 0;
		bool shouldWriteToDisk = ShouldWriteToDisk(file.name);

		if (shouldWriteToDisk)
		{
			RootsHandle rootsHandle = process.GetStartInfo().rootsHandle;

			bool storeCompressed = m_storeIntermediateFilesCompressed && g_globalRules.FileCanBeCompressed(file.name);
			bool shouldDevirtualize = false;
			bool escapeSpaces = false;
			if (!storeCompressed)
				shouldDevirtualize = HasVfs(rootsHandle) && rules.ShouldDevirtualizeFile(file.name, escapeSpaces);

#if UBA_ENABLE_ON_DISK_FILE_MAPPINGS
			if (!storeCompressed && !shouldDevirtualize && file.mappingHandle.fh)
			{
				bool success = true;
				SetFilePointerEx(file.mappingHandle.fh, ToLargeInteger(file.mappingWritten), NULL, FILE_BEGIN);
				::SetEndOfFile(file.mappingHandle.fh);

				if (false)
				{
					TrackWorkScope work(m_workManager, AsView(TC("Flush")));
					work.AddHint(file.name);
					FlushFileBuffers(file.mappingHandle.fh);
				}

				if (true)
				{
					FILE_DISPOSITION_INFO info;
					info.DeleteFile = false;
					if (!::SetFileInformationByHandle(file.mappingHandle.fh, FileDispositionInfo, &info, sizeof(info)))
						m_logger.Warning(TC("Failed to remove delete-on-close"));
				}

				if (false)
				{
					StringBuffer<> name2;
					name2.Append(TCV("\\??\\")).Append(file.name);

					u8 buffer[2048];
					auto& info = *(FILE_RENAME_INFO*)buffer;;
					info.ReplaceIfExists = true;
					info.RootDirectory = 0;
					info.FileNameLength = u32(name2.count*sizeof(tchar));
					info.Flags = FILE_RENAME_FLAG_POSIX_SEMANTICS;
					memcpy(info.FileName, name2.data, info.FileNameLength+sizeof(tchar));
					TrackWorkScope work(m_workManager, AsView(TC("Rename")));
					work.AddHint(file.name);
					success = SetFileInformationByHandle(file.mappingHandle.fh, FileRenameInfo, &info, sizeof(buffer));
				}

				if (success)
				{
					{
						TrackWorkScope work(m_workManager, AsView(TC("CloseFileMapping")));
						work.AddHint(file.name);
						CloseFileMapping(m_logger, file.mappingHandle, file.name.c_str());
					}
					file.mappingHandle = {};
					file.originalMappingHandle = {};
					m_storage.InvalidateCachedFileInfo(file.key);
					return true;
				}
				m_logger.Warning(TC("SetFileInformationByHandle failed %s (%s)"), file.name.c_str(), LastErrorToText().data);
			}
#endif

			#if UBA_DEBUG_TRACK_MAPPING
			m_debugLogger->Info(TC("Writing written file with mapping 0x%llx for %s"), u64(file.mappingHandle.mh), file.name.c_str());
			#endif

			u64 fileSize = file.mappingWritten;
			u8* mem = MapViewOfFile(m_logger, file.mappingHandle, FILE_MAP_READ, 0, fileSize);
			if (!mem)
				return m_logger.Error(TC("Failed to map view of filehandle for read %s (%s)"), file.name.c_str(), LastErrorToText().data);

			//PrefetchVirtualMemory(mem, fileSize);

			auto memClose = MakeGuard([&](){ UnmapViewOfFile(m_logger, mem, fileSize, file.name.c_str()); });

			if (storeCompressed)
			{
				Storage::WriteResult res;
				CompressedFileHeader header { CalculateCasKey(mem, fileSize, true, &m_workManager, file.name.c_str()) };

				if (!m_storage.WriteCompressedFile(res, TC("MemoryMap"), InvalidFileHandle, mem, fileSize, file.name.c_str(), &header, sizeof(header), file.lastWriteTime))
					return false;

				shouldEvictFromMemory = false; // Can't evict without properly update filemappingtable.. the file on disk does now not match what was registered for write
				writtenSize = res.storedSize;
			}
			else
			{
				FileAccessor destinationFile(m_logger, file.name.c_str());

				if (shouldDevirtualize)
				{
					// Need to turn paths back into local paths

					if (!destinationFile.CreateWrite(false, DefaultAttributes(), 0, m_tempPath.data))
						return false;

					MemoryBlock block(5*1024*1024);
					if (!DevirtualizeDepsFile(rootsHandle, block, mem, fileSize, escapeSpaces, file.name.c_str()))
						return false;

					if (!destinationFile.Write(block.memory, block.writtenSize))
						return false;
					writtenSize = block.writtenSize;
				}
				else
				{
					if (!WriteMemoryToDisk(destinationFile, mem, fileSize))
						return false;
					writtenSize = fileSize;
				}

				if (u64 time = file.lastWriteTime)
					if (!SetFileLastWriteTime(destinationFile.GetHandle(), time))
						return m_logger.Error(TC("Failed to set file time on filehandle for %s"), file.name.c_str());

				if (!destinationFile.Close(file.lastWriteTime ? nullptr : &file.lastWriteTime))
					return false;
			}

			// There are directory crawlers happening in parallel so we need to really make sure to invalidate this one since a crawler can actually
			// hit this file with information from a query before it was written.. and then it will turn it back to "verified" using old info
			m_storage.InvalidateCachedFileInfo(file.key);
		}
		else
		{
			// Delete existing file to make sure it is not picked up (since it is out of date)
			uba::DeleteFileW(file.name.c_str());

			// If shouldWriteToDisk is false we can't evict from memory because we can't re-read from disk if needed
			shouldEvictFromMemory = false;
		}

		FileMappingHandle mh = file.mappingHandle;
		file.mappingHandle = {};
		file.originalMappingHandle = {};

		if (shouldEvictFromMemory)
		{
			#if UBA_DEBUG_TRACK_MAPPING
			m_debugLogger->Info(TC("Mapping eviction queued 0x%llx (%s)"), u64(mh.mh), file.name.c_str());
			#endif

			m_workManager.AddWork([mh, n = file.name, this](const WorkContext&)
				{
					#if UBA_DEBUG_TRACK_MAPPING
					m_debugLogger->Info(TC("Mapping evicted 0x%llx (%s)"), u64(mh.mh), n.c_str());
					#endif

					CloseFileMapping(m_logger, mh, n.c_str());
				}, 1, TC("CloseFileMapping"));
		}
		else
		{
			StringBuffer<> name;
			Storage::GetMappingString(name, mh, 0);
			SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
			auto insres = m_fileMappingTableLookup.try_emplace(file.key);
			FileMappingEntry& entry = insres.first->second;
			lookupLock.Leave();

			SCOPED_FUTEX(entry.lock, entryCs);
			if (!insres.second && entry.canBeFreed) // It could be that this file has been read as input and that is fine.
				m_logger.Error(TC("Trying to write the same file twice (%s)"), file.name.c_str());
			UBA_ASSERT(mh.IsValid());
			entry.handled = true;
			entry.mapping = mh;
			entry.mappingOffset = 0;
			entry.contentSize = file.mappingWritten;
			entry.lastWriteTime = file.lastWriteTime;
			entry.isDir = false;
			entry.success = true;
			entry.canBeFreed = true;
			entry.usedCount = 0;
			entry.usedCountBeforeFree = g_globalRules.GetUsedCountBeforeFree(file.name);
			entry.storedSize = writtenSize;
			entry.isInvisible = rules.IsInvisible(file.name);

			#if UBA_DEBUG_TRACK_MAPPING
			entry.name = file.name;
			m_debugLogger->Info(TC("Mapping kept 0x%llx (%s) from detoured process (UsedCountBeforeFree: %u)"), u64(mh.mh), entry.name.c_str(), u32(entry.usedCountBeforeFree));
			#endif

			if (!entry.isInvisible)
			{
				SCOPED_WRITE_LOCK(m_fileMappingTableMemLock, lock);
				BinaryWriter writer(m_fileMappingTableMem, m_fileMappingTableSize);
				writer.WriteStringKey(file.key);
				writer.WriteString(name);
				writer.Write7BitEncoded(file.mappingWritten);
				u32 newSize = (u32)writer.GetPosition();
				m_fileMappingTableSize = (u32)newSize;
			}
		}

		if (shouldWriteToDisk)
			TraceWrittenFile(process.m_id, file.name, writtenSize);

		return true;
	}

	bool Session::WriteMemoryToDisk(FileAccessor& destinationFile, const void* fileMem, u64 fileSize)
	{
		// Seems like best combo (for windows at least) is to use writes with overlap and max 16 at the same time.
		// On one machine we get twice as fast without overlap if no bottleneck. On another machine (ntfs compression on) we get twice as slow without overlap
		// Both machines behaves well with overlap AND bottleneck. Both machine are 128 logical core thread rippers.
		bool useFileMapForWrite = fileSize && fileSize <= m_writeFilesFileMapMax; // ::CreateFileMappingW does not work for zero-length files
		bool useOverlap = !useFileMapForWrite && fileSize >= m_writeFilesNoBufferingMin;//fileSize > 2*1024*1024;

		u32 attributes = DefaultAttributes();
		if (useOverlap)
			attributes |= (FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING);

		if (useFileMapForWrite)
		{
			if (!destinationFile.CreateMemoryWrite(false, attributes, fileSize, m_tempPath.data))
				return false;
			MapMemoryCopy(destinationFile.GetData(), fileMem, fileSize);
		}
		else
		{
			if (!destinationFile.CreateWrite(false, attributes, fileSize, m_tempPath.data))
				return false;
			if (!destinationFile.Write(fileMem, fileSize, 0, true))
				return false;
		}
		return true;
	}

	bool Session::GetFileMemory(const Function<bool(const void*, u64)>& func, const StringKey& fileNameKey, StringView filePath, bool deleteInternalMapping)
	{
		FileMappingHandle mapping;
		u64 size;
		{
			SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
			auto findIt = m_fileMappingTableLookup.find(fileNameKey);
			if (findIt == m_fileMappingTableLookup.end())
				return false;
			FileMappingEntry& entry = findIt->second;
			if (deleteInternalMapping && !entry.isInvisible)
				return m_logger.Error(TC("Trying to delete mapping that is not invisible (%s)"), filePath.data);
			mapping = entry.mapping;
			size = entry.contentSize;
			if (deleteInternalMapping)
			{
				if (!entry.isInvisible)
					return m_logger.Error(TC("Trying to delete mapping that is not invisible (%s)"), filePath.data);
				m_fileMappingTableLookup.erase(findIt);
			}
		}

		u8* mem = MapViewOfFile(m_logger, mapping, FILE_MAP_READ, 0, size);
		if (!mem)
			return false;
		bool res = func(mem, size);
		UnmapViewOfFile(m_logger, mem, size, filePath.data);
		if (deleteInternalMapping)
			CloseFileMapping(m_logger, mapping, TC(""));
		return res;
	}

	void Session::AddEnvironmentVariableNoLock(const tchar* key, const tchar* value)
	{
		m_environmentVariables.insert(m_environmentVariables.end(), key, key + TStrlen(key));
		m_environmentVariables.push_back('=');
		m_environmentVariables.insert(m_environmentVariables.end(), value, value + TStrlen(value));
		m_environmentVariables.push_back(0);
	}

	bool Session::WriteDirectoryEntriesInternal(DirectoryTable::Directory& dir, const StringKey& dirKey, StringView dirPath, bool isRefresh, u32& outTableOffset)
	{
		if (dir.tableOffset != InvalidTableOffset && !isRefresh)
		{
			isRefresh = true;
		}

		auto& dirTable = m_directoryTable;

		u32 volumeSerial = 0;
		u32 volumeSerialIndex = 0;
		u32 dirAttributes = 0;
		u64 fileIndex = 0;

		u64 written = 0;
		u32 itemCount = 0;

		StringKeyHasher hasher;
		if (dirPath.count)
		{
			StringBuffer<> forHash;
			forHash.Append(dirPath);
			if (CaseInsensitiveFs)
				forHash.MakeLower();
			hasher.Update(forHash.data, forHash.count);
		}

		#if UBA_DEBUG_TRACK_DIR
		m_debugLogger->BeginScope();
		auto dg = MakeGuard([&]() { m_debugLogger->EndScope(); });
		StringBuffer<> str;
		str.Append(TCV("TRACKDIR "));
		if (isRefresh)
			str.Append(TCV("(Refresh) "));
		str.Append(dirPath).Append('\n');
		m_debugLogger->Log(LogEntryType_Info, str);
		#endif


		Vector<u8> memoryBlock;
		memoryBlock.resize(4096);
		BinaryWriter memoryWriter(memoryBlock.data(), 0, memoryBlock.size());

		if (dirKey != m_directoryForcedEmpty)
		{
			StringBuffer<4> realPath;
			if constexpr (IsWindows)
			{
				if (dirPath.count == 2)
					dirPath = realPath.Append(dirPath).Append(PathSeparator);
			}
			else
			{
				if (!dirPath.count)
					dirPath = realPath.Append(PathSeparator);
			}

			bool res = TraverseDir(m_logger, dirPath,
				[&](const DirectoryEntry& e)
				{
					StringBuffer<256> fileNameForHash;
					fileNameForHash.Append(PathSeparator).Append(e.name, e.nameLen);
					if (CaseInsensitiveFs)
						fileNameForHash.MakeLower();

					StringKey fileKey = ToStringKey(hasher, fileNameForHash.data, fileNameForHash.count);
					auto res = dir.files.try_emplace(fileKey, ~0u);
					if (!res.second)
						return;
					UBA_ASSERT(e.attributes);
					memoryWriter.WriteString(e.name, e.nameLen);

					#if UBA_DEBUG_TRACK_DIR
					m_debugLogger->Info(TC("    %s (Size: %llu, Attr: %u, Key: %s, Id: %llu)"), e.name, e.size, e.attributes, KeyToString(fileKey).data, e.id);
					#endif

					u64 id = e.id;
					if (id == 0xffffffffffffffffllu) // When using projfs we might not have the file yet and in that case we need to make this up.
						id = ++m_fileIndexCounter;

					res.first->second = u32(memoryWriter.GetPosition()); // Temporary offset that will be used further down to calculate the real offset
					memoryWriter.WriteFileAttributes(e.attributes);
					memoryWriter.WriteVolumeSerial(e.volumeSerial == volumeSerial ? volumeSerialIndex : m_volumeCache.GetSerialIndex(e.volumeSerial));
					memoryWriter.WriteFileIndex(id);
					if (!IsDirectory(e.attributes))
					{
						memoryWriter.WriteFileTime(e.lastWritten);
						memoryWriter.WriteFileSize(e.size);
					}

					FileEntryAdded(res.first->first, e.lastWritten, e.size);


					++itemCount;
					if (memoryWriter.GetPosition() > memoryBlock.size() - MaxPath)
					{
						memoryBlock.resize(memoryBlock.size() * 2);
						memoryWriter.ChangeData(memoryBlock.data(), memoryBlock.size());
					}
				}, true,
				[&](const DirectoryInfo& e)
				{
					volumeSerial = e.volumeSerial;
					volumeSerialIndex = m_volumeCache.GetSerialIndex(volumeSerial);
					dirAttributes = e.attributes;
					fileIndex = e.id;
				});
			if (!res)
			{
				#if UBA_DEBUG_TRACK_DIR
				m_debugLogger->Info(TC("    FAILED (not existing?)"));
				#endif

				if (!IsWindows || dirPath.count > 3)
					return false;
			}
		}
		else
		{
			#if UBA_DEBUG_TRACK_DIR
			m_debugLogger->Info(TC("    FORCED EMPTY"));
			#endif

			#if PLATFORM_WINDOWS
			dirAttributes = FILE_ATTRIBUTE_DIRECTORY;
			#else
			UBA_ASSERTF(false, TC("Not implemented"));
			#endif
		}

		written = memoryWriter.GetPosition();

		u64 storageSize = sizeof(StringKey) + Get7BitEncodedCount(dir.tableOffset) + Get7BitEncodedCount(itemCount) + written;

		u32 tableOffset;

		SCOPED_WRITE_LOCK(dirTable.m_memoryLock, memoryLock);
		u32 writePos = dirTable.m_memorySize;
		EnsureDirectoryTableMemory(writePos + 128 + storageSize);
		BinaryWriter tableWriter(m_directoryTableMem + dirTable.m_memorySize);

		if (isRefresh)
		{
			tableWriter.Write7BitEncoded(storageSize);
			tableWriter.WriteStringKey(dirKey);
			tableOffset = writePos + u32(tableWriter.GetPosition());
			tableWriter.Write7BitEncoded(dir.tableOffset);
		}
		else
		{
			storageSize += Get7BitEncodedCount(dirAttributes) + Get7BitEncodedCount(volumeSerialIndex) + sizeof(fileIndex);
			tableWriter.Write7BitEncoded(storageSize);
			tableWriter.WriteStringKey(dirKey);
			tableOffset = writePos + u32(tableWriter.GetPosition());
			tableWriter.Write7BitEncoded(dir.tableOffset);
			tableWriter.WriteFileAttributes(dirAttributes);
			tableWriter.WriteVolumeSerial(volumeSerialIndex);
			tableWriter.WriteFileIndex(fileIndex);
		}

		tableWriter.Write7BitEncoded(itemCount);
		u32 filesOffset = writePos + u32(tableWriter.GetPosition());
		tableWriter.WriteBytes(memoryBlock.data(), written);
		dirTable.m_memorySize += u32(tableWriter.GetPosition());
		
		memoryLock.Leave();

		// Update offsets to be relative to full memory
		for (auto& kv : dir.files)
			kv.second = filesOffset + kv.second;


		outTableOffset = tableOffset;
		dir.tableOffset = tableOffset;
		return true;
	}

	void Session::WriteDirectoryEntriesRecursive(const StringKey& dirKey, StringView dirPath, u32& outTableOffset)
	{
		auto& dirTable = m_directoryTable;
		SCOPED_WRITE_LOCK(dirTable.m_lookupLock, lookupLock);
		auto res = dirTable.m_lookup.try_emplace(dirKey, dirTable.m_memoryBlock);
		DirectoryTable::Directory& dir = res.first->second;
		lookupLock.Leave();

		SCOPED_WRITE_LOCK(dir.lock, dirLock);

		if (dir.parseOffset == 1)
		{
			outTableOffset = dir.tableOffset;
			return;
		}

		if (!WriteDirectoryEntriesInternal(dir, dirKey, dirPath, false, outTableOffset))
		{
			outTableOffset = InvalidTableOffset;
			dir.parseOffset = 2;
		}
		else
		{
			dir.parseOffset = 1;
		}

		u64 dirlen = dirPath.count;

		if (!dirlen) // This is for non-windows.. '/' is actually empty to get hashes correct
			return;

		// scan backwards first
		const tchar* rit = (tchar*)dirPath.data + dirlen - 2;
		while (rit > dirPath.data)
		{
			if (*rit != PathSeparator)
			{
				--rit;
				continue;
			}
			break;
		}

		if (IsWindows && rit <= dirPath.data) // There are no path separators left, this is the drive
		{
			dirPath.count = 0;
			return;
		}

		dirPath.count = u32(rit - dirPath.data);

		StringBuffer<> parentDirForHash;
		parentDirForHash.Append(dirPath);
		if (CaseInsensitiveFs)
			parentDirForHash.MakeLower();
		StringKey parentKey = ToStringKey(parentDirForHash);

		// Traverse through ancestors and populate them, this is an optimization
		u32 parentOffset;
		WriteDirectoryEntriesRecursive(parentKey, dirPath, parentOffset);
	}

	u32 Session::WriteDirectoryEntries(const StringKey& dirKey, const StringView& dirPath, u32* outTableOffset)
	{
		auto& dirTable = m_directoryTable;
		u32 temp;
		if (!outTableOffset)
			outTableOffset = &temp;
		WriteDirectoryEntriesRecursive(dirKey, dirPath, *outTableOffset);
		SCOPED_READ_LOCK(dirTable.m_memoryLock, memoryLock);
		return dirTable.m_memorySize;
	}

	u32 Session::AddFileMapping(StringKey fileNameKey, const tchar* fileName, const tchar* newFileName, u64 fileSize)
	{
		UBA_ASSERT(fileNameKey != StringKeyZero);
		SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
		auto insres = m_fileMappingTableLookup.try_emplace(fileNameKey);
		FileMappingEntry& entry = insres.first->second;
		lookupLock.Leave();

		SCOPED_FUTEX(entry.lock, entryCs);

		if (entry.handled)
		{
			entryCs.Leave();
			SCOPED_READ_LOCK(m_fileMappingTableMemLock, lookupCs2);
			return entry.success ? m_fileMappingTableSize : 0;
		}

		entry.contentSize = fileSize;
		entry.isDir = false;
		entry.success = true;
		entry.mapping = {};
		entry.handled = true;

		#if UBA_DEBUG_TRACK_MAPPING
		entry.name = newFileName;
		#endif

		SCOPED_WRITE_LOCK(m_fileMappingTableMemLock, lock);
		BinaryWriter writer(m_fileMappingTableMem, m_fileMappingTableSize);
		writer.WriteStringKey(fileNameKey);
		writer.WriteString(newFileName);
		writer.Write7BitEncoded(fileSize);
		u32 newSize = (u32)writer.GetPosition();
		m_fileMappingTableSize = (u32)newSize;
		return newSize;
	}

	bool Session::GetOrCreateMemoryMapFromFile(MemoryMap& out, StringKey fileNameKey, const tchar* fileName, bool isCompressedCas, u64 alignment, const tchar* hint, ProcessImpl* requestingProcess, bool canBeFreed)
	{
		TimerScope ts(Stats().waitMmapFromFile);

		SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
		auto insres = m_fileMappingTableLookup.try_emplace(fileNameKey);
		FileMappingEntry& entry = insres.first->second;
		lookupLock.Leave();


		auto updateRequestingProcess = [&]()
			{
				if (requestingProcess && entry.canBeFreed && !m_runningRemote)
				{
					SCOPED_FUTEX(requestingProcess->m_usedFileMappingsLock, lock);
					if (!requestingProcess->m_hasExited && requestingProcess->m_usedFileMappings.insert(fileNameKey).second)
						++entry.refCount;
				}
			};

		SCOPED_FUTEX(entry.lock, entryLock);

		if (entry.handled)
		{
			if (!entry.success)
				return false;
			out.size = entry.contentSize;
			if (entry.mapping.IsValid())
			{
				Storage::GetMappingString(out.name, entry.mapping, entry.mappingOffset);
				updateRequestingProcess();
			}
			else
				out.name.Append(entry.isDir ? TC("$d") : TC("$f"));
			return true;
		}

		ts.Cancel();
		TimerScope ts2(Stats().createMmapFromFile);

		out.size = 0;

		entry.handled = true;

		bool isDir = false;
		u32 attributes = DefaultAttributes();
		FileHandle fileHandle = uba::CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, attributes);
		if (fileHandle == InvalidFileHandle)
		{
			u32 error = GetLastError();
			if (error == ERROR_ACCESS_DENIED || error == ERROR_PATH_NOT_FOUND) // Probably directory? .. path not found can be returned if path is the drive ('e:\' etc)
			{
				fileHandle = uba::CreateFileW(fileName, 0, 0x00000007, 0x00000003, FILE_FLAG_BACKUP_SEMANTICS);
				if (fileHandle == InvalidFileHandle)
					return m_logger.Error(TC("Failed to open file %s (%s)"), fileName, LastErrorToText().data);

				isDir = true;
			}
			else
				return m_logger.Error(TC("Failed to open file %s (%s)"), fileName, LastErrorToText().data);
		}
		auto _ = MakeGuard([&](){ uba::CloseFile(fileName, fileHandle); });

		u64 storedSize = InvalidValue;
		u64 contentSize = 0;
		u64 lastWriteTime = 0;
		u64 fileStartOffset = 0;

		bool isCompressed = isCompressedCas;
		if (!isDir)
		{
			if (isCompressedCas)
			{
				if (!ReadFile(m_logger, fileName, fileHandle, &contentSize, sizeof(u64)))
					return m_logger.Error(TC("Failed to read first bytes from file %s (%s)"), fileName, LastErrorToText().data);
			}
			else
			{
				FileBasicInformation info;
				if (!GetFileBasicInformationByHandle(info, m_logger, fileName, fileHandle))
					return false;

				storedSize = info.size;
				contentSize = info.size;
				lastWriteTime = info.lastWriteTime;

				if (m_readIntermediateFilesCompressed && info.size > sizeof(CompressedFileHeader) && g_globalRules.FileCanBeCompressed(ToView(fileName)))
				{
					CompressedFileHeader header(CasKeyZero);
					if (!ReadFile(m_logger, fileName, fileHandle, &header, sizeof(header)))
						return m_logger.Error(TC("Failed to read header of compressed file %s (%s)"), fileName, LastErrorToText().data);
					if (header.IsValid())
					{
						fileStartOffset = sizeof(CompressedFileHeader);
						isCompressed = true;
						if (!ReadFile(m_logger, fileName, fileHandle, &contentSize, sizeof(u64)))
							return m_logger.Error(TC("Failed to read first bytes from file %s (%s)"), fileName, LastErrorToText().data);
					}
					else
					{
						if (!SetFilePointer(m_logger, fileName, fileHandle, 0))
							return false;
					}
				}
			}
		}

		if (isDir || contentSize == 0)
		{
			if (isDir)
				out.name.Append(TCV("$d"));
			else
				out.name.Append(TCV("$f"));
		}
		else
		{
			if (contentSize > m_fileMappingBuffer.GetFileMappingCapacity())
				return m_logger.Error(TC("File %s has a size (%llu) that is too large to fit in mapping buffer (%s)"), fileName, contentSize, hint);

			FileMappingHandle mapping;
			u64 mappingOffset = 0;
			u8* mappingMemory = nullptr;

			MappedView mappedView;

			auto ownedGuard1 = MakeGuard([&]() { CloseFileMapping(m_logger, mapping, fileName); });
			auto ownedGuard2 = MakeGuard([&]() { m_workManager.AddWork([this, mappingMemory, contentSize, fn = TString(fileName)](const WorkContext&) { UnmapViewOfFile(m_logger, mappingMemory, contentSize, fn.c_str()); }, 1, TC("UnmapViewOfFile")); });
			auto viewGuard = MakeGuard([&]() { m_fileMappingBuffer.UnmapView(mappedView, fileName); });

			if (canBeFreed)
			{
				viewGuard.Cancel();
				mapping = CreateMemoryMappingW(m_logger, PAGE_READWRITE, contentSize, nullptr, fileName);
				if (!mapping.IsValid())
					return false;
				mappingMemory = MapViewOfFile(m_logger, mapping, FILE_MAP_WRITE, 0, contentSize);
				if (!mappingMemory)
					return false;
			}
			else
			{
				UBA_ASSERTF(alignment, TC("No alignment set when creating memory map for %s (%s)"), fileName, hint);
				UBA_ASSERT(!entry.canBeFreed);
				ownedGuard1.Cancel();
				ownedGuard2.Cancel();
				mappedView = m_fileMappingBuffer.AllocAndMapView(MappedView_Transient, contentSize, alignment, fileName, false);
				if (!mappedView.memory)
					return false;
				mapping = mappedView.handle;
				mappingOffset = mappedView.offset;
				mappingMemory = mappedView.memory;
			}

			if (isCompressed)
			{
				if (!m_storage.DecompressFileToMemory(fileName, fileHandle, mappingMemory, contentSize, TC("GetOrCreateMemoryMapFromFile"), fileStartOffset))
					return false;
			}
			else
			{
				if (!ReadFile(m_logger, fileName, fileHandle, mappingMemory, contentSize))
					return false;
			}

			ownedGuard2.Execute();
			ownedGuard1.Cancel();
			viewGuard.Execute();

			entry.canBeFreed = canBeFreed;
			entry.mappingOffset = mappingOffset;
			Storage::GetMappingString(out.name, mapping, mappingOffset);
			entry.mapping = mapping;

			if (canBeFreed)
			{
				entry.usedCount = 0;
				entry.usedCountBeforeFree = g_globalRules.GetUsedCountBeforeFree(ToView(fileName));
			}

			updateRequestingProcess();
		}

		entry.success = true;

		{
			SCOPED_WRITE_LOCK(m_fileMappingTableMemLock, lock);
			BinaryWriter writer(m_fileMappingTableMem, m_fileMappingTableSize);
			writer.WriteStringKey(fileNameKey);
			writer.WriteString(out.name);
			writer.Write7BitEncoded(contentSize);
			m_fileMappingTableSize = (u32)writer.GetPosition();
		}

		#if UBA_DEBUG_TRACK_MAPPING
		entry.name = fileName;
		m_debugLogger->Info(TC("Mapping created 0x%llx (%s) from file (%s) - %s"), u64(entry.mapping.mh), entry.name.c_str(), hint, TimeToText(GetTime() - ts2.start).str);
		#endif

		entry.isDir = isDir;
		entry.contentSize = contentSize;
		entry.storedSize = storedSize;
		entry.lastWriteTime = lastWriteTime;
		
		out.size = contentSize;
		return true;
	}

	bool Session::GetOrCreateMemoryMapFromStorage(MemoryMap& out, StringKey fileNameKey, const tchar* fileName, const CasKey& casKey, u64 alignment)
	{
		//StringBuffer<> workName;
		//u32 len = TStrlen(fileName);
		//workName.Append(TCV("MM:")).Append(len > 30 ? fileName + (len - 30) : fileName);
		//TrackWorkScope tws(m_workManager, workName.data);

		SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
		auto insres = m_fileMappingTableLookup.try_emplace(fileNameKey);
		FileMappingEntry& entry = insres.first->second;
		lookupLock.Leave();

		SCOPED_FUTEX(entry.lock, entryCs);

		if (entry.handled)
		{
			entryCs.Leave();
			if (!entry.success)
				return false;
			out.size = entry.contentSize;
			if (entry.mapping.IsValid())
				Storage::GetMappingString(out.name, entry.mapping, entry.mappingOffset);
			else
				out.name.Append(entry.isDir ? TC("$d") : TC("$f"));
			return true;
		}

		out.size = 0;

		entry.handled = true;

		MappedView mappedViewRead = m_storage.MapView(casKey, fileName);
		if (!mappedViewRead.handle.IsValid())
			return false;

		u64 contentSize = InvalidValue;

		if (mappedViewRead.isCompressed)
		{
			auto mvrg = MakeGuard([&](){ m_storage.UnmapView(mappedViewRead, fileName); });
			const u8* readMemory = mappedViewRead.memory;
			contentSize = *(u64*)readMemory;
			readMemory += 8;

			if (contentSize == 0)
			{
				out.name.Append(TCV("$f"));
			}
			else
			{
				if (contentSize > m_fileMappingBuffer.GetFileMappingCapacity())
					return m_logger.Error(TC("File %s has a size (%llu) that is too large to fit in mapping buffer (GetOrCreateMemoryMapFromView)"), fileName, contentSize);

				auto mappedViewWrite = m_fileMappingBuffer.AllocAndMapView(MappedView_Transient, contentSize, alignment, fileName);
				if (!mappedViewWrite.memory)
					return false;
				auto unmapGuard = MakeGuard([&](){ m_fileMappingBuffer.UnmapView(mappedViewWrite, fileName); });

				if (!m_storage.DecompressMemoryToMemory(readMemory, mappedViewRead.size, mappedViewWrite.memory, contentSize, fileName, TC("TransientMapping")))
					return false;
				unmapGuard.Execute();

				entry.mappingOffset = mappedViewWrite.offset;
				Storage::GetMappingString(out.name, mappedViewWrite.handle, mappedViewWrite.offset);
				entry.mapping = mappedViewWrite.handle;
			}
			mvrg.Execute();
		}
		else
		{
			UBA_ASSERT(mappedViewRead.memory == nullptr);
			entry.mappingOffset = mappedViewRead.offset;
			Storage::GetMappingString(out.name, mappedViewRead.handle, mappedViewRead.offset);
			entry.mapping = mappedViewRead.handle;
			contentSize = mappedViewRead.size;
		}
		entry.success = true;

		{
			SCOPED_WRITE_LOCK(m_fileMappingTableMemLock, lock);
			BinaryWriter writer(m_fileMappingTableMem, m_fileMappingTableSize);
			writer.WriteStringKey(fileNameKey);
			writer.WriteString(out.name);
			writer.Write7BitEncoded(contentSize);
			m_fileMappingTableSize = (u32)writer.GetPosition();
		}

		entry.isDir = false;
		entry.contentSize = contentSize;
		
		out.size = contentSize;

		#if UBA_DEBUG_TRACK_MAPPING
		entry.name = fileName;
		m_debugLogger->Info(TC("Mapping created 0x%llx (%s) from view"), u64(entry.mapping.mh), entry.name.c_str());
		#endif

		return true;
	}

	bool GetDirKey(StringKey& outDirKey, StringBufferBase& outDirName, const tchar*& outLastSlash, const StringView& fileName)
	{
		outLastSlash = TStrrchr(fileName.data, PathSeparator);
		UBA_ASSERTF(outLastSlash, TC("Can't get dir key for path %s"), fileName.data);
		if (!outLastSlash)
			return false;

		u64 dirLen = u64(outLastSlash - fileName.data);
		outDirName.Append(fileName.data, dirLen);
		outDirKey = CaseInsensitiveFs ? ToStringKeyLower(outDirName) : ToStringKey(outDirName);
		return true;
	}

	bool Session::RegisterCreateFileForWrite(StringKey fileNameKey, const StringView& fileName, bool registerRealFile, u64 fileSize, u64 lastWriteTime, bool invalidateStorage)
	{
		// Remote is not updating its own directory table
		if (m_runningRemote)
			return true;

		auto& dirTable = m_directoryTable;

		StringKey dirKey;
		const tchar* lastSlash;
		StringBuffer<> dirName;
		if (!GetDirKey(dirKey, dirName, lastSlash, fileName))
			return true;
			
		#if 0//_DEBUG  // Bring this back, turned off right now because a few lines above the call to this method we add a mapping
		{
			SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
			auto findIt = m_fileMappingTableLookup.find(fileNameKey);
			if (findIt != m_fileMappingTableLookup.end())
			{
				FileMappingEntry& entry = findIt->second;
				lookupLock.Leave();
				SCOPED_FUTEX(entry.lock, entryCs);
				UBA_ASSERT(!entry.mapping);
			}
		}
		#endif

		bool shouldWriteToDisk = registerRealFile && ShouldWriteToDisk(fileName);

		// When not writing to disk we need to populate lookup before adding non-written files.. otherwise they will be lost once lookup is actually populated
		if (!shouldWriteToDisk)
		{
			u32 res = WriteDirectoryEntries(dirKey, dirName);
			UBA_ASSERTF(res, TC("Failed to write directory entries for %s"), dirName.data); (void)res;
		}

		SCOPED_READ_LOCK(dirTable.m_lookupLock, lookupCs);
		auto findIt = dirTable.m_lookup.find(dirKey);
		if (findIt == dirTable.m_lookup.end())
			return true;

		DirectoryTable::Directory& dir = findIt->second;
		lookupCs.Leave();

		SCOPED_WRITE_LOCK(dir.lock, dirLock);

		// To prevent race where code creating dir manage to add to lookup but then got here later than this thread.
		while (dir.parseOffset == 0)
		{
			dirLock.Leave();
			Sleep(1);
			dirLock.Enter();
		}

		// Directory was attempted to be added when it didn't exist. It is still added to dirtable lookup but we set parseOffset to 2.
		// If adding a file, clearly it does exist.. so let's reparse it.
		if (dir.parseOffset == 2)
		{
			dirLock.Leave();
			u32 res = WriteDirectoryEntries(dirKey, dirName);
			UBA_ASSERT(res); (void)res;
			dirLock.Enter();
		}
		UBA_ASSERTF(dir.parseOffset == 1 || !m_shouldWriteToDisk, TC("Registering create file for write %s with unexpect dir.parseOffset %u "), fileName.data, dir.parseOffset);

		if (fileNameKey == StringKeyZero)
		{
			StringBuffer<> forKey;
			forKey.Append(fileName);
			if (CaseInsensitiveFs)
				forKey.MakeLower();
			fileNameKey = ToStringKey(forKey);
		}
		auto insres = dir.files.try_emplace(fileNameKey, ~u32(0));

		u64 fileIndex = InvalidValue;
		u32 attributes = 0;
		u32 volumeSerial = 0;
		bool isDirectory = false;

		if (shouldWriteToDisk)
		{
			FileInformation info;
			if (!GetFileInformation(info, m_logger, fileName.data))
				return m_logger.Error(TC("Failed to get file information for %s while checking file added for write. This should not happen! (%s)"), fileName.data, LastErrorToText().data);

			attributes = info.attributes;
			volumeSerial = info.volumeSerialNumber;
			lastWriteTime = info.lastWriteTime;
			isDirectory = IsDirectory(attributes);
			if (isDirectory)
				fileSize = 0;
			else
				fileSize = info.size;
			fileIndex = info.index;
		}
		else
		{
			// TODO: Do we need more code here?
			attributes = DefaultAttributes();
			volumeSerial = 1;
			fileIndex = ++m_fileIndexCounter;
		}

		// Check if new write is actually a write. The file might just have been open with write permissions and then actually never written to.
		// We check this by using lastWriteTime. If it hasn't change, directory table is already up-to-date
		if (!insres.second && insres.first->second != ~u32(0))
		{
			BinaryReader reader(m_directoryTableMem + insres.first->second);
			u32 oldAttr = reader.ReadFileAttributes();

			if (isDirectory) // Ignore updating directories.. they should always be the same regardless
			{
				UBA_ASSERT(IsDirectory(oldAttr));
				return true;
			}
			reader.ReadVolumeSerial();
			u64 oldFileIndex = reader.ReadFileIndex();(void)oldFileIndex;

			u64 oldLastWriteTime = reader.ReadFileTime();
			if (lastWriteTime == oldLastWriteTime)
			{
				#if !PLATFORM_WINDOWS
				UBA_ASSERT(oldFileIndex == fileIndex); // Checking so it is really the same file
				#endif
				u64 oldSize = reader.ReadFileSize();
				if (oldSize == fileSize && oldAttr == attributes) // Only attributes could change from a chmod
					return true;
				// TODO: Somehow this can happen and I have no idea how. last written time should be set on close file so it shouldnt be possible.
				//else
				//	m_logger.Warning(TC("Somehow file %s has same last written time at two points in time but different size (old %llu new %llu)"), fileName.data, oldSize, fileSize);
			}
		}

		// There are directory crawlers happening in parallel so we need to really make sure to invalidate this one since a crawler can actually
		// hit this file with information from a query before it was written.. and then it will turn it back to "verified" using old info
		if (registerRealFile && invalidateStorage)
			m_storage.InvalidateCachedFileInfo(fileNameKey);

		FileEntryAdded(fileNameKey, lastWriteTime, fileSize);

		u8 temp[1024];
		u64 written;
		u64 entryPos;
		{
			BinaryWriter writer(temp, 0, sizeof(temp));
			writer.WriteStringKey(dirKey);
			writer.Write7BitEncoded(dir.tableOffset); // Previous entry for same directory
			writer.Write7BitEncoded(1); // Count
			writer.WriteString(lastSlash + 1);
			entryPos = writer.GetPosition();
			writer.WriteFileAttributes(attributes);
			writer.WriteVolumeSerial(m_volumeCache.GetSerialIndex(volumeSerial));
			writer.WriteFileIndex(fileIndex);
			if (!isDirectory)
			{
				writer.WriteFileTime(lastWriteTime);
				writer.WriteFileSize(fileSize);
			}
			written = writer.GetPosition();
		}

		#if UBA_DEBUG_TRACK_DIR
		m_debugLogger->Info(TC("TRACKADD    %s (Size: %llu, Attr: %u, Key: %s, Id: %llu)"), fileName.data, fileSize, attributes, KeyToString(fileNameKey).data, fileIndex);
		#endif


		SCOPED_WRITE_LOCK(dirTable.m_memoryLock, memoryLock);
		u32 writePos = dirTable.m_memorySize;
		EnsureDirectoryTableMemory(writePos + 8 + written);
		BinaryWriter writer(m_directoryTableMem + writePos);
		writer.Write7BitEncoded(written); // Storage size
		insres.first->second = dirTable.m_memorySize + u32(writer.GetPosition() + entryPos); // Storing position to last write time
		u32 tableOffset = u32(writer.GetPosition()) + sizeof(StringKey);
		writer.WriteBytes(temp, written);
		dir.tableOffset = dirTable.m_memorySize + tableOffset;
		dirTable.m_memorySize += u32(writer.GetPosition());
		return true;
	}

	u32 Session::RegisterDeleteFile(StringKey fileNameKey, const StringView& fileName)
	{
		// Remote is not updating its own directory table
		if (m_runningRemote)
			return GetDirectoryTableSize();

		auto& dirTable = m_directoryTable;

		StringKey dirKey;
		const tchar* lastSlash;
		StringBuffer<> dirName;
		if (!GetDirKey(dirKey, dirName, lastSlash, fileName))
			return InvalidTableOffset;

		SCOPED_READ_LOCK(dirTable.m_lookupLock, lookupCs);
		auto res = dirTable.m_lookup.find(dirKey);
		if (res == dirTable.m_lookup.end())
			return 0;
		DirectoryTable::Directory& dir = res->second;
		lookupCs.Leave();
		SCOPED_WRITE_LOCK(dir.lock, dirLock);

		while (dir.parseOffset == 0)
		{
			dirLock.Leave();
			Sleep(1);
			dirLock.Enter();
		}
		UBA_ASSERTF(dir.parseOffset == 1, TC("Registering deleted file %s with unexpect dir.parseOffset %u "), fileName.data, dir.parseOffset);

		if (fileNameKey == StringKeyZero)
		{
			StringBuffer<> forKey;
			forKey.Append(fileName);
			if (CaseInsensitiveFs)
				forKey.MakeLower();
			fileNameKey = ToStringKey(forKey);
		}
		
		// Does not exist, no need adding to file table
		if (dir.files.erase(fileNameKey) == 0)
			return 0;

		u8 temp[1024];
		u64 written;
		{
			BinaryWriter writer(temp, 0, sizeof(temp));
			writer.WriteStringKey(dirKey);
			writer.Write7BitEncoded(dir.tableOffset); // Previous entry for same directory
			writer.Write7BitEncoded(1); // Count
			writer.WriteString(lastSlash + 1);
			writer.WriteFileAttributes(0);
			writer.WriteVolumeSerial(0);
			writer.WriteFileIndex(0);
			if (true) // !IsDirectory()
			{
				writer.WriteFileTime(0);
				writer.WriteFileSize(0);
			}
			written = writer.GetPosition();
		}

		#if UBA_DEBUG_TRACK_DIR
		m_debugLogger->Info(TC("TRACKDEL    %s (Key: %s)"), fileName.data, KeyToString(fileNameKey).data);
		#endif

		SCOPED_WRITE_LOCK(dirTable.m_memoryLock, memoryLock);
		u32 writePos = dirTable.m_memorySize;
		EnsureDirectoryTableMemory(writePos + 8 + written);
		BinaryWriter writer(m_directoryTableMem + writePos);
		writer.Write7BitEncoded(written); // Storage size
		u32 tableOffset = u32(writer.GetPosition()) + sizeof(StringKey);
		writer.WriteBytes(temp, written);
		dir.tableOffset = dirTable.m_memorySize + tableOffset;
		dirTable.m_memorySize += u32(writer.GetPosition());
		return dirTable.m_memorySize;
	}

	RootsHandle Session::RegisterRoots(const void* rootsData, uba::u64 rootsDataSize)
	{
		CasKey key = ToCasKey(CasKeyHasher().Update(rootsData, rootsDataSize), false);
		RootsHandle rootsHandle = WithVfs(key.a, false);

		SCOPED_FUTEX(m_rootsLookupLock, rootsLock);
		RootsEntry& entry = m_rootsLookup.try_emplace(rootsHandle).first->second;
		rootsLock.Leave();

		SCOPED_FUTEX(entry.lock, entryLock);
		UBA_ASSERT(!entry.handled || memcmp(entry.memory.data(), rootsData, rootsDataSize) == 0);

		if (!entry.handled)
		{
			PopulateRootsEntry(entry, rootsData, rootsDataSize);
			entry.handled = true;
		}

		return WithVfs(rootsHandle, !entry.locals.empty());
	}

	bool Session::CopyImports(Vector<BinaryModule>& out, const tchar* library, tchar* applicationDir, tchar* applicationDirEnd, UnorderedSet<TString>& handledImports, const char* const* loaderPaths)
	{
		if (!handledImports.insert(library).second)
			return true;
		TSprintf_s(applicationDirEnd, 512 - (applicationDirEnd - applicationDir), TC("%s"), library);
		const tchar* applicationName = applicationDir;
		u32 attr = GetFileAttributesW(applicationName); // TODO: Use attributes table
		tchar temp[512];
		tchar temp2[512];
		StringBuffer<512> temp3;
		bool result = true;

		if (attr == INVALID_FILE_ATTRIBUTES)
		{
#if PLATFORM_WINDOWS
			if (!SearchPathW(NULL, library, NULL, 512, temp, NULL))
				return true; // TODO: We have to return true here because there are scenarios where failing is actually ok (it seems it can return false on crt shim libraries such as api-ms-win-crt*)
#elif PLATFORM_MAC
			if (!loaderPaths)
				return m_logger.Error("CopyImports - Failed to find file %s (no loader paths)", applicationName);
			u32 loaderPathCount = 0;
			for (auto it = loaderPaths; *it; ++it)
			{
				++loaderPathCount;
				StringBuffer<> absolutePath;
				absolutePath.Append(applicationDir, applicationDirEnd - applicationDir).Append(*it).EnsureEndsWithSlash().Append(library);
				FixPath(absolutePath.data, nullptr, 0, temp3.Clear());
				attr = GetFileAttributesW(temp3.data);
				if (attr == INVALID_FILE_ATTRIBUTES)
					continue;
				memcpy(temp, temp3.data, temp3.count+1);
				break;
			}
			if (attr == INVALID_FILE_ATTRIBUTES)
				return m_logger.Error("CopyImports - Failed to find file %s (%u loader paths)", applicationName, loaderPathCount);
			//UBA_ASSERTF(false, TC("DIR NOT FOUND: %s"), applicationName);
			#else
			// SOOO, for linux it might be fine not finding these files.
			// DT_NEEDED does not mean it is needed(!).. and no way of knowing which ones that are needed
			#if UBA_DEBUG
			m_logger.Error("Code path not implemented for linux! (CopyImports %s)", library);
			#endif
			return true;
#endif

			applicationName = temp;
			attr = DefaultAttributes();

			tchar* lastSlash = TStrrchr(temp, PathSeparator);
			UBA_ASSERTF(lastSlash, TC("No slash found in path %s"), temp);
			u64 applicationDirLen = u64(lastSlash + 1 - temp);
			memcpy(temp2, temp, applicationDirLen * sizeof(tchar));
			applicationDir = temp2;
			applicationDirEnd = temp2 + applicationDirLen;
		}
		else
		{
			#if PLATFORM_WINDOWS
			attr = DefaultAttributes();
			#endif
		}

		FixPath(applicationName, nullptr, 0, temp3.Clear());

		bool isSystem = StartsWith(applicationName, m_systemPath.data);
		if (isSystem && IsKnownSystemFile(applicationName))
			return true;

		BinaryModule& binaryModule = out.emplace_back(library, temp3.ToString(), attr, isSystem);(void)binaryModule;

		StringBuffer<> errorStr;
		BinaryInfo info;
		ParseBinary(temp3, ToView(applicationDir), info, [&](const tchar* importName, bool isKnown, const char* const* importLoaderPaths)
			{
				if (result && !isKnown)
					result = CopyImports(out, importName, applicationDir, applicationDirEnd, handledImports, importLoaderPaths);
			}, errorStr);
		if (errorStr.count)
			return m_logger.Error(errorStr.data);

		#if PLATFORM_MAC
		binaryModule.minOsVersion = info.minVersion;
		#endif

		// This code is needed if application is compiled with tsan
		//strcpy(applicationDirEnd, "libclang_rt.tsan.so");
		//out.push_back({ "libclang_rt.tsan.so", applicationDir, S_IRUSR | S_IWUSR });
		return result;
	}

	Session::Session(const SessionCreateInfo& info, const tchar* logPrefix, bool runningRemote, WorkManager& workManager)
	:	m_storage(info.storage)
	,	m_logger(info.logWriter, logPrefix)
	,	m_workManager(workManager)
	,	m_directoryTable(m_directoryTableMemory)
	,	m_fileMappingBuffer(m_logger, &workManager)
	,	m_processCommunicationAllocator(m_logger, TC("CommunicationAllocator"))
	,	m_trace(info.logWriter)
	,	m_writeFilesBottleneck(info.writeFilesBottleneck)
	,	m_writeFilesFileMapMax(u64(info.writeFilesFileMapMaxMb)*1024*1024)
	,	m_writeFilesNoBufferingMin(u64(info.writeFilesNoBufferingMinMb)*1024*1024)
	,	m_dependencyCrawler(m_logger, workManager)
	{
		UBA_ASSERTF(info.rootDir && *info.rootDir, TC("No root dir set when creating session"));
		m_rootDir.count = GetFullPathNameW(info.rootDir, m_rootDir.capacity, m_rootDir.data, NULL);
		m_rootDir.Replace('/', PathSeparator).EnsureEndsWithSlash();

		m_sessionDir.Append(m_rootDir).Append(TCV("sessions")).Append(PathSeparator);
		if (info.useUniqueId)
		{
			u32 retryIndex = 0;
			while (true)
			{
				time_t rawtime;
				time(&rawtime);
				tm ti;
				localtime_s(&ti, &rawtime);
				m_id.Appendf(TC("%02i%02i%02i_%02i%02i%02i"), ti.tm_year - 100,ti.tm_mon+1,ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
				if (retryIndex > 0)
					m_id.Append('_').AppendValue(retryIndex);
				m_sessionDir.Append(m_id);
				bool alreadyExists;
				if (m_storage.CreateDirectory(m_sessionDir.data, &alreadyExists) && !alreadyExists)
					break;
				m_sessionDir.Resize(m_sessionDir.count - m_id.count);
				m_id.Clear();
				++retryIndex;
			}
		}
		else
		{
			m_id.Append(TCV("Debug"));
			m_sessionDir.Append(m_id);
		}
		m_sessionDir.Append(PathSeparator);

		m_runningRemote = runningRemote;
		m_allowCustomAllocator = info.allowCustomAllocator;
		m_allowMemoryMaps = info.allowMemoryMaps;
		m_allowKeepFilesInMemory = info.allowKeepFilesInMemory;
		m_allowOutputFiles = info.allowOutputFiles;
		m_allowSpecialApplications = info.allowSpecialApplications;
		m_suppressLogging = info.suppressLogging;
		if (!info.allowMemoryMaps)
			m_keepOutputFileMemoryMapsThreshold = 0;
		else
			m_keepOutputFileMemoryMapsThreshold = info.keepOutputFileMemoryMapsThreshold;
		m_shouldWriteToDisk = info.shouldWriteToDisk;
		UBA_ASSERTF(m_shouldWriteToDisk || m_allowMemoryMaps, TC("Can't disable both should write to disk and allow memory maps"));

		m_storeIntermediateFilesCompressed = info.storeIntermediateFilesCompressed && IsWindows; // Non-windows not implemented (yet)
		m_readIntermediateFilesCompressed = (m_storeIntermediateFilesCompressed || (info.readIntermediateFilesCompressed && IsWindows)) && !runningRemote; // with remote we decompress the files into memory
		m_allowLocalDetour = info.allowLocalDetour;
		m_extractObjFilesSymbols = info.extractObjFilesSymbols;
		m_allowLinkDependencyCrawler = info.allowLinkDependencyCrawler;

		m_detailedTrace = info.detailedTrace;
		m_traceChildProcesses = info.traceChildProcesses;
		m_traceWrittenFiles = info.traceWrittenFiles;
		m_logToFile = info.logToFile;
		if (info.extraInfo)
			m_extraInfo = info.extraInfo;

		if (info.deleteSessionsOlderThanSeconds)
		{
			StringBuffer<> sessionsDir;
			sessionsDir.Append(m_rootDir).Append(TCV("sessions"));

			u64 systemTimeAsFileTime = GetSystemTimeAsFileTime();

			TraverseDir(m_logger, sessionsDir,
				[&](const DirectoryEntry& e)
				{
					u64 seconds = GetFileTimeAsSeconds(systemTimeAsFileTime - e.lastWritten);
					if (seconds <= info.deleteSessionsOlderThanSeconds)
						return;

					if (IsDirectory(e.attributes)) // on macos we get a ".ds_store" file created by the os
					{
						StringBuffer<> sessionDir(sessionsDir);
						sessionDir.EnsureEndsWithSlash().Append(e.name);
						DeleteAllFiles(m_logger, sessionDir.data);
					}
				});
		}

		m_sessionBinDir.Append(m_sessionDir).Append(TCV("bin"));
		m_sessionOutputDir.Append(m_sessionDir).Append(TCV("output"));
		m_sessionLogDir.Append(m_sessionDir).Append(TCV("log"));

		if (m_runningRemote)
		{
			m_storage.CreateDirectory(m_sessionBinDir.data);
			m_storage.CreateDirectory(m_sessionOutputDir.data);
		}

		m_tempPath.Append(m_sessionDir).Append(TCV("temp"));
		m_storage.CreateDirectory(m_tempPath.data);
		m_tempPath.EnsureEndsWithSlash();

		m_sessionBinDir.EnsureEndsWithSlash();
		m_sessionOutputDir.EnsureEndsWithSlash();

		m_storage.CreateDirectory(m_sessionLogDir.data);
		m_sessionLogDir.EnsureEndsWithSlash();

		// We never want to populate files in Temp
		#if PLATFORM_WINDOWS
		if (info.treatTempDirAsEmpty)
		{
			wchar_t systemTemp[256];
			GetEnvironmentVariableW(L"TEMP", systemTemp, 256);
			StringBuffer<> temp;
			FixPath(systemTemp, nullptr, 0, temp);
			temp.MakeLower();
			m_directoryForcedEmpty = ToStringKey(temp);
		}
		#endif

		if (info.traceOutputFile)
			m_traceOutputFile.Append(info.traceOutputFile);

		if (m_readIntermediateFilesCompressed && !m_runningRemote)
			m_dependencyCrawler.Init(
				[this](const StringView& fileName, u32& outAttr) { return FileExists(m_logger, fileName.data, nullptr, &outAttr); }, // FileExists
				[this](const StringView& path, const DependencyCrawler::FileFunc& fileFunc) {}, // TraverseFilesFunc
				false);

	}

	bool Session::Create(const SessionCreateInfo& info)
	{
		#if UBA_DEBUG_LOGGER
		m_debugLogger = StartDebugLogger(m_logger, StringBuffer<512>().Append(m_sessionDir).Append(TCV("SessionDebug.log")).data);
		#endif

		#if PLATFORM_WINDOWS
		m_systemPath.count = GetEnvironmentVariableW(TC("SystemRoot"), m_systemPath.data, m_systemPath.capacity);
		#else
		m_systemPath.Append(TCV("/nonexistingpath"));
		#endif

		m_fileMappingTableHandle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE, FileMappingTableMemSize, nullptr, TC("FileMappings"));
		UBA_ASSERT(m_fileMappingTableHandle.IsValid());
		m_fileMappingTableMem = MapViewOfFile(m_logger, m_fileMappingTableHandle, FILE_MAP_WRITE, 0, FileMappingTableMemSize);
		UBA_ASSERT(m_fileMappingTableMem);

		m_directoryTableHandle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE|SEC_RESERVE, DirTableMemSize, nullptr, TC("DirMappings"));
		UBA_ASSERT(m_directoryTableHandle.IsValid());
		m_directoryTableMem = MapViewOfFile(m_logger, m_directoryTableHandle, FILE_MAP_WRITE, 0, DirTableMemSize);
		UBA_ASSERT(m_directoryTableMem);

		m_directoryTable.m_memory = m_directoryTableMem;
		m_directoryTable.m_lookup.reserve(30000);
		m_fileMappingTableLookup.reserve(70000);

		m_fileMappingBuffer.AddTransient(TC("FileMappings"), info.keepTransientDataMapped);

		if (!m_processCommunicationAllocator.Init(CommunicationMemSize, CommunicationMemSize * 512))
		{
			m_allowLocalDetour = false;
			m_logger.Warning(TC("Failed to create process communication allocator.. local detouring will be disabled."));
		}
		if (!CreateProcessJobObject())
			return false;

		// Environment variables that should stay local when building remote (not replicated)
		#if PLATFORM_WINDOWS
		m_localEnvironmentVariables.insert(TC("SystemRoot"));
		m_localEnvironmentVariables.insert(TC("SystemDrive"));
		m_localEnvironmentVariables.insert(TC("NUMBER_OF_PROCESSORS"));
		m_localEnvironmentVariables.insert(TC("PROCESSOR_ARCHITECTURE"));
		m_localEnvironmentVariables.insert(TC("PROCESSOR_IDENTIFIER"));
		m_localEnvironmentVariables.insert(TC("PROCESSOR_LEVEL"));
		m_localEnvironmentVariables.insert(TC("PROCESSOR_REVISION"));
		#endif

		if (info.useFakeVolumeSerial && !m_runningRemote)
			if (!m_volumeCache.Init(m_logger))
				return false;

		StringBuffer<> traceName;
		if (info.traceName && *info.traceName)
			traceName.Append(info.traceName);
		else if (info.launchVisualizer || !m_traceOutputFile.IsEmpty() || info.traceEnabled)
		{
			traceName.Append(m_id);

			OwnerInfo ownerInfo = GetOwnerInfo();
			if (ownerInfo.pid)
				traceName.Appendf(TC("_%s%u"), ownerInfo.id, ownerInfo.pid);

			if (!info.useUniqueId)
			{
				Guid guid;
				CreateGuid(guid);
				traceName.Append(GuidToString(guid).str);
			}
		}

		if (!traceName.IsEmpty())
			StartTrace(IsWindows ? traceName.data : nullptr, info.traceReserveSizeMb); // non-windows named shared memory not implemented (only needed for UbaVisualizer which you can't run on linux either way)

		#if PLATFORM_WINDOWS
		if (info.launchVisualizer)
		{
			HMODULE currentModule = GetModuleHandle(NULL);
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&g_dummy, &currentModule);
			tchar fileName[512];
			GetModuleFileNameW(currentModule, fileName, 512);
			uba::StringBuffer<> launcherCmd;
			launcherCmd.Append(TCV("\""));
			launcherCmd.AppendDir(fileName);
			launcherCmd.Append(TCV("\\UbaVisualizer.exe\""));
			launcherCmd.Append(TCV(" -named=")).Append(traceName);
			STARTUPINFOW si;
			memset(&si, 0, sizeof(si));
			PROCESS_INFORMATION pi;
			m_logger.Info(TC("Starting visualizer: %s"), launcherCmd.data);
			DWORD creationFlags = DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;
			CreateProcessW(NULL, launcherCmd.data, NULL, NULL, false, creationFlags, NULL, NULL, &si, &pi);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
		#endif

		m_storage.RegisterExternalFileMappingsProvider([this](Storage::ExternalFileMapping& out, StringKey fileNameKey, const tchar* fileName)
			{
				SCOPED_FUTEX_READ(m_fileMappingTableLookupLock, lookupLock);
				auto findIt = m_fileMappingTableLookup.find(fileNameKey);
				if (findIt == m_fileMappingTableLookup.end())
					return false;
				FileMappingEntry& entry = findIt->second;
				lookupLock.Leave();
				SCOPED_FUTEX_READ(entry.lock, entryLock);
				if (!entry.handled || !entry.success || !entry.mapping.IsValid())
					return false;
				out.handle = entry.mapping;
				out.offset = entry.mappingOffset;
				out.size = entry.contentSize;
				out.lastWriteTime = entry.lastWriteTime;
				out.storedSize = entry.storedSize;
				out.createIndependentMapping = entry.createIndependentMapping;
				return true;
			});

		return true;
	}

	Session::~Session()
	{
		StopTrace(m_traceOutputFile.data);

		CancelAllProcessesAndWait();
		FlushDeadProcesses();

		for (auto& kv : m_virtualSourceFiles)
			CloseFileMapping(m_logger, kv.second.mappingHandle, TC("VirtualFile"));

		for (auto& i : m_fileMappingTableLookup)
			if (i.second.canBeFreed)
				CloseFileMapping(m_logger, i.second.mapping, TC("FileMappingKeptFromOutput"));

		UnmapViewOfFile(m_logger, m_fileMappingTableMem, FileMappingTableMemSize, TC("FileMappingTable"));
		CloseFileMapping(m_logger, m_fileMappingTableHandle, TC("FileMappingTable"));

		UnmapViewOfFile(m_logger, m_directoryTableMem, DirTableMemSize, TC("DirectoryTable"));
		CloseFileMapping(m_logger, m_directoryTableHandle, TC("DirectoryTable"));

		//#if !UBA_DEBUG
		//u32 count;
		//DeleteAllFiles(m_logger, m_sessionDir.data, count);
		//#endif

		#if UBA_DEBUG_LOGGER
		m_debugLogger = StopDebugLogger(m_debugLogger);
		#endif
	}

	void Session::CancelAllProcessesAndWait(bool terminate)
	{
		bool isEmpty = false;
		bool isFirst = true;
		while (!isEmpty)
		{
			Vector<ProcessHandle> processes;
			{
				SCOPED_FUTEX(m_processesLock, lock);
				processes.reserve(m_processes.size());
				for (auto& pair : m_processes)
					if (pair.second.m_process && !pair.second.m_process->IsCalledFromThis())
						processes.push_back(pair.second);
				isEmpty = processes.empty();
			}

			if (isFirst)
			{
				isFirst = false;
				if (!isEmpty)
					m_logger.Info(TC("Cancelling %llu processes and wait for them to exit"), processes.size());
				++m_logger.isMuted;
			}

			for (auto& process : processes)
				process.Cancel(true);

			#if PLATFORM_WINDOWS
			if (m_processJobObject != NULL)
			{
				SCOPED_FUTEX(m_processJobObjectLock, lock);
				CloseHandle(m_processJobObject);
				m_processJobObject = NULL;
			}
			#endif

			for (auto& process : processes)
				process.WaitForExit(100000);
		}

		--m_logger.isMuted;
	}

	void Session::CancelAllProcesses()
	{
		Vector<ProcessHandle> processes;
		SCOPED_FUTEX(m_processesLock, lock);
		processes.reserve(m_processes.size());
		for (auto& pair : m_processes)
			processes.push_back(pair.second);
		lock.Leave();
		for (auto& process : processes)
			process.Cancel(true);
	}

	ProcessHandle Session::RunProcess(const ProcessStartInfo& startInfo, bool async, bool enableDetour)
	{
		FlushDeadProcesses();
		ValidateStartInfo(startInfo);
		enableDetour &= m_allowLocalDetour;
		return InternalRunProcess(startInfo, async, nullptr, enableDetour);
	}

	void Session::ValidateStartInfo(const ProcessStartInfo& startInfo)
	{
		UBA_ASSERTF(startInfo.workingDir && *startInfo.workingDir, TC("Working dir must be set when spawning process"));
		UBA_ASSERTF(!TStrchr(startInfo.workingDir, '~'), TC("WorkingDir path must use long name (%s)"), startInfo.workingDir);
	}

	ProcessHandle Session::InternalRunProcess(const ProcessStartInfo& startInfo, bool async, ProcessImpl* parent, bool enableDetour)
	{
		auto& si = const_cast<ProcessStartInfo&>(startInfo);
		const tchar* originalLogFile = si.logFile;
		
		u32 processId = CreateProcessId();

		StringBuffer<> logFile;
		if (si.logFile && *si.logFile)
		{
			if (TStrchr(si.logFile, PathSeparator) == nullptr)
			{
				logFile.Append(m_sessionLogDir).Append(si.logFile);
				si.logFile = logFile.data;
			}
		}
		else if (m_logToFile)
		{
			logFile.Append(m_sessionLogDir);
			GenerateNameForProcess(logFile, startInfo.arguments, processId);
			logFile.Append(TCV(".log"));
			si.logFile = logFile.data;
		}

		if (!si.rules)
			si.rules = GetRules(si);

		void* env = GetProcessEnvironmentVariables();
		auto process = new ProcessImpl(*this, processId, parent, enableDetour);
		ProcessHandle h(process);
		if (!process->Start(startInfo, m_runningRemote, env, async))
			h = {};

		si.logFile = originalLogFile;
		return h;
	}

	StringKey GetKeyAndFixedName(StringBuffer<>& outFixedFilePath, StringKeyHasher& outHasher, const tchar* filePath)
	{
		StringBuffer<> workingDir;
		if (!IsAbsolutePath(filePath))
		{
			GetCurrentDirectoryW(workingDir);
			workingDir.EnsureEndsWithSlash();
		}
		FixPath(filePath, workingDir.data, workingDir.count, outFixedFilePath);

		StringKey dirKey;
		StringBuffer<> dirNameForHash;
		const tchar* baseFileName = nullptr;
		GetDirKey(dirKey, dirNameForHash, baseFileName, outFixedFilePath);

		if (CaseInsensitiveFs)
			dirNameForHash.MakeLower();

		outHasher.Update(dirNameForHash);

		if (baseFileName)
		{
			StringBuffer<256> baseFileNameForHash;
			baseFileNameForHash.Append(baseFileName);
			if (CaseInsensitiveFs)
				baseFileNameForHash.MakeLower();

			outHasher.Update(baseFileNameForHash);
		}

		StringKey result = ToStringKey(outHasher);

#if UBA_DEBUG
		StringBuffer<> testPath(outFixedFilePath);
		if (CaseInsensitiveFs)
			testPath.MakeLower();
		StringKey testKey = ToStringKey(testPath);
		UBA_ASSERTF(testKey == result, TC("Key mismatch for %s"), outFixedFilePath.data);
#endif

		return result;
	}

	StringKey GetKeyAndFixedName(StringBuffer<>& outFixedFilePath, const tchar* filePath)
	{
		StringKeyHasher hasher;
		return GetKeyAndFixedName(outFixedFilePath, hasher, filePath);
	}

	bool Session::RefreshDirectory(const tchar* dirName, bool forceRegister)
	{
		UBA_ASSERT(!m_runningRemote);

		StringBuffer<> dirPath;
		StringKeyHasher hasher;
		GetKeyAndFixedName(dirPath, hasher, dirName);

		StringKey dirKey = ToStringKey(hasher);

		auto& dirTable = m_directoryTable;
		SCOPED_READ_LOCK(dirTable.m_lookupLock, lookupLock);
		auto res = dirTable.m_lookup.find(dirKey);
		if (res == dirTable.m_lookup.end())
		{
			lookupLock.Leave();
			if (forceRegister)
				WriteDirectoryEntries(dirKey, dirPath);
			return true;
		}
		DirectoryTable::Directory& dir = res->second;
		lookupLock.Leave();
		SCOPED_WRITE_LOCK(dir.lock, dirLock);

		while (dir.parseOffset == 0)
		{
			dirLock.Leave();
			Sleep(1);
			dirLock.Enter();
		}
		UBA_ASSERT(dir.parseOffset == 1);

		//m_directoryTable.PopulateDirectoryNoLock(hasher, dir);

		u32 tableOffset;
		return WriteDirectoryEntriesInternal(dir, dirKey, dirPath, true, tableOffset);
	}

	bool Session::RegisterNewFile(const tchar* filePath)
	{
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedFilePath;
		auto key = GetKeyAndFixedName(fixedFilePath, filePath);
		return RegisterCreateFileForWrite(key, fixedFilePath, true);
	}

	bool Session::RegisterVirtualFile(const tchar* filePath, const tchar* sourceFile, u64 sourceOffset, u64 sourceSize)
	{
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedFilePath;
		auto fileNameKey = GetKeyAndFixedName(fixedFilePath, filePath);
		if (!RegisterVirtualFileInternal(fileNameKey, fixedFilePath, sourceFile, sourceOffset, sourceSize))
			return false;
		return RegisterCreateFileForWrite(fileNameKey, fixedFilePath, false, sourceSize, 0, false);
	}

	bool Session::CreateVirtualFile(const tchar* filePath, const void* memory, u64 memorySize)
	{
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedFilePath;
		auto fileNameKey = GetKeyAndFixedName(fixedFilePath, filePath);
		if (!CreateVirtualFileInternal(fileNameKey, fixedFilePath, memory, memorySize))
			return false;
		return RegisterCreateFileForWrite(fileNameKey, fixedFilePath, false, memorySize, 0, false);
	}

	bool Session::DeleteVirtualFile(const tchar* filePath)
	{
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedFilePath;
		auto key = GetKeyAndFixedName(fixedFilePath, filePath);
		RegisterDeleteFile(key, fixedFilePath);

		SCOPED_FUTEX(m_virtualSourceFilesLock, virtualSourceFilesLock);
		auto findIt = m_virtualSourceFiles.find(key); 
		if (findIt == m_virtualSourceFiles.end())
			return false;
		VirtualSourceFile vsf = findIt->second;
		m_virtualSourceFiles.erase(findIt);
		virtualSourceFilesLock.Leave();
		CloseFileMapping(m_logger, vsf.mappingHandle, TC("VirtualFile"));
		return true;
	}

	void Session::RegisterDeleteFile(const tchar* filePath)
	{
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedFilePath;
		auto key = GetKeyAndFixedName(fixedFilePath, filePath);
		RegisterDeleteFile(key, fixedFilePath);
	}

	bool Session::RegisterNewDirectory(const tchar* directoryPath)
	{
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedDirPath;
		auto dirKey = GetKeyAndFixedName(fixedDirPath, directoryPath);
		if (!RegisterCreateFileForWrite(dirKey, fixedDirPath, true))
			return false;
		WriteDirectoryEntries(dirKey, fixedDirPath);
		return true;
	}

	void Session::RegisterCustomService(CustomServiceFunction&& function)
	{
		m_customServiceFunction = function;
	}

	void Session::RegisterGetNextProcess(GetNextProcessFunction&& function)
	{
		m_getNextProcessFunction = function;
	}

	bool Session::GetOutputFileSize(u64& outSize, const tchar* filePath)
	{
		if (m_shouldWriteToDisk)
			return m_logger.Error(TC("GetFileSize only implemented for path where ShouldWriteToDisk is false"));
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedFilePath;
		auto key = GetKeyAndFixedName(fixedFilePath, filePath);
		return GetOutputFileSizeInternal(outSize, key, fixedFilePath);
	}

	bool Session::GetOutputFileData(void* outData, const tchar* filePath, bool deleteInternalMapping)
	{
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedFilePath;
		auto key = GetKeyAndFixedName(fixedFilePath, filePath);
		return GetOutputFileDataInternal(outData, key, fixedFilePath, deleteInternalMapping);
	}

	bool Session::WriteOutputFile(const tchar* filePath, bool deleteInternalMapping)
	{
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedFilePath;
		auto key = GetKeyAndFixedName(fixedFilePath, filePath);
		return WriteOutputFileInternal(key, fixedFilePath, deleteInternalMapping);
	}

	const tchar* Session::GetId() { return m_id.data; }
	Storage& Session::GetStorage() { return m_storage; }
	MutableLogger& Session::GetLogger() { return m_logger; }
	LogWriter& Session::GetLogWriter() { return m_logger.m_writer; }
	Trace& Session::GetTrace() { return m_trace; }

	const ApplicationRules* Session::GetRules(const ProcessStartInfo& si)
	{
		const tchar* exeNameStart = si.application;
		const tchar* exeNameEnd = exeNameStart + TStrlen(si.application);
		UBA_ASSERT(exeNameEnd - exeNameStart > 1);
		if (const tchar* lastSeparator = TStrrchr(exeNameStart, PathSeparator))
			exeNameStart = lastSeparator + 1;
		if (const tchar* lastSeparator2 = TStrrchr(exeNameStart, NonPathSeparator))
			exeNameStart = lastSeparator2 + 1;
		if (*exeNameStart == '"')
			++exeNameStart;
		if (exeNameEnd[-1] == '"')
			--exeNameEnd;
		StringBuffer<128> exeName;
		UBA_ASSERTF(exeNameStart < exeNameEnd, TC("Bad application string: %s"), si.application);
		exeName.Append(exeNameStart, exeNameEnd - exeNameStart);
		
		auto rules = GetApplicationRules();
		
		bool isDotnet = false;

		while (true)
		{
			exeName.MakeLower();
			u32 appHash = GetApplicationHash(exeName);

			for (u32 i = 1;; ++i)
			{
				u32 hash = rules[i].hash;
				if (!hash)
					break;
				if (appHash != hash)
					continue;
				return rules[i].rules;
			}

			if (!exeName.Equals(TCV("dotnet.exe")))
				return rules[isDotnet].rules;
			
			isDotnet = true;

			u32 firstArgumentStart = 0;
			u32 firstArgumentEnd = 0;
			bool quoted = false;
			for (u32 i = 0, e = TStrlen(si.arguments); i != e; ++i)
			{
				tchar c = si.arguments[i];
				if (firstArgumentEnd)
				{
					if (c == '\\')
						firstArgumentStart = i + 1;
					firstArgumentEnd = i + 1;
					if ((quoted && c != '"') || (!quoted && c != ' ' && c != '\t'))
						continue;
					firstArgumentEnd = i;
					break;
				}
				else
				{
					if (c == ' ' || c == '\t')
					{
						++firstArgumentStart;
						continue;
					}
					else if (c == '"')
					{
						++firstArgumentStart;
						quoted = true;
					}
					firstArgumentEnd = firstArgumentStart + 1;
				}
			}
			exeName.Clear().Append(si.arguments + firstArgumentStart, firstArgumentEnd - firstArgumentStart);
		}
		return rules[isDotnet].rules;
	}

	const tchar* Session::GetTempPath()
	{
		return m_tempPath.data;
	}
	
	const tchar* Session::GetRootDir()
	{
		return m_rootDir.data;
	}

	const tchar* Session::GetSessionDir()
	{
		return m_sessionDir.data;
	}

	u32 Session::CreateProcessId()
	{
		return ++m_processIdCounter;
	}

	bool Session::VirtualizePath(StringBufferBase& inOut, RootsHandle rootsHandle)
	{
		if (!rootsHandle)
			return true;
		if (IsWindows ? inOut[1] != ':' : inOut[0] != '/')
			return true;
		auto rootsEntry = GetRootsEntry(rootsHandle);
		if (!rootsEntry)
			return false;
		if (rootsEntry->roots.IsEmpty())
			return true;
		auto& locals = rootsEntry->locals;
		auto& vfs = rootsEntry->vfs;
		for (u32 i=0, e=u32(locals.size()); i!=e; ++i)
		{
			if (!inOut.StartsWith(locals[i].c_str()))
				continue;
			StringBuffer<> temp;
			temp.Append(inOut.data + locals[i].size());
			inOut.Clear().Append(vfs[i]).Append(temp);
			return true;
		}

		return true;
	}

	bool Session::DevirtualizePath(StringBufferBase& inOut, RootsHandle rootsHandle, bool reportError)
	{
		if (!rootsHandle)
			return true;
		if (IsWindows ? inOut[1] != ':' : inOut[0] != '/')
			return true;
		auto rootsEntry = GetRootsEntry(rootsHandle);
		if (!rootsEntry)
			return false;
		if (rootsEntry->roots.IsEmpty())
			return true;
		auto root = rootsEntry->roots.FindRoot(inOut);
		if (!root)
			return reportError ? m_logger.Error(TC("Can't find root for path %s (Available roots: %s)"), inOut.data, rootsEntry->roots.GetAllRoots().c_str()) : false;

		auto& path = rootsEntry->locals[root->index/PathsPerRoot];
		StringBuffer<> temp;
		temp.Append(inOut.data + root->path.size());
		inOut.Clear().Append(path).Append(temp);
		return true;
	}

	bool Session::DevirtualizeString(TString& inOut, RootsHandle rootsHandle, bool allowPathsWithoutRoot, const tchar* hint)
	{
		if (!HasVfs(rootsHandle))
			return true;
		auto rootsEntry = GetRootsEntry(rootsHandle);
		if (!rootsEntry)
			return false;

		u64 newSize = 0;
		bool hasRoot = false;
		auto checkString = [&](const tchar* str, u64 strLen, u32 rootPos)
			{
				if (rootPos == ~0u)
				{
					newSize += strLen;
					return;
				}
				hasRoot = true;
				auto& path = rootsEntry->locals[(*str - RootPaths::RootStartByte)/PathsPerRoot];
				newSize += path.size();
				#if PLATFORM_WINDOWS
				u32 rootIndex = (*str - RootPaths::RootStartByte);
				u32 type = rootIndex % PathsPerRoot;
				if (type == 2) // DoubleForward
					for (tchar c : path)
						newSize += c == '\\';

				#endif
			};

		if (!rootsEntry->roots.NormalizeString<tchar>(m_logger, inOut.data(), inOut.size(), checkString, allowPathsWithoutRoot, hint))
			return false;

		if (!hasRoot)
			return false;

		TString newString;
		newString.resize(newSize);
		tchar* newStringPos = newString.data();

		auto handleString = [&](const tchar* str, u64 strLen, u32 rootPos)
			{
				if (rootPos == ~0u)
				{
					memcpy(newStringPos, str, strLen*sizeof(tchar));
					newStringPos += strLen;
					return;
				}
				u32 rootIndex = (*str - RootPaths::RootStartByte);
				u32 localsIndex = rootIndex/PathsPerRoot;
				auto& path = rootsEntry->locals[localsIndex];
				tchar* start = newStringPos;
				newStringPos += path.size();
				memcpy(start, path.data(), path.size()*sizeof(tchar));

				#if PLATFORM_WINDOWS
				u32 type = rootIndex % PathsPerRoot;
				if (type == 1) // just backslash
				{
				}
				else if (type == 0)
				{
					*newStringPos = 0;
					Replace(start, '\\', '/');
				}
				else if (type == 2) // Double forward slash
				{
					*newStringPos = 0;
					for (tchar* it=start; it!=newStringPos;++it)
						if (*it == '\\')
						{
							*it = '/';
							memmove(it+1, it, (newStringPos-it)*sizeof(tchar));
							++newStringPos;
						}
				}
				else
				{
					UBA_ASSERTF(false, TC("Not root path type %u not implemented (%s)"), type, hint); // We don't like double backslash or escaped space
				}
				#endif
			};

		rootsEntry->roots.NormalizeString<tchar>(m_logger, inOut.data(), inOut.size(), handleString, allowPathsWithoutRoot, hint);

		UBA_ASSERT(newStringPos == newString.data() + newString.size());
		inOut = std::move(newString);

		return true;
	}

	bool Session::PopulateLocalToIndexRoots(RootPaths& out, RootsHandle rootsHandle)
	{
		if (!rootsHandle)
			return true;
		auto rootsEntry = GetRootsEntry(rootsHandle);
		if (!rootsEntry)
			return false;

		BinaryReader reader((const u8*)rootsEntry->memory.data(), 0, rootsEntry->memory.size());
		while (reader.GetLeft())
		{
			u8 id = reader.ReadByte();
			reader.SkipString();
			StringBuffer<> rootPath;
			reader.ReadString(rootPath);
			if (!out.RegisterRoot(m_logger, rootPath.data, true, id))
				return false;
		}

		// TODO: Provide or calculate these
		#if PLATFORM_WINDOWS
		out.RegisterIgnoredRoot(m_logger, TC("z:/UEVFS"));
		#else
		out.RegisterIgnoredRoot(m_logger, TC("/UEVFS"));
		#endif

		return true;//out.RegisterSystemRoots(m_logger, 0);
	}

	void Session::ProcessAdded(Process& process, u32 sessionId)
	{
		u32 processId = process.GetId();

		auto& startInfo = process.GetStartInfo();
		if (!process.IsChild() || m_traceChildProcesses)
			m_trace.ProcessAdded(sessionId, processId, ToView(startInfo.GetDescription()), ToView(startInfo.breadcrumbs));

		SCOPED_FUTEX(m_processesLock, lock);
		bool success = m_processes.try_emplace(processId, ProcessHandle(&process)).second;
		UBA_ASSERT(success);(void)success;
	}

	void Session::ProcessExited(ProcessImpl& process, u64 executionTime)
	{
		const tchar* application = process.GetStartInfo().application;
		StringBuffer<> applicationName;
		applicationName.AppendFileName(application);
		if (applicationName.count > 21)
			applicationName[21] = 0;

		u32 id = process.GetId();

		if (!process.IsChild() || m_traceChildProcesses)
		{
			StackBinaryWriter<1024> writer;
			process.m_processStats.Write(writer);
			process.m_sessionStats.Write(writer);
			process.m_storageStats.Write(writer);
			process.m_kernelStats.Write(writer);
			u32 exitCode = process.GetExitCode();
			Vector<ProcessLogLine> emptyLines;
			auto& logLines = (exitCode != 0 || m_detailedTrace) ? process.m_logLines : emptyLines;
			m_trace.ProcessExited(id, exitCode, writer.GetData(), writer.GetPosition(), logLines);
			SCOPED_FUTEX(m_processStatsLock, lock);
			m_processStats.Add(process.m_processStats);
			m_stats.Add(process.m_sessionStats);
		}

		SCOPED_FUTEX(process.m_usedFileMappingsLock, usedFileMappingsLock);
		auto usedFileMappings = std::move(process.m_usedFileMappings); // Might be that crawler is still working
		usedFileMappingsLock.Leave();

		for (auto& fileNameKey : usedFileMappings)
		{
			SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
			auto findIt = m_fileMappingTableLookup.find(fileNameKey);
			UBA_ASSERT(findIt != m_fileMappingTableLookup.end());
			FileMappingEntry& entry = findIt->second;
			lookupLock.Leave();

			SCOPED_FUTEX(entry.lock, entryLock);

			if (entry.usedCount < entry.usedCountBeforeFree)
				++entry.usedCount;

			if (--entry.refCount)
				continue;

			if (entry.usedCountBeforeFree == 255 || entry.usedCount < entry.usedCountBeforeFree)
				continue;

			UBA_ASSERT(entry.canBeFreed);

			#if UBA_DEBUG_TRACK_MAPPING
			m_debugLogger->Info(TC("Mapping freed 0x%llx (%s)"), u64(entry.mapping.mh), entry.name.c_str());
			#endif

			m_workManager.AddWork([mh = entry.mapping, this](const WorkContext&) { CloseFileMapping(m_logger, mh, TC("UsedFileMapping")); }, 1, TC("CloseFileMapping"));
			entry.handled = false;
			entry.mapping = {};
		}



		SCOPED_FUTEX(m_processesLock, lock);
		m_deadProcesses.emplace_back(&process); // Here to prevent Process thread call trigger a delete of Process which causes a deadlock
		auto& stats = m_applicationStats[applicationName.data];
		stats.count++;
		stats.time += executionTime;
		auto count = m_processes.erase(id);
		UBA_ASSERT(count == 1);(void)count;
	}

	void Session::FlushDeadProcesses()
	{
		SCOPED_FUTEX(m_processesLock, lock);
		Vector<ProcessHandle> deadProcesses;
		deadProcesses.swap(m_deadProcesses);
		lock.Leave();
	}

	bool Session::ProcessThreadStart(ProcessImpl& process)
	{
		return true;
	}

	bool Session::GetInitResponse(InitResponse& out, const InitMessage& msg)
	{
		out.directoryTableHandle = m_directoryTableHandle.ToU64();
		{
			SCOPED_READ_LOCK(m_directoryTable.m_memoryLock, l);
			out.directoryTableSize = (u32)m_directoryTable.m_memorySize;
		}
		{
			SCOPED_READ_LOCK(m_directoryTable.m_lookupLock, l);
			out.directoryTableCount = (u32)m_directoryTable.m_lookup.size();
		}
		out.mappedFileTableHandle = m_fileMappingTableHandle.ToU64();
		{
			SCOPED_READ_LOCK(m_fileMappingTableMemLock, l);
			out.mappedFileTableSize = m_fileMappingTableSize;
		}
		{
			SCOPED_FUTEX_READ(m_fileMappingTableLookupLock, l);
			out.mappedFileTableCount = (u32)m_fileMappingTableLookup.size();
		}
		return true;
	}

	u32 Session::GetDirectoryTableSize()
	{
		SCOPED_READ_LOCK(m_directoryTable.m_memoryLock, lock);
		return m_directoryTable.m_memorySize;
	}

	u32 Session::GetFileMappingSize()
	{
		SCOPED_READ_LOCK(m_fileMappingTableMemLock, lock);
		return m_fileMappingTableSize;
	}

	SessionStats& Session::Stats()
	{
		if (SessionStats* s = SessionStats::GetCurrent())
			return *s;
		return m_stats;
	}

	u32 Session::GetActiveProcessCount()
	{
		SCOPED_FUTEX_READ(m_processesLock, cs);
		return u32(m_processes.size());
	}

	void Session::PrintProcessStats(ProcessStats& stats, const tchar* logName)
	{
		m_logger.Info(TC("  -- %s --"), logName);
		stats.Print(m_logger);
	}

	void Session::StartTrace(const tchar* traceName, u32 traceReserveSizeMb)
	{
		if (traceName)
			LoggerWithWriter(m_logger.m_writer).Info(TC("---- Starting trace: %s ----"), traceName);
		else
			LoggerWithWriter(m_logger.m_writer).Info(TC("---- Starting trace ----"));

		u64 traceReserveSize = u64(traceReserveSizeMb)*1024*1024;
		if (m_detailedTrace)
			traceReserveSize *= 2;

		if (!m_trace.StartWrite(traceName, traceReserveSize))
			return;

		StringBuffer<256> buf;
		if (!GetComputerNameW(buf))
			buf.Append(TCV("LOCAL"));
		StringBuffer<> systemInfo;
		GetSystemInfo(systemInfo);
		m_trace.SessionAdded(0, {}, buf, systemInfo);
		StartTraceThread();
	}

	bool Session::StopTrace(const tchar* writeFile)
	{
		StopTraceThread();
		return m_trace.StopWrite(writeFile);
	}

	bool Session::SaveSnapshotOfTrace()
	{
		return m_trace.Write(m_traceOutputFile.data, true);
	}

	void Session::StartTraceThread()
	{
		m_traceThreadEvent.Create(true);
		m_traceThread.Start([this]() { ThreadTraceLoop(); return 0; }, TC("UbaTraceLoop"));
	}

	void Session::StopTraceThread()
	{
		m_traceThreadEvent.Set();
		m_traceThread.Wait();
	}

	void Session::PrintSummary(Logger& logger)
	{
		logger.BeginScope();
		logger.Info(TC("  ------- Detours stats summary -------"));
		m_processStats.Print(logger);
		logger.Info(TC(""));

		MultiMap<u64, std::pair<const TString*, u32>> sortedApps;
		for (auto& pair : m_applicationStats)
			sortedApps.insert({pair.second.time, {&pair.first, pair.second.count}});
		for (auto i=sortedApps.rbegin(), e=sortedApps.rend(); i!=e; ++i)
		{
			const TString& name = *i->second.first;
			u64 time = i->first;
			u32 count = i->second.second;
			logger.Info(TC("  %-21s %5u %9s"), name.c_str(), count, TimeToText(time).str);
		}
		logger.Info(TC(""));

		logger.Info(TC("  ------- Session stats summary -------"));

		PrintSessionStats(logger);
		logger.EndScope();
	}

	bool Session::GetBinaryModules(Vector<BinaryModule>& out, const tchar* application)
	{
		const tchar* applicationName = application;

		if (tchar* lastSlash = TStrrchr((tchar*)application, PathSeparator))
			applicationName = lastSlash + 1;

		u64 applicationDirLen = u64(applicationName - application);
		tchar applicationDir[512];
		UBA_ASSERT(applicationDirLen < 512);
		memcpy(applicationDir, application, applicationDirLen * sizeof(tchar));
		tchar* applicationDirEnd = applicationDir + applicationDirLen;

		UnorderedSet<TString> handledImports;
		return CopyImports(out, applicationName, applicationDir, applicationDirEnd, handledImports, nullptr);
	}

	void Session::Free(Vector<BinaryModule>& v)
	{
		v.resize(0);
		v.shrink_to_fit();
	}

	bool Session::IsRarelyRead(ProcessImpl& process, const StringView& fileName) const
	{
		return process.m_startInfo.rules->IsRarelyRead(fileName);
	}

	bool Session::IsRarelyReadAfterWritten(ProcessImpl& process, const StringView& fileName) const
	{
		return process.m_startInfo.rules->IsRarelyReadAfterWritten(fileName);
	}

	bool Session::IsKnownSystemFile(const tchar* applicationName)
	{
#if PLATFORM_WINDOWS
		return uba::IsKnownSystemFile(applicationName);
#else
		return false;
#endif
	}

	bool Session::ShouldWriteToDisk(const StringView& fileName)
	{
		if (m_shouldWriteToDisk)
			return true;
		return fileName.EndsWith(TCV(".h"));
	}

	bool Session::PrepareProcess(ProcessImpl& process, bool isChild, StringBufferBase& outRealApplication, const tchar*& outRealWorkingDir)
	{
		ProcessStartInfoHolder& startInfo = process.m_startInfo;
		if (StartsWith(startInfo.application, TC("ubacopy")))
			return true;

		if (!IsAbsolutePath(startInfo.application))
		{
			if (!SearchPathForFile(m_logger, outRealApplication.Clear(), startInfo.application, ToView(startInfo.workingDir), {}))
				return false;
			startInfo.applicationStr = outRealApplication.data;
			startInfo.application = startInfo.applicationStr.c_str();
		}

		if (!isChild && !m_runningRemote && m_readIntermediateFilesCompressed && m_allowLinkDependencyCrawler)
		{
			auto crawlerType = startInfo.rules->GetDependencyCrawlerType();
			if (crawlerType == DependencyCrawlerType_MsvcLinker || crawlerType == DependencyCrawlerType_ClangLinker)
				RunDependencyCrawler(process);
		}

		if (!m_allowCustomAllocator)
			startInfo.useCustomAllocator = false;

		return true;
	}

	u32 Session::GetMemoryMapAlignment(const StringView& fileName) const
	{
		return GetMemoryMapAlignment(fileName, m_runningRemote);
	}

	u32 Session::GetMemoryMapAlignment(const StringView& fileName, bool runningRemote) const
	{
		// It is not necessarily better to make mem maps of everything.. only things that are read more than once in the build.
		// Reason is because there is additional overhead to use memory mappings.
		// Upside is that all things that are memory mapped can be stored compressed in cas storage so it saves space.

		if (fileName.EndsWith(TCV(".h")) || fileName.EndsWith(TCV(".inl")) || fileName.EndsWith(TCV(".gch")))
			return 4 * 1024; // clang seems to need 4k alignment? Is it a coincidence it works or what is happening inside the code? (msvc works with alignment 1byte here)
		if (fileName.EndsWith(TCV(".lib")))
			return 4 * 1024;

		if (runningRemote) // We store these compressed to save space
		{
			if (fileName.EndsWith(TCV(".obj")) || fileName.EndsWith(TCV(".o")))
				return 4 * 1024; // pch needs 64k alignment
			if (fileName.EndsWith(TCV(".pch")))
				return 64 * 1024; // pch needs 64k alignment
		}
		else
		{
			if (fileName.EndsWith(TCV(".h.obj")))
				return 4 * 1024;
		}
		return 0;
	}

	void* Session::GetProcessEnvironmentVariables()
	{
		SCOPED_FUTEX(m_environmentVariablesLock, lock);
		if (!m_environmentVariables.empty())
			return m_environmentVariables.data();

#if PLATFORM_WINDOWS
		auto HandleEnvironmentVar = [&](const tchar* env)
		{
			StringBuffer<> varName;
			varName.Append(env, TStrchr(env, '=') - env);
			const tchar* varValue = env + varName.count + 1;

			if (m_runningRemote && varName.Equals(TCV("PATH")))
			{
				AddEnvironmentVariableNoLock(TC("PATH"), TC("c:\\noenvironment"));
				return;
			}
			if (varName.Equals(TCV("TEMP")) || varName.Equals(TCV("TMP")))
			{
				AddEnvironmentVariableNoLock(varName.data, m_tempPath.data);
				return;
			}
			if (varName.Equals(TCV("_CL_")) || varName.Equals(TCV("CL")))
			{
				return;
			}

			AddEnvironmentVariableNoLock(varName.data, varValue);
		};

		if (m_environmentMemory.empty())
		{
			auto strs = GetEnvironmentStringsW();
			for (auto env = strs; *env; env += TStrlen(env) + 1)
				HandleEnvironmentVar(env);
			FreeEnvironmentStrings(strs);
		}
		else
		{
			BinaryReader reader(m_environmentMemory.data(), 0, m_environmentMemory.size());
			while (reader.GetLeft())
				HandleEnvironmentVar(reader.ReadString().c_str());
		}
		AddEnvironmentVariableNoLock(TC("MSBUILDDISABLENODEREUSE"), TC("1")); // msbuild will reuse existing helper nodes but since those are not detoured we can't let that happen
		AddEnvironmentVariableNoLock(TC("DOTNET_CLI_USE_MSBUILD_SERVER"), TC("0")); // Disable msbuild server
		AddEnvironmentVariableNoLock(TC("DOTNET_CLI_TELEMETRY_OPTOUT"), TC("1")); // Stop talking to telemetry service
#else
		auto HandleEnvironmentVar = [&](const tchar* env)
		{
			if (StartsWith(env, "TMPDIR="))
				return;

			if (!StartsWith(env, "PATH="))
			{
				m_environmentVariables.insert(m_environmentVariables.end(), env, env + TStrlen(env) + 1);
				return;
			}

			TString paths;

			const char* start = env + 5;
			const char* it = start;
			bool isLast = false;
			while (!isLast)
			{
				if (*it != ':')
				{
					if (*it)
					{
						++it;
						continue;
					}
					isLast = true;
				}

				const char* s = start;
				const char* e = it;
				start = ++it;

				if (StartsWith(s, "/mnt/"))
					continue;
				if (!paths.empty())
					paths += ":";
				paths.append(s, e);
			}
			AddEnvironmentVariableNoLock("PATH", paths.c_str());
		};

		if (m_environmentMemory.empty())
		{
			int i = 0;
			while (char* env = environ[i++])
				HandleEnvironmentVar(env);
		}
		else
		{
			BinaryReader reader(m_environmentMemory.data(), 0, m_environmentMemory.size());
			while (reader.GetLeft())
				HandleEnvironmentVar(reader.ReadString().c_str());
		}
		AddEnvironmentVariableNoLock("TMPDIR", m_tempPath.data);
#endif

		AddEnvironmentVariableNoLock(TC("UBA_DETOURED"), TC("1"));

		m_environmentVariables.push_back(0);
		return m_environmentVariables.data();
	}

	bool Session::CreateFile(CreateFileResponse& out, const CreateFileMessage& msg)
	{
		const StringBufferBase& fileName = msg.fileName;
		const StringKey& fileNameKey = msg.fileNameKey;

		if ((msg.access & ~FileAccess_Read) == 0)
		{
			TrackWorkScope tws(m_workManager, AsView(TC("CreateFile")), ColorWork);
			tws.AddHint(msg.fileName);
			return CreateFileForRead(out, tws, fileName, fileNameKey, msg.process, *msg.process.m_startInfo.rules);
		}

		auto tableSizeGuard = MakeGuard([&]()
			{
				out.mappedFileTableSize = GetFileMappingSize();
				out.directoryTableSize = GetDirectoryTableSize();
			});

		// if ((message.Access & FileAccess.Write) != 0)
		m_storage.ReportFileWrite(fileNameKey, fileName.data);

		if (m_runningRemote && !fileName.StartsWith(m_tempPath.data))
		{
			SCOPED_FUTEX(m_outputFilesLock, lock);
			auto insres = m_outputFiles.try_emplace(fileName.data);
			if (insres.second)
			{
				out.fileName.Append(m_sessionOutputDir).Append(KeyToString(fileNameKey));
				insres.first->second = out.fileName.data;
			}
			else
			{
				out.fileName.Append(insres.first->second.c_str());
			}
		}
		else
		{
			out.fileName.Append(fileName);
		}

		UBA_ASSERT(fileNameKey != StringKeyZero);
		SCOPED_FUTEX(m_activeFilesLock, lock);
		u32 wantsOnCloseId = m_wantsOnCloseIdCounter++;
		out.closeId = wantsOnCloseId;
		auto insres = m_activeFiles.try_emplace(wantsOnCloseId);
		if (!insres.second)
			return m_logger.Error(TC("TRYING TO ADD %s twice!"), out.fileName.data);

		insres.first->second.name = fileName.data;
		insres.first->second.nameKey = fileNameKey;
		return true;
	}

	bool Session::CreateFileForRead(CreateFileResponse& out, TrackWorkScope& tws, const StringView& fileName, const StringKey& fileNameKey, ProcessImpl& process, const ApplicationRules& rules)
	{
		auto tableSizeGuard = MakeGuard([&]()
			{
				out.mappedFileTableSize = GetFileMappingSize();
				out.directoryTableSize = GetDirectoryTableSize();
			});

		if constexpr (!IsWindows)
		{
			out.fileName.Append(fileName);
			return true;
		}
		
		if (fileName.EndsWith(TCV(".dll")) || fileName.EndsWith(TCV(".exe")))
		{
			UBA_ASSERTF(IsAbsolutePath(fileName.data), TC("Got bad filename from process (%s)"), fileName.data);
			AddFileMapping(fileNameKey, fileName.data, TC("#"));
			out.fileName.Append(TCV("#"));
			return true;
		}
			
		if (m_allowMemoryMaps)
		{
			u64 alignment = GetMemoryMapAlignment(fileName);
			bool canBeCompressed = m_readIntermediateFilesCompressed && g_globalRules.FileCanBeCompressed(fileName);
			bool useMemoryMap = alignment != 0 || canBeCompressed;
			if (useMemoryMap)
			{
				MemoryMap map;
				bool canBeFreed = canBeCompressed;
				if (GetOrCreateMemoryMapFromFile(map, fileNameKey, fileName.data, false, alignment, TC("CreateFile"), &process, canBeFreed))
				{
					out.size = map.size;
					out.fileName.Append(map.name);
				}
				else
				{
					out.fileName.Append(fileName);
				}
				return true;
			}
		}

		if (!IsRarelyRead(process, fileName))
		{
			AddFileMapping(fileNameKey, fileName.data, TC("#"));
			out.fileName.Append(TCV("#"));
			return true;
		}

		out.fileName.Append(fileName);
		return true;
	}

	void Session::RemoveWrittenFile(ProcessImpl& process, const StringKey& fileKey)
	{
		SCOPED_FUTEX(process.m_shared.writtenFilesLock, writtenLock);
		auto& writtenFiles = process.m_shared.writtenFiles;
		auto findIt = writtenFiles.find(fileKey);
		if (findIt == writtenFiles.end())
			return;

		FileMappingHandle h = findIt->second.mappingHandle;
		TString name = findIt->second.name;
		writtenFiles.erase(findIt);
		writtenLock.Leave();

		if (!h.IsValid())
			return;

		#if UBA_DEBUG_TRACK_MAPPING
		m_debugLogger->Info(TC("Removed %s with handle 0x%llx"), name.c_str(), h.mh);
		#endif

		CloseFileMapping(process.m_session.GetLogger(), h, name.c_str());
	}

	bool Session::CloseFile(CloseFileResponse& out, const CloseFileMessage& msg)
	{
		SCOPED_FUTEX(m_activeFilesLock, lock);
		auto findIt = m_activeFiles.find(msg.closeId);
		if (findIt == m_activeFiles.end())
			return m_logger.Error(TC("This should not happen. Got unknown closeId %u - %s"), msg.closeId, msg.fileName.data);

		ActiveFile activeFile = findIt->second;
		m_activeFiles.erase(msg.closeId);
		lock.Leave();

		if (!msg.success)
		{
			return true;
		}

		bool registerRealFile = true;
		u64 fileSize = 0;
		u64 lastWriteTime = 0;

		ProcessStartInfo& startInfo = msg.process.m_startInfo;
		auto& rules = *startInfo.rules;

		if (msg.deleteOnClose)
		{
			RemoveWrittenFile(msg.process, activeFile.nameKey);
		}
		else
		{
			StringKey key = activeFile.nameKey;
			StringView name = activeFile.name;
			StringView msgName = msg.fileName;
			if (!msg.newName.IsEmpty())
			{
				UBA_ASSERT(!msg.deleteOnClose);
				RemoveWrittenFile(msg.process, key);
				name = msg.newName;
				key = msg.newNameKey;
				if (!m_runningRemote)
					msgName = msg.newName;
			}
			UBA_ASSERT(key != StringKeyZero);
			SCOPED_FUTEX(msg.process.m_shared.writtenFilesLock, writtenLock);
			auto insres = msg.process.m_shared.writtenFiles.try_emplace(key);
			WrittenFile& writtenFile = insres.first->second;

			if (m_allowOutputFiles && writtenFile.owner != nullptr && writtenFile.owner != &msg.process)
			{
				// This can happen when library has /GL (whole program optimization) but target has not.. then link.exe will restart
				//UBA_ASSERTF(false, TC("File %s changed owner.. should not happen (OldOwner: %s New owner: %s)"), name, writtenFile.owner->m_realApplication.c_str(), msg.process.m_realApplication.c_str());
			}

			writtenFile.attributes = msg.attributes;

			bool addMapping = true;
			if (insres.second)
			{
				writtenFile.name = name.ToString();
				writtenFile.key = key;
				writtenFile.backedName = msgName.ToString();
				writtenFile.owner = &msg.process;
			}
			else
			{
				if (writtenFile.backedName != msgName.data)
				{
					UBA_ASSERT(!msg.mappingHandle.IsValid() && !writtenFile.mappingHandle.IsValid());
					writtenFile.backedName = msgName.ToString();
				}

				if (!msg.mappingHandle.IsValid() || (msg.mappingHandle == writtenFile.originalMappingHandle && writtenFile.owner == &msg.process))
				{
					if (msg.mappingWritten)
					{
						writtenFile.mappingWritten = msg.mappingWritten;
						writtenFile.lastWriteTime = GetSystemTimeAsFileTime();
					}
					addMapping = false;
				}
				else if (writtenFile.mappingHandle.IsValid())
				{
					#if UBA_DEBUG_TRACK_MAPPING
					m_debugLogger->Info(TC("Closing old mapping 0x%llx for %s"), u64(writtenFile.mappingHandle.mh), writtenFile.name.c_str());
					#endif

					CloseFileMapping(m_logger, writtenFile.mappingHandle, msg.fileName.data);
					writtenFile.mappingHandle = {};
					writtenFile.originalMappingHandle = {};
				}

				writtenFile.owner = &msg.process;
			}

			if (!m_runningRemote && HasVfs(startInfo.rootsHandle)) // For posix we write the dependency file directly to disk so we need to update it if vfs is enabled
			{
				bool escapeSpaces;
				if (!msg.mappingHandle.IsValid() && rules.ShouldDevirtualizeFile(activeFile.name, escapeSpaces))
				{
					// TODO: On linux we don't use file mappings for outputs yet.. so we have to open the file and change it
					FileAccessor readFile(m_logger, name.data);
					if (!readFile.OpenMemoryRead())
						return false;
					void* mem = readFile.GetData();
					fileSize = readFile.GetSize();
					MemoryBlock block(5*1024*1024);
					RootsHandle rootsHandle = startInfo.rootsHandle;
					if (!DevirtualizeDepsFile(rootsHandle, block, mem, fileSize, escapeSpaces, name.data))
						return false;
					if (!readFile.Close())
						return false;
					FileAccessor writeFile(m_logger, name.data);
					if (!writeFile.CreateWrite())
						return false;
					if (!writeFile.Write(block.memory, block.writtenSize))
						return false;
					if (!writeFile.Close(&lastWriteTime))
						return false;
					fileSize = block.writtenSize;
				}
			}

			if (addMapping)
			{
				FileMappingHandle mappingHandle;
				if (msg.mappingHandle.IsValid())
					if (!DuplicateFileMapping(m_logger, msg.process.m_nativeProcessHandle, msg.mappingHandle, GetCurrentProcessHandle(), mappingHandle, 0, false, DUPLICATE_SAME_ACCESS, msgName.data))
						return m_logger.Error(TC("Failed to duplicate file mapping handle for %s"), name.data);

				writtenFile.mappingHandle = mappingHandle;
				writtenFile.mappingWritten = msg.mappingWritten;
				writtenFile.originalMappingHandle = msg.mappingHandle;
				writtenFile.lastWriteTime = GetSystemTimeAsFileTime();

				#if UBA_DEBUG_TRACK_MAPPING
				m_debugLogger->Info(TC("Adding written file with mapping 0x%llx (from 0x%llx) for %s"), u64(writtenFile.mappingHandle.mh), u64(msg.mappingHandle.mh), writtenFile.name.c_str());
				#endif
			}

			if (writtenFile.mappingHandle.IsValid())
			{
				registerRealFile = false;
				fileSize = writtenFile.mappingWritten;
				lastWriteTime = writtenFile.lastWriteTime;
			}

			if ((msg.process.m_extractExports || m_extractObjFilesSymbols) && rules.ShouldExtractSymbols(activeFile.name) && fileSize != 0)
				if (!ExtractSymbolsFromObjectFile(msg, name.data, fileSize))
					return false;
		}

		if (!rules.IsInvisible(activeFile.name))
		{
			if (!msg.newName.IsEmpty())
			{
				RegisterDeleteFile(activeFile.nameKey, activeFile.name);
				if (RegisterCreateFileForWrite(msg.newNameKey, msg.newName, registerRealFile, fileSize, lastWriteTime) && registerRealFile)
					TraceWrittenFile(msg.process.m_id, msg.newName, fileSize);
			}
			else if (msg.deleteOnClose)
				RegisterDeleteFile(activeFile.nameKey, activeFile.name);
			else
			{
				if (RegisterCreateFileForWrite(activeFile.nameKey, activeFile.name, registerRealFile, fileSize, lastWriteTime) && registerRealFile)
					TraceWrittenFile(msg.process.m_id, activeFile.name, fileSize);
			}
		}

		out.directoryTableSize = GetDirectoryTableSize();
		return true;
	}

	bool Session::DeleteFile(DeleteFileResponse& out, const DeleteFileMessage& msg)
	{
		out.result = true;
		bool deleteRealFile = true;

		if (msg.closeId != 0)
		{
			SCOPED_FUTEX(m_activeFilesLock, lock);
			m_activeFiles.erase(msg.closeId);
		}

		{
			SCOPED_FUTEX(m_outputFilesLock, lock);
			m_outputFiles.erase(msg.fileName.data);
		}

		{
			auto& shared = msg.process.m_shared;
			SCOPED_FUTEX(shared.writtenFilesLock, lock);
			auto findIt = shared.writtenFiles.find(msg.fileNameKey);
			if (findIt != shared.writtenFiles.end())
			{
				WrittenFile& writtenFile = findIt->second;
				if (writtenFile.mappingHandle.IsValid())
				{
					CloseFileMapping(m_logger, writtenFile.mappingHandle, writtenFile.name.c_str());
					deleteRealFile = false;
				}
				shared.writtenFiles.erase(findIt);
			}
		}

		RemoveWrittenFile(msg.process, msg.fileNameKey);

		if (deleteRealFile)
			out.result = uba::DeleteFileW(msg.fileName.data);
		out.errorCode = GetLastError();
		out.directoryTableSize = RegisterDeleteFile(msg.fileNameKey, msg.fileName);
		return true;
	}

	bool Session::CopyFile(CopyFileResponse& out, const CopyFileMessage& msg)
	{
		out.fromName.Append(msg.fromName);
		out.toName.Append(msg.toName);

		UBA_ASSERT(msg.toKey != StringKeyZero);
		SCOPED_FUTEX(m_activeFilesLock, lock);
		u32 closeId = m_wantsOnCloseIdCounter++;
		if (!m_activeFiles.try_emplace(closeId, ActiveFile{ msg.toName.data, msg.toKey }).second)
		{
			m_logger.Error(TC("SHOULD NOT HAPPEN"));
		}
		out.closeId = closeId;
		return true;
	}

	bool Session::MoveFile(MoveFileResponse& out, const MoveFileMessage& msg)
	{
		auto& process = msg.process;
		bool isMoved = false;
		{
			auto& fs = process.m_shared;
			SCOPED_FUTEX(fs.writtenFilesLock, writtenLock);
			auto findIt = fs.writtenFiles.find(msg.fromKey);
			if (findIt != fs.writtenFiles.end())
			{
				auto& oldFile = findIt->second;
				bool isMapping = oldFile.mappingHandle.IsValid();
				if (!isMapping)
				{
					out.result = MoveFileExW(msg.fromName.data, msg.toName.data, msg.flags);
					if (!out.result)
					{
						out.errorCode = GetLastError();
						return true;
					}
					isMoved = true;
				}

				UBA_ASSERT(msg.toKey != StringKeyZero);
				auto insres = fs.writtenFiles.try_emplace(msg.toKey);
				UBA_ASSERT(insres.second);
				WrittenFile& newFile = insres.first->second;
				newFile = oldFile;
				newFile.key = msg.toKey;
				newFile.name = msg.toName.data;
				fs.writtenFiles.erase(findIt);

				if (isMapping)
				{
					out.errorCode = ERROR_SUCCESS;
					out.result = true;
					return true;
				}
			}
		}

		if (!isMoved)
		{
			out.result = MoveFileExW(msg.fromName.data, msg.toName.data, msg.flags);
			if (!out.result)
			{
				out.errorCode = GetLastError();
				return true;
			}
		}

		if (!process.GetStartInfo().rules->IsInvisible(msg.toName))
			if (RegisterCreateFileForWrite(msg.toKey, msg.toName, true))
				TraceWrittenFile(process.m_id, msg.toName, 0);

		out.errorCode = ERROR_SUCCESS;
		out.directoryTableSize = RegisterDeleteFile(msg.fromKey, msg.fromName);
		return true;
	}

	bool Session::Chmod(ChmodResponse& out, const ChmodMessage& msg)
	{
		#if PLATFORM_WINDOWS
		UBA_ASSERT(false); // This is not used
		#else
		out.errorCode = 0;
		if (chmod(msg.fileName.data, (mode_t)msg.fileMode) == 0)
		{
			RegisterCreateFileForWrite(msg.fileNameKey, msg.fileName, true);
			return true;
		}
		out.errorCode = errno;
		#endif
		return true;
	}

	bool Session::CreateDirectory(CreateDirectoryResponse& out, const CreateDirectoryMessage& msg)
	{
		out.result = uba::CreateDirectoryW(msg.name.data);

		StringKey dirKey;
		const tchar* lastSlash;
		StringBuffer<> dirName;
		if (!GetDirKey(dirKey, dirName, lastSlash, msg.name))
			return true;

		if (!out.result)
			out.errorCode = GetLastError();

		// There is a chance that another thread just created the directory and we can't return directoryTableSize until we know it is written
		// So let's both success and already exists add entries
		if (out.result || out.errorCode == ERROR_ALREADY_EXISTS)
		{
			// Both these functions need to be called. otherwise we can get created directories that does not end up in directory table
			RegisterCreateFileForWrite(msg.nameKey, msg.name, true);
			WriteDirectoryEntries(dirKey, dirName);
		}

		out.directoryTableSize = GetDirectoryTableSize();
		return true;
	}

	bool Session::RemoveDirectory(RemoveDirectoryResponse& out, const RemoveDirectoryMessage& msg)
	{
		out.result = uba::RemoveDirectoryW(msg.name.data);
		if (out.result)
			RegisterDeleteFile(msg.nameKey, msg.name);
		else
			out.errorCode = GetLastError();
		// This has a race condition. If same directory is removed at the same time the failing one
		// might send back a directoryTableSize that does not include the delete
		out.directoryTableSize = GetDirectoryTableSize();
		return true;
	}

	bool Session::GetFullFileName(GetFullFileNameResponse& out, const GetFullFileNameMessage& msg)
	{
		UBA_ASSERTF(false, TC("SHOULD NOT HAPPEN (only remote).. %s"), msg.fileName.data);
		return false;
	}

	bool Session::GetLongPathName(GetLongPathNameResponse& out, const GetLongPathNameMessage& msg)
	{
		UBA_ASSERTF(false, TC("SHOULD NOT HAPPEN (only remote).. %s"), msg.fileName.data);
		return false;
	}

	bool Session::GetListDirectoryInfo(ListDirectoryResponse& out, const StringView& dirName, const StringKey& dirKey)
	{
		u32 tableOffset;
		u32 tableSize = WriteDirectoryEntries(dirKey, dirName, &tableOffset);
		out.tableOffset = tableOffset;
		out.tableSize = tableSize;
		return true;
	}

	bool Session::WriteFilesToDisk(ProcessImpl& process, WrittenFile** files, u32 fileCount)
	{
		if (!fileCount)
			return true;

		// This is to not kill I/O when writing lots of pdb/dlls in parallel
		#if PLATFORM_WINDOWS
		BottleneckScope scope(m_writeFilesBottleneck, Stats().waitBottleneck);
		#endif

		if (process.IsCancelled())
			return false;

		Atomic<bool> success = true;
		auto span = std::span(files, fileCount);
		m_workManager.ParallelFor(fileCount - 1, span, [&](const WorkContext&, auto& it)
			{
				KernelStatsScope ks(process.m_kernelStats);
				StorageStatsScope ss(process.m_storageStats);
				SessionStatsScope sessionStatsScope(process.m_sessionStats);
				if (!WriteFileToDisk(process, **it))
					success = false;
			}, TCV("WriteFilesToDisk"));
		return success;
	}

	bool Session::AllocFailed(Process& process, const tchar* allocType, u32 error)
	{
		m_logger.Warning(TC("Allocation failed in %s (%s).. process will sleep and try again"), allocType, LastErrorToText(error).data);
		return true;
	}

	bool Session::GetNextProcess(Process& process, bool& outNewProcess, NextProcessInfo& outNextProcess, u32 prevExitCode, BinaryReader& statsReader)
	{
		if (!m_getNextProcessFunction)
		{
			outNewProcess = false;
			return true;
		}

		outNewProcess = m_getNextProcessFunction(process, outNextProcess, prevExitCode);
		if (!outNewProcess)
			return true;

		m_trace.ProcessEnvironmentUpdated(process.GetId(), outNextProcess.description, statsReader.GetPositionData(), statsReader.GetLeft(), outNextProcess.breadcrumbs);

		return true;
	}

	bool Session::CustomMessage(Process& process, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 recvSize = reader.ReadU32();
		u32* sendSize = (u32*)writer.AllocWrite(4);
		void* sendData = writer.GetData() + writer.GetPosition();
		u32 written = 0;
		if (m_customServiceFunction)
			written = m_customServiceFunction(process, reader.GetPositionData(), recvSize, sendData, u32(writer.GetCapacityLeft()));
		*sendSize = written;
		writer.AllocWrite(written);
		return true;
	}

	bool Session::SHGetKnownFolderPath(Process& process, BinaryReader& reader, BinaryWriter& writer)
	{
		UBA_ASSERT(false); // Should only be called on UbaSessionClient
		return false;
	}

	bool Session::HostRun(BinaryReader& reader, BinaryWriter& writer)
	{
		#if !PLATFORM_WINDOWS

		Vector<TString> args;
		while (reader.GetLeft())
			args.push_back(reader.ReadString());
		bool success = false;

		StringBuffer<> command;
		for (auto& arg : args)
		{
			if (command.count)
				command.Append(' ');
			command.Append(arg);
		}

		char result[4096];
		if (FILE* fp = popen(command.data, "r"))
		{
			char* dest = result;
			errno = 0;
			while (true)
			{
				if (!fgets(dest, sizeof(result) - (dest - result), fp))
				{
					success = errno == 0;
					if (!success)
						snprintf(result, sizeof(result), "fgets failed with command: %s", command.data);
					break;
				}
				dest += strlen(dest);
			}
			pclose(fp);
		}
		else
			snprintf(result, sizeof(result), "popen failed with command: %s", command.data);
		writer.WriteBool(success);
		writer.WriteString(result);
		#endif
		return true;
	}

	bool Session::GetSymbols(const tchar* application, bool isArm, BinaryReader& reader, BinaryWriter& writer)
	{
		StringBuffer<256> detoursLibPath;
		StringBuffer<256> alternativeLibPath;

		detoursLibPath.Append(m_detoursLibrary[IsArmBinary].c_str()).Resize(detoursLibPath.Last(PathSeparator) - detoursLibPath.data);
		GetAlternativeUbaPath(m_logger, alternativeLibPath, detoursLibPath, IsArmBinary);

		StringView searchPaths[3] = { detoursLibPath, alternativeLibPath, {} };

		u64 size = reader.ReadU32();
		BinaryReader reader2(reader.GetPositionData(), 0, size);

		StringBuffer<16*1024> sb;
		ParseCallstackInfo(sb, reader2, application, searchPaths);
		writer.WriteString(sb);
		return true;
	}

	bool Session::CheckRemapping(ProcessImpl& process, BinaryReader& reader, BinaryWriter& writer)
	{
		StringBuffer<> fileName;
		reader.ReadString(fileName);
		StringKey fileNameKey = reader.ReadStringKey();
		UBA_ASSERT(fileNameKey != StringKeyZero);

		#if UBA_DEBUG_TRACK_MAPPING
		//m_debugLogger->Info(TC("Mapping check (%s)"), fileName.data);
		#endif

		MemoryMap out;
		u64 alignment = GetMemoryMapAlignment(fileName);
		if (!GetOrCreateMemoryMapFromFile(out, fileNameKey, fileName.data, false, alignment, TC("Remap"), &process, true))
			return m_logger.Error(TC("Failed to remap %s"), fileName.data);
		writer.WriteU32(GetFileMappingSize());
		return true;
	}

	bool Session::RunSpecialProgram(ProcessImpl& process, BinaryReader& reader, BinaryWriter& writer)
	{
		UBA_ASSERT(false);
		return true;
	}
	
	void Session::FileEntryAdded(StringKey fileNameKey, u64 lastWritten, u64 size)
	{
	}

	bool Session::FlushWrittenFiles(ProcessImpl& process)
	{
		return true;
	}

	bool Session::UpdateEnvironment(ProcessImpl& process, const StringView& reason, bool resetStats)
	{
		if (!resetStats)
			return true;
		UBA_ASSERT(!m_runningRemote); // local do not write session stats
		StackBinaryWriter<16 * 1024> writer;
		process.m_processStats.Write(writer);
		process.m_storageStats.Write(writer);
		process.m_kernelStats.Write(writer);
		m_trace.ProcessEnvironmentUpdated(process.GetId(), reason, writer.GetData(), writer.GetPosition(), ToView(process.GetStartInfo().breadcrumbs));
		process.m_processStats = {};
		process.m_storageStats = {};
		process.m_kernelStats = {};
		return true;
	}

	bool Session::LogLine(ProcessImpl& process, const tchar* line, LogEntryType logType)
	{
		return true;
	}

	void Session::PrintSessionStats(Logger& logger)
	{
		u64 mappingBufferSize;
		u32 mappingBufferCount;
		m_fileMappingBuffer.GetSizeAndCount(MappedView_Transient, mappingBufferSize, mappingBufferCount);
		logger.Info(TC("  DirectoryTable      %7u %9s"), u32(m_directoryTable.m_lookup.size()), BytesToText(GetDirectoryTableSize()).str);
		logger.Info(TC("  MappingTable        %7u %9s"), u32(m_fileMappingTableLookup.size()), BytesToText(GetFileMappingSize()).str);
		logger.Info(TC("  MappingBuffer       %7u %9s"), mappingBufferCount, BytesToText(mappingBufferSize).str);
		m_stats.Print(logger);
		logger.Info(TC(""));
	}

	bool Session::RegisterVirtualFileInternal(const StringKey& fileNameKey, const StringView& filePath, const tchar* sourceFile, u64 sourceOffset, u64 sourceSize)
	{
		TimerScope ts(Stats().createMmapFromFile);

		VirtualSourceFile virtualFile;
		StringKey sourceFileKey = CaseInsensitiveFs ? ToStringKeyLower(ToView(sourceFile)) : ToStringKey(ToView(sourceFile));
		SCOPED_FUTEX(m_virtualSourceFilesLock, virtualSourceFilesLock);
		auto insres = m_virtualSourceFiles.try_emplace(sourceFileKey); 
		if (insres.second)
		{
			FileHandle fileHandle = uba::CreateFileW(sourceFile, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, DefaultAttributes());
			if (fileHandle == InvalidFileHandle)
				return m_logger.Error(TC("[RegisterVirtualFileInternal] CreateFileW for %s failed (%s)"), sourceFile, LastErrorToText().data);
			u64 fileSize = 0;
			if (!GetFileSizeEx(fileSize, fileHandle))
				return m_logger.Error(TC("[RegisterVirtualFileInternal] GetFileSizeEx for %s failed (%s)"), sourceFile, LastErrorToText().data);
			auto fg = MakeGuard([&]() { uba::CloseFile(sourceFile, fileHandle); });
			virtualFile.mappingHandle = CreateFileMappingW(m_logger, fileHandle, PAGE_READONLY, fileSize, sourceFile);
			virtualFile.size = fileSize;
			insres.first->second = virtualFile;
		}
		else
			virtualFile = insres.first->second;
		if (!virtualFile.mappingHandle.IsValid())
			return m_logger.Error(TC("[RegisterVirtualFileInternal] CreateFileMapping for %s failed (%s)"), sourceFile, LastErrorToText().data);
		virtualSourceFilesLock.Leave();

		if (sourceSize + sourceOffset > virtualFile.size)
			return m_logger.Error(TC("Virtual file offset(%llu)+size(%llu) outside source file size(%llu)"), sourceOffset, sourceSize, virtualFile.size, filePath.data);

		return RegisterVirtualFileInternal(fileNameKey, filePath, virtualFile.mappingHandle, sourceSize, sourceOffset);
	}

	bool Session::CreateVirtualFileInternal(const StringKey& fileNameKey, const StringView& filePath, const void* memory, u64 memorySize)
	{
		TimerScope ts(Stats().createMmapFromFile);

		SCOPED_FUTEX(m_virtualSourceFilesLock, virtualSourceFilesLock);
		auto insres = m_virtualSourceFiles.try_emplace(fileNameKey); 
		if (!insres.second)
			return false;
		VirtualSourceFile& virtualFile = insres.first->second;
		FileMappingHandle mapping = CreateMemoryMappingW(m_logger, PAGE_READWRITE, memorySize, nullptr, filePath.data);
		if (!mapping.IsValid())
			return false;
		u8* mem2 = MapViewOfFile(m_logger, mapping, FILE_MAP_WRITE, 0, memorySize);
		if (!mem2)
			return false;
		MapMemoryCopy(mem2, memory, memorySize);
		UnmapViewOfFile(m_logger, mem2, memorySize, filePath.data);

		virtualFile.mappingHandle = mapping;
		virtualFile.size = memorySize;
		return RegisterVirtualFileInternal(fileNameKey, filePath, virtualFile.mappingHandle, memorySize, 0);
	}

	bool Session::RegisterVirtualFileInternal(const StringKey& fileNameKey, const StringView& filePath, FileMappingHandle mappingHandle, u64 mappingSize, u64 mappingOffset)
	{
		SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
		auto insres = m_fileMappingTableLookup.try_emplace(fileNameKey);
		FileMappingEntry& entry = insres.first->second;
		lookupLock.Leave();

		SCOPED_FUTEX(entry.lock, entryLock);
		
		if (entry.handled)
			return m_logger.Error(TC("Virtual file %s has already been registered"), filePath.data);

		entry.mapping = mappingHandle;
		entry.mappingOffset = mappingOffset;
		entry.contentSize = mappingSize;
		entry.handled = true;
		entry.lastWriteTime = 0; // TODO: Take lastwritetime of source file?
		entry.createIndependentMapping = true; // We want independent mapping in casdb so they can be deleted

		StringBuffer<> mappingName;
		Storage::GetMappingString(mappingName, mappingHandle, mappingOffset);
		entry.success = true;

		#if UBA_DEBUG_TRACK_MAPPING
		entry.name = filePath.data;
		m_debugLogger->Info(TC("Mapping created 0x%llx (%s) from virtual file"), u64(entry.mapping.mh), entry.name.c_str());
		#endif

		SCOPED_WRITE_LOCK(m_fileMappingTableMemLock, lock);
		BinaryWriter writer(m_fileMappingTableMem, m_fileMappingTableSize);
		writer.WriteStringKey(fileNameKey);
		writer.WriteString(mappingName);
		writer.Write7BitEncoded(mappingSize);
		m_fileMappingTableSize = (u32)writer.GetPosition();

		return true;
	}

	bool Session::GetOutputFileSizeInternal(u64& outSize, const StringKey& fileNameKey, StringView filePath)
	{
		SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
		auto findIt = m_fileMappingTableLookup.find(fileNameKey);
		if (findIt == m_fileMappingTableLookup.end())
			return false;
		FileMappingEntry& entry = findIt->second;
		lookupLock.Leave();
		SCOPED_FUTEX(entry.lock, entryCs);
		outSize = entry.contentSize;
		return true;
	}

	bool Session::GetOutputFileDataInternal(void* outData, const StringKey& fileNameKey, StringView filePath, bool deleteInternalMapping)
	{
		return GetFileMemory([&](const void* fileMem, u64 fileSize) { MapMemoryCopy(outData, fileMem, fileSize); return true; }, fileNameKey, filePath, deleteInternalMapping);
	}

	bool Session::WriteOutputFileInternal(const StringKey& fileNameKey, StringView filePath, bool deleteInternalMapping)
	{
		//m_logger.Info(TC("TRYING TO WRITE OUTPUT %s (%s)"), filePath, KeyToString(fileNameKey).data);
		FileAccessor destinationFile(m_logger, filePath.data);
		if (!GetFileMemory([&](const void* fileMem, u64 fileSize) { return WriteMemoryToDisk(destinationFile, fileMem, fileSize); }, fileNameKey, filePath, deleteInternalMapping))
			return false;
		if (!destinationFile.Close())
			return false;
		return true;
	}

	bool Session::CreateProcessJobObject()
	{
		#if PLATFORM_WINDOWS
		m_processJobObject = CreateJobObject(nullptr, nullptr);
		if (!m_processJobObject)
			return m_logger.Error(TC("Failed to create process job object"));
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = { };
		info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(m_processJobObject, JobObjectExtendedLimitInformation, &info, sizeof(info));
		#endif
		return true;
	}

	void Session::EnsureDirectoryTableMemory(u64 neededSize)
	{
		auto& dirTable = m_directoryTable;
		if (neededSize <= m_directoryTableMemCommitted)
			return;

		u64 newSize = AlignUp(neededSize, u64(1024*1024));
		if (newSize > DirTableMemSize)
		{
			static bool called = m_logger.Error(TC("Directory table overflow. DirTableMemSize need to be increased (Size: %llu)"), dirTable.m_memorySize);
		}

		u64 toCommit = newSize - m_directoryTableMemCommitted;
		u8* address = m_directoryTableMem + m_directoryTableMemCommitted;
		if (!MapViewCommit(address, toCommit))
			m_logger.Error(TC("Failed to commit memory for directory table (Committed: %llu, ToCommit: %llu) (%s)"), m_directoryTableMemCommitted, toCommit, LastErrorToText().data);
		m_directoryTableMemCommitted += toCommit;
	}

	void Session::GetSystemInfo(StringBufferBase& out)
	{
		u32 cpuCount = GetLogicalProcessorCount();
		u32 cpuGroupCount = GetProcessorGroupCount();

		StringBuffer<128> cpuStr(TC("CPU"));
		if (IsRunningArm())
			cpuStr.Append(TCV("[Arm]"));
		cpuStr.Append(':');
		if (cpuGroupCount != 1)
			cpuStr.AppendValue(cpuGroupCount).Append('x');
		cpuStr.AppendValue(cpuCount/cpuGroupCount);

		u64 totalMemoryInKilobytes = 0;

#if PLATFORM_WINDOWS
		GetPhysicallyInstalledSystemMemory(&totalMemoryInKilobytes);

		{
			u32 maxMHz = 0;
			DWORD valueSize = 4;
			const tchar* key = TC("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0");
			LSTATUS res = RegGetValueW(HKEY_LOCAL_MACHINE, key, TC("~MHz"), RRF_RT_REG_DWORD, NULL, &maxMHz, &valueSize);
			if (res != ERROR_SUCCESS)
			{
				// This will not always be the same and since we use the system info as part of key for client uniqueness it is annoying to get multiple sessions for same instance
				Vector<PROCESSOR_POWER_INFORMATION> procInfos;
				procInfos.resize(cpuCount);
				if (CallNtPowerInformation(ProcessorInformation, NULL, 0, procInfos.data(), cpuCount*sizeof(PROCESSOR_POWER_INFORMATION)) == STATUS_SUCCESS)
					maxMHz = procInfos[0].MaxMhz;
			}
			cpuStr.Appendf(TC(" @ %.1fGHz"), float(maxMHz) / 1000.0f);
		}

#else
		u64 throwAway;
		GetMemoryInfo(throwAway, totalMemoryInKilobytes);
		totalMemoryInKilobytes /= 1024;
		double processorMhz = 0.0;
	#if PLATFORM_LINUX
		if (FILE *fp = fopen("/proc/cpuinfo", "r"))
		{
			char line[256];
			while (processorMhz == 0.0 && fgets(line, sizeof(line), fp))
				if (strncmp(line, "cpu MHz", 7) == 0)
					sscanf(line, "cpu MHz : %lf", &processorMhz);
			fclose(fp);
			cpuStr.Appendf(" @ %.1fGHz", processorMhz/1000.0);
		}
	#else
		char brand[128];
		size_t size = sizeof(brand);
		if (sysctlbyname("machdep.cpu.brand_string", brand, &size, NULL, 0) == 0)
			cpuStr.Clear().Append(brand).Appendf(" CPU:%u", cpuCount);
	#endif
#endif

		const tchar* capacityStr = TC("NoLimit");
		u64 capacity = m_storage.GetStorageCapacity();
		BytesToText temp(capacity);
		if (capacity)
			capacityStr = temp.str;
		out.Appendf(TC("%s Mem:%ugb Cas:%s/%s"), cpuStr.data, u32(totalMemoryInKilobytes/(1024*1024)), BytesToText(m_storage.GetStorageUsed()).str, capacityStr);

		StringBuffer<128> zone;
		if (m_storage.GetZone(zone))
			out.Append(TCV(" Zone:")).Append(zone);

		#if PLATFORM_WINDOWS
		if (!IsRunningWine())
		{
			DWORD value = 0;
			DWORD valueSize = 4;
			const tchar* fsKey = TC("SYSTEM\\CurrentControlSet\\Control\\FileSystem");
			LSTATUS res = RegGetValueW(HKEY_LOCAL_MACHINE, fsKey, TC("NtfsDisableLastAccessUpdate"), RRF_RT_REG_DWORD, NULL, &value, &valueSize);
			if (res != ERROR_SUCCESS)
			{
				m_logger.Detail(TC("Failed to retreive ntfs registry key (%i)"), res);
			}
			else
			{
				u32 lastAccessSettingsValue = value & 0xf;
				if (lastAccessSettingsValue == 0 || lastAccessSettingsValue == 2)
					out.Append(TCV(" NtfsLastAccessEnabled"));
			}
			value = 0;
			res = RegGetValueW(HKEY_LOCAL_MACHINE, fsKey, TC("NtfsDisable8dot3NameCreation"), RRF_RT_REG_DWORD, NULL, &value, &valueSize);
			if (res == ERROR_SUCCESS)
				if (value == 0)
					out.Append(TCV(" NtfsShortNamesEnabled"));
		}
		else
		{
			StringBuffer<> testDir;
			testDir.Append(m_rootDir).Append(TCV("UbaTestShortNames"));
			::RemoveDirectory(testDir.data);
			wchar_t shortName[1024];
			if (::CreateDirectoryW(testDir.data, NULL))
				if (GetShortPathName(testDir.data, shortName, 1024) != 0 && !Contains(shortName, TC("UbaTestShortNames")))
					out.Append(TCV(" NtfsShortNamesEnabled"));
		}
		#endif

		if (!m_extraInfo.empty())
			out.Append(m_extraInfo);

		#if UBA_DEBUG
		out.Append(TCV(" - DEBUG"));
		#endif
	}

	bool Session::GetMemoryInfo(u64& outAvailable, u64& outTotal)
	{
#if PLATFORM_WINDOWS

		MEMORYSTATUSEX memStatus = { sizeof(memStatus) };
		if (!GlobalMemoryStatusEx(&memStatus))
		{
			outAvailable = 0;
			outTotal = 0;
			return m_logger.Error(TC("Failed to get global memory status (%s)"), LastErrorToText().data);
		}

		// Page file can grow and we want to use the absolute max size to figure out when we need to wait to start new processes
		if (m_maxPageSize == ~u64(0))
		{
			tchar systemDrive = 'c';
			{
				tchar temp[32];
				if (GetEnvironmentVariableW(TC("SystemDrive"), temp, 32))
					systemDrive = ToLower(temp[0]);
			}

			m_maxPageSize = 0;
			wchar_t str[1024];
			DWORD strBytes = sizeof(str);
			LSTATUS res = RegGetValueW(HKEY_LOCAL_MACHINE, TC("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management"), TC("PagingFiles"), RRF_RT_REG_MULTI_SZ, NULL, str, &strBytes);
			if (res == ERROR_SUCCESS)
			{
				DWORD pagefileOnOsVolume = 0;
				DWORD pagefileOnOsVolumeSize = 4;
				RegGetValueW(HKEY_LOCAL_MACHINE, TC("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management"), TC("PagefileOnOsVolume"), RRF_RT_DWORD, NULL, &pagefileOnOsVolume, &pagefileOnOsVolumeSize);

				wchar_t* line = str;
				for (; size_t lineLen = wcslen(line); line += lineLen + 1)
				{
					if (lineLen < 3)
						continue;

					u64 maxSizeMb = 0;

					StringBuffer<8> drive;
					drive.Append(line, 3); // Get drive root path

					if (drive[0] == '?') // Drive '?' can exist when "Automatically manage paging file size for all drives".. 
					{
						// We can use ExistingPageFiles registry key to figure out which drive...
						// This key can contain multiple page files normally.. have no idea if it can contain multiple when drive is '?'.. but for now, just use the first
						wchar_t str2[1024];
						DWORD str2Bytes = sizeof(str2);
						res = RegGetValueW(HKEY_LOCAL_MACHINE, TC("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management"), TC("ExistingPageFiles"), RRF_RT_REG_MULTI_SZ, NULL, str2, &str2Bytes);
						if (res != ERROR_SUCCESS)
							continue;

						auto colon = wcschr(str2, ':'); // Path is something like \??\C:\pagefile.sys or similar.. let's search for : and use character in front of it.
						if (colon == nullptr || colon == str2)
							continue;

						drive[0] = colon[-1];

						if (pagefileOnOsVolume && ToLower(drive[0]) != systemDrive)
							continue;
					}
					else if (!pagefileOnOsVolume || ToLower(drive[0]) == systemDrive)
					{
						const wchar_t* maxSizeStr = wcsrchr(line, ' ');
						
						if (!maxSizeStr || !StringBuffer<32>(maxSizeStr + 1).Parse(maxSizeMb))
						{
							m_logger.Warning(TC("Unrecognized page file information format (please report): %s"), line);
							continue;
						}

						if (maxSizeMb) // Custom set page file size
						{
							m_maxPageSize += maxSizeMb * 1024 * 1024;
							continue;
						}
					}
					else
					{
						m_logger.Warning(TC("Page file is set on drive %c: but registry key value PagefileOnOsVolume is set to 1. Fix registry"), drive[0]);
					}

					// Max possible system-managed page file
					maxSizeMb = Max(u64(memStatus.ullTotalPhys) * 3, 4ull * 1024 * 1024 * 1024);

					// Check if disk is limiting factor of system-managed page file
					// Page file can be max 1/8 of volume size and ofc not more than free space
					ULARGE_INTEGER totalNumberOfBytes;
					ULARGE_INTEGER totalNumberOfFreeBytes;
					if (!GetDiskFreeSpaceExW(drive.data, NULL, &totalNumberOfBytes, &totalNumberOfFreeBytes))
						return m_logger.Error(TC("GetDiskFreeSpaceExW failed to get information about %s (%s)"), drive.data, LastErrorToText().data);

					u64 maxDiskPageFileSize = Min(totalNumberOfBytes.QuadPart / 8, totalNumberOfFreeBytes.QuadPart);
					m_maxPageSize += Min(maxDiskPageFileSize, maxSizeMb);
				}
			}
		}

		u64 currentPageSize = memStatus.ullTotalPageFile - memStatus.ullTotalPhys;
		if (currentPageSize < m_maxPageSize)
		{
			outTotal = memStatus.ullTotalPhys + m_maxPageSize;
			outAvailable = memStatus.ullAvailPageFile + (m_maxPageSize - currentPageSize);
		}
		else
		{
			outTotal = memStatus.ullTotalPageFile;
			outAvailable = memStatus.ullAvailPageFile;
		}
#else
		u64 memKb;
		GetPhysicallyInstalledSystemMemory(memKb);
		outTotal = memKb*1024*1024;
		outAvailable = outTotal;
#endif
		return true;
	}

	void Session::WriteSummary(BinaryWriter& writer, const Function<void(Logger& logger)>& summaryFunc)
	{
		struct SummaryLogWriter : public LogWriter
		{
			SummaryLogWriter(BinaryWriter& w) : writer(w) {}
			virtual void BeginScope() override {}
			virtual void EndScope() override {}
			virtual void Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen) override
			{
				writer.WriteString(str, strLen);
				++count;
			}

			BinaryWriter& writer;
			u32 count = 0;
		};
		u32* lineCount = (u32*)writer.AllocWrite(4);

		SummaryLogWriter logWriter(writer);
		LoggerWithWriter logger(logWriter, TC(""));

		summaryFunc(logger);

		*lineCount = logWriter.count;
	}

	bool GetCpuTime(u64& outTotalTime, u64& outIdleTime)
	{
		outTotalTime = 0;
		outIdleTime = 0;
#if PLATFORM_WINDOWS

		GROUP_AFFINITY originalAffinity {};
		GROUP_AFFINITY newAffinity{};
		static u16 groupCount = GetActiveProcessorGroupCount();
		if (groupCount <= 1)
		{
			u64 idleTime, kernelTime, userTime;
			if (!GetSystemTimes((FILETIME*)&idleTime, (FILETIME*)&kernelTime, (FILETIME*)&userTime))
				return false;
			outIdleTime += idleTime;
			outTotalTime += kernelTime + userTime;
		}
		else
		{
			for (u16 group=0; group!=groupCount; ++group)
			{
				newAffinity.Mask = ~0ull;
				newAffinity.Group = group;
				if (!SetThreadGroupAffinity(GetCurrentThread(), &newAffinity, group == 0 ? &originalAffinity : NULL))
					return false;
				u64 idleTime, kernelTime, userTime;
				if (!GetSystemTimes((FILETIME*)&idleTime, (FILETIME*)&kernelTime, (FILETIME*)&userTime))
					return false;
				outIdleTime += idleTime;
				outTotalTime += kernelTime + userTime;
			}
			if (!SetThreadGroupAffinity(GetCurrentThread(), &originalAffinity, nullptr))
				return false;
		}

#elif PLATFORM_LINUX
		int fd = open("/proc/stat", O_RDONLY | O_CLOEXEC);
		if (fd == -1)
			return false;
		char buffer[512];
		int size = read(fd, buffer, sizeof_array(buffer) - 1);
		close(fd);
		if (size == -1)
			return false;
		buffer[size] = 0;
		char* endl = strchr(buffer, '\n');
		if (!endl)
			return false;
		u64 values[16] = { 0 };
		u32 valueCount = 0;
		*endl = 0;
		char* parsePos = buffer;

		// cpu
		parsePos = strchr(parsePos, ' ');
		if (!parsePos)
			return false;

		while (true)
		{
			// remove spaces
			while (*parsePos && (*parsePos < '0' || *parsePos > '9'))
				++parsePos;
			if (!*parsePos)
				break;
			char* numberStart = parsePos;
			while (*parsePos && *parsePos >= '0' && *parsePos <= '9')
				++parsePos;
			bool isLast = *parsePos == 0;
			*parsePos = 0;
			++parsePos;
			values[valueCount++] = strtoull(numberStart, nullptr, 10);
			if (isLast)
				break;
		}

		// user: normal processes executing in user mode
		// nice: niced processes executing in user mode
		// system: processes executing in kernel mode
		// idle: twiddling thumbs
		// iowait: waiting for I/O to complete
		// irq: servicing interrupts
		// softirq: servicing softirqs
		// steal
		if (valueCount <= 6)
			return false;
		u64 work = values[0] + values[1] + values[2];
		outIdleTime = values[3] + values[4] + values[5] + values[6] + values[7];
		outTotalTime = work + outIdleTime;

#else // PLATFORM_MAC
		mach_msg_type_number_t  CpuMsgCount = 0;
		processor_flavor_t CpuInfoType = PROCESSOR_CPU_LOAD_INFO;;
		natural_t CpuCount = 0;
		processor_cpu_load_info_t CpuData;
		host_t host = mach_host_self();
		u64 work = 0;
		int res = host_processor_info(host, CpuInfoType, &CpuCount, (processor_info_array_t *)&CpuData, &CpuMsgCount);
		if(res != KERN_SUCCESS)
			return false;//m_logger.Error(TC("Kernel error: %s"), mach_error_string(res));
		for(int i = 0; i < (int)CpuCount; i++)
		{
			work += CpuData[i].cpu_ticks[CPU_STATE_SYSTEM];
			work += CpuData[i].cpu_ticks[CPU_STATE_USER];
			work += CpuData[i].cpu_ticks[CPU_STATE_NICE];
			outIdleTime += CpuData[i].cpu_ticks[CPU_STATE_IDLE];
		}
		outTotalTime = work + outIdleTime;
#endif
		return true;
	}

	float Session::UpdateCpuLoad()
	{
		u64 totalTime = 0;
		u64 idleTime = 0;
		if (!GetCpuTime(totalTime, idleTime))
			return m_cpuLoad;

		u64 totalTimeSinceLastTime = totalTime - m_previousTotalCpuTime;
		u64 idleTimeSinceLastTime = idleTime - m_previousIdleCpuTime;

		float cpuLoad = 1.0f - ((totalTimeSinceLastTime > 0) ? (float(idleTimeSinceLastTime) / float(totalTimeSinceLastTime)) : 0);

		m_previousTotalCpuTime = totalTime;
		m_previousIdleCpuTime = idleTime;

		// TODO: This is the wrong solution.. but can't repro the bad values some people get
		if (cpuLoad >= 0 && cpuLoad <= 1.0f)
			m_cpuLoad = cpuLoad;

		return m_cpuLoad;
	}

	bool Session::HasVfs(RootsHandle handle) const
	{
		return (handle & 1ull) == 1ull;
	}

	RootsHandle Session::WithVfs(RootsHandle handle, bool vfs) const
	{
		return vfs ? (handle | 1ull) : (handle & ~1ull);
	}

	const Session::RootsEntry* Session::GetRootsEntry(RootsHandle rootsHandle)
	{
		SCOPED_FUTEX_READ(m_rootsLookupLock, rootsLock);
		auto findIt = m_rootsLookup.find(WithVfs(rootsHandle, false));
		if (findIt == m_rootsLookup.end())
			return m_logger.Error(TC("Can't find entry from roots handle %llu"), rootsHandle) ? nullptr : nullptr;

		RootsEntry& entry = findIt->second;
		rootsLock.Leave();

		UBA_ASSERT(entry.handled);
		return &entry;
	}

	void Session::PopulateRootsEntry(RootsEntry& entry, const void* rootsData, uba::u64 rootsDataSize)
	{
		entry.memory.resize(rootsDataSize);
		memcpy(entry.memory.data(), rootsData, rootsDataSize);

		BinaryReader reader((const u8*)rootsData, 0, rootsDataSize);
		while (reader.GetLeft())
		{
			u8 id = reader.ReadByte();(void)id; // Root id.. ignore for conversion from vfs to local
			StringBuffer<> temp;
			reader.ReadString(temp);
			if (!temp.count) // If vfs is not set it means that vfs should not be used (the roots entry memory might be used for cacheclient though)
				break;
			entry.roots.RegisterRoot(m_logger, temp.data);
			entry.vfs.emplace_back(temp.data, temp.count);
			#if PLATFORM_WINDOWS
			Replace((tchar*)entry.vfs.data(), '/', '\\');
			#endif
			reader.ReadString(temp.Clear());
			entry.locals.emplace_back(temp.data, temp.count);
		}
	}

	bool Session::ExtractSymbolsFromObjectFile(const CloseFileMessage& msg, const tchar* fileName, u64 fileSize)
	{
		if (!msg.mappingHandle.IsValid())
			return m_logger.Error(TC("Can't extract symbols from obj file that is written directly to disk (%s writing %s)"), msg.process.m_startInfo.application, fileName);

		FileMappingHandle objectFileMappingHandle;

		if (!DuplicateFileMapping(m_logger, msg.process.m_nativeProcessHandle, msg.mappingHandle, GetCurrentProcessHandle(), objectFileMappingHandle, FILE_MAP_ALL_ACCESS, false, 0, fileName))
			return m_logger.Error(TC("Failed to duplicate file mapping handle for %s"), fileName);
		auto ofmh = MakeGuard([&]() { CloseFileMapping(m_logger, objectFileMappingHandle, fileName); });

		u8* mem = MapViewOfFile(m_logger, objectFileMappingHandle, FILE_MAP_ALL_ACCESS, 0, fileSize);
		if (!mem)
			return m_logger.Error(TC("Failed to map view of filehandle for read %s (%s)"), fileName, LastErrorToText().data);
		auto memClose = MakeGuard([&](){ UnmapViewOfFile(m_logger, mem, fileSize, fileName); });

		ObjectFile* objectFile = ObjectFile::Parse(m_logger, ObjectFileParseMode_All, mem, fileSize, fileName);
		if (!objectFile)
			return false;
		auto ofg = MakeGuard([&]() { delete objectFile; });

		const tchar* lastDot = TStrrchr(fileName, '.');
		UBA_ASSERT(lastDot);
		StringBuffer<> exportsFile;
		exportsFile.Append(fileName, lastDot - fileName).Append(TCV(".exi"));

		bool verbose = false;
		#if UBA_DEBUG
		verbose = true;
		#endif
		MemoryBlock memoryBlock(32*1024*1024);
		if (!objectFile->WriteImportsAndExports(m_logger, memoryBlock, verbose))
			return false;

		FileMappingHandle symHandle = CreateMemoryMappingW(m_logger, PAGE_READWRITE, memoryBlock.writtenSize, nullptr, TC("SymHandle"));
		if (!symHandle.IsValid())
			return false;
		auto mg = MakeGuard([&]() { CloseFileMapping(m_logger, symHandle, TC("SymHandle")); });
		u8* mem2 = MapViewOfFile(m_logger, symHandle, FILE_MAP_ALL_ACCESS, 0, memoryBlock.writtenSize);
		if (!mem2)
			return false;

		MapMemoryCopy(mem2, memoryBlock.memory, memoryBlock.writtenSize);
		UnmapViewOfFile(m_logger, mem2, memoryBlock.writtenSize, TC("SymHandle"));

		StringKey symFileKey = CaseInsensitiveFs ? ToStringKeyLower(exportsFile) : ToStringKey(exportsFile);
		u64 lastWriteTime = GetSystemTimeAsFileTime();

		if (!RegisterCreateFileForWrite(symFileKey, exportsFile, false, memoryBlock.writtenSize, lastWriteTime))
			return false;

		mg.Cancel();

		auto insres = msg.process.m_shared.writtenFiles.try_emplace(symFileKey);
		WrittenFile& writtenFile = insres.first->second;

		UBA_ASSERT(writtenFile.owner == nullptr || writtenFile.owner == &msg.process);
		writtenFile.key = symFileKey;
		writtenFile.owner = &msg.process;
		writtenFile.attributes = msg.attributes;
		writtenFile.mappingHandle = symHandle;
		writtenFile.mappingWritten = memoryBlock.writtenSize;
		writtenFile.lastWriteTime = lastWriteTime;
		writtenFile.name = exportsFile.data;

		return true;
	}

	bool Session::DevirtualizeDepsFile(RootsHandle rootsHandle, MemoryBlock& destData, const void* sourceData, u64 sourceSize, bool escapeSpaces, const tchar* hint)
	{
		auto rootsEntryPtr = GetRootsEntry(rootsHandle);
		if (!rootsEntryPtr)
			return false;
		const RootsEntry& rootsEntry = *rootsEntryPtr;

		UBA_ASSERT(!rootsEntry.locals.empty());

		Vector<std::string> localsAnsi;
		localsAnsi.reserve(rootsEntry.locals.size());

		for (auto& str : rootsEntry.locals)
		{
			char ansi[512];
			u32 ansiPos = 0;
			for (auto c : str)
			{
				UBA_ASSERT(c < 256);
				if (escapeSpaces)
				{
					if (c == ' ')
						ansi[ansiPos++] = '\\';
				}
				else
				{
					if (c == '\\')
						ansi[ansiPos++] = '\\';
				}
				ansi[ansiPos++] = (char)c;
			}
			ansi[ansiPos] = 0;
			localsAnsi.emplace_back(ansi, ansi + ansiPos);
		}

		auto handleString = [&](const char* str, u64 strLen, u32 rootPos)
			{
				if (rootPos == ~0u)
				{
					memcpy(destData.Allocate(strLen, 1, TC("")), str, strLen);
					return;
				}
				auto& path = localsAnsi[(*str - RootPaths::RootStartByte)/PathsPerRoot];
				memcpy(destData.Allocate(path.size(), 1, TC("")), path.c_str(), path.size());
			};

		return rootsEntry.roots.NormalizeString<char>(m_logger, (const char*)sourceData, sourceSize, handleString, true, hint);
	}

	void Session::ThreadTraceLoop()
	{
		while (true)
		{
			TraceSessionUpdate();
			if (m_traceThreadEvent.IsSet(500))
				break;
		}
	}

	void Session::TraceWrittenFile(u32 processId, const StringView& file, u64 size)
	{
		if (!m_traceWrittenFiles)
			return;
		StringBuffer<> str(TC("WrittenFile: "));
		str.Append(file);
		if (!size)
		{
			size = InvalidValue;
			FileBasicInformation info;
			if (!GetFileBasicInformation(info, m_logger, file.data, false))
				str.Append(TCV(" (GetFileBasicInformation failed)")).Append(BytesToText(size).str).Append(')');
			else
				size = info.size;
		}
		if (size != InvalidValue)
			str.Append(TCV( " (size: ")).Append(BytesToText(size).str).Append(')');
		m_trace.ProcessAddBreadcrumbs(processId, str, false);
	}

	void Session::TraceSessionUpdate()
	{
	}

	void Session::RunDependencyCrawler(ProcessImpl& process)
	{
		auto& startInfo = process.GetStartInfo();

		auto crawlerType = startInfo.rules->GetDependencyCrawlerType();
		if (crawlerType == DependencyCrawlerType_None)
			return;

		const tchar* at = TStrchr(startInfo.arguments, '@');
		if (!at)
			return;

		auto CreateFileFunc = [this, ph = ProcessHandle(&process), rules = startInfo.rules](TrackWorkScope& tracker, const StringView& fileName, const DependencyCrawler::AccessFileFunc& func)
			{
				auto& process = *static_cast<ProcessImpl*>(ph.m_process);
				if (process.IsCancelled())
					return false;

				CreateFileResponse out;
				{
					tracker.AddHint(fileName);

					if (!CreateFileForRead(out, tracker, fileName, ToStringKey(fileName), process, *rules))
						return false;
				}

				if (!func)
					return true;

				if (out.fileName[0] == '^')
				{
					MappedView view = m_fileMappingBuffer.MapView(out.fileName, out.size, fileName.data);
					if (view.memory)
					{
						bool res = func(view.memory, out.size);
						m_fileMappingBuffer.UnmapView(view, fileName.data);
						return res;
					}
					return m_logger.Warning(TC("Failed to open %s"), out.fileName.data);
				}

				if (out.fileName.Equals(TCV("$d")))
				{
					// This can happen on apple targets.. crawler finds some includes that are not proper includes
					return m_logger.Warning(TC("Trying to open directory %s as file"), fileName.data);
				}

				if (out.fileName.Equals(TCV("#")))
					out.fileName.Clear().Append(fileName);

				FileAccessor fa(m_logger, out.fileName.data);
				if (fa.OpenMemoryRead(0, false))
					return func(fa.GetData(), fa.GetSize());
				else
					return m_logger.Warning(TC("Failed to open %s"), out.fileName.data);
			};

		auto DevirtualizePathFunc = [this, rootsHandle = startInfo.rootsHandle](StringBufferBase& inOut) { return DevirtualizePath(inOut, rootsHandle, false); };

		m_dependencyCrawler.Add(at + 1, startInfo.workingDir, CreateFileFunc, DevirtualizePathFunc, startInfo.application, crawlerType, startInfo.rules->index);
	}

	void GenerateNameForProcess(StringBufferBase& out, const tchar* arguments, u32 counterSuffix)
	{
		const tchar* start = arguments;
		const tchar* it = arguments;
		StringBuffer<> temp;
		while (true)
		{
			if (*it != ' ' && *it != 0)
			{
				++it;
				continue;
			}
			temp.Clear();
			temp.Append(start, u64(it - start));
			if (!temp.Contains(TC(".rsp")) && !temp.Contains(TC(".bat")))
			{
				if (*it == 0)
					break;
				++it;
				start = it;
				continue;
			}
			out.AppendFileName(temp.data);
			if (out.data[out.count -1] == '"')
				out.Resize(out.count -1);
			break;
		}

		if (out.IsEmpty())
			out.Append(TCV("NoGoodName"));

		if (counterSuffix)
			out.Appendf(TC("_%03u"), counterSuffix);
	}

	bool GetZone(StringBufferBase& outZone)
	{
		outZone.count = GetEnvironmentVariableW(TC("UBA_ZONE"), outZone.data, outZone.capacity);
		if (outZone.count)
			return true;

		// TODO: Remove.
		#if PLATFORM_MAC
		if (!GetComputerNameW(outZone))
			return false;

		if (outZone.StartsWith(TC("dc4-mac")) || outZone.StartsWith(TC("rdu-mac")))
		{
			outZone.Resize(7);
			return true;
		}
		outZone.count = 0;
		#endif

		return false;
	}
}
