// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaFileMapping.h"
#include "UbaBottleneck.h"
#include "UbaDirectoryIterator.h"
#include "UbaFile.h"
#include "UbaPlatform.h"
#include "UbaProcessStats.h"
#include "UbaTimer.h"
#include "UbaWorkManager.h"

#if !PLATFORM_WINDOWS
#include <sys/file.h>
#endif

#define UBA_DEBUG_FILE_MAPPING 0

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace uba
{
	#if UBA_DEBUG_FILE_MAPPING
	Futex g_fileMappingsLock;
	struct DebugFileMapping { TString hint; Atomic<u64> viewCount; };
	UnorderedMap<HANDLE, DebugFileMapping> g_fileMappings;
	UnorderedMap<const void*, HANDLE> g_viewMappings;
	#endif


#if PLATFORM_WINDOWS
	Bottleneck& g_createFileHandleBottleneck = *new Bottleneck(8); // Allocated and leaked just to prevent shutdown asserts in debug

	HANDLE asHANDLE(FileHandle fh);

	HANDLE InternalCreateFileMappingW(Logger& logger, HANDLE hFile, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName, const tchar* hint)
	{
		HANDLE res;
		if (flProtect == PAGE_READWRITE)
		{
			// Experiment to try to prevent lock happening on AWS servers when lots of helpers are sending back obj files.
			Timer timer;
			BottleneckScope bs(g_createFileHandleBottleneck, timer);
			res = ::CreateFileMappingW(hFile, NULL, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
		}
		else
			res = ::CreateFileMappingW(hFile, NULL, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);

		#if UBA_DEBUG_FILE_MAPPING
		if (res)
		{
			SCOPED_FUTEX(g_fileMappingsLock, lock);
			auto insres = g_fileMappings.try_emplace(res, hint);(void)insres;
			UBA_ASSERT(insres.second);
			//logger.Info(TC("FILEMAPPING CREAT: %s %llu (%llu)"), hint, u64(res), g_fileMappings.size());
		}
		#endif

		return res;
	}
#else
	int asFileDescriptor(FileHandle fh);

	Futex g_mappingUidCounterLock;
	Atomic<u64> g_mappingUidCounter;
#endif

	FileMappingHandle CreateMemoryMappingW(Logger& logger, u32 flProtect, u64 maxSize, const tchar* name, const tchar* hint)
	{
		ExtendedTimerScope ts(KernelStats::GetCurrent().createFileMapping);
#if PLATFORM_WINDOWS
		return { 0, InternalCreateFileMappingW(logger, INVALID_HANDLE_VALUE, flProtect, (DWORD)ToHigh(maxSize), ToLow(maxSize), name, hint) };
#else
		UBA_ASSERT(!name);
		UBA_ASSERT((flProtect & (~u32(PAGE_READWRITE | SEC_RESERVE))) == 0);

		// Since we need to not leak file mappings we use files as a trick to know which ones are used and not
		StringBuffer<64> lockDir;
		lockDir.Append("/tmp/uba_shm_locks");

		SCOPED_FUTEX(g_mappingUidCounterLock, lock);
		if (!g_mappingUidCounter)
		{
			// Create dir
			if (mkdir(lockDir.data, 0777) == -1)
				if (errno != EEXIST)
				{
					logger.Error(TC("Failed to create %s for memory mapping (%s)"), lockDir.data, strerror(errno));
					return {};
				}

			// Clear out all orphaned shm_open
			TraverseDir(logger, lockDir,
				[&](const DirectoryEntry& e)
				{
					u32 uid = strtoul(e.name, nullptr, 10);

					StringBuffer<128> lockFile;
					lockFile.Append(lockDir).EnsureEndsWithSlash().Append(e.name);
					int lockFd = open(lockFile.data, O_RDWR, S_IRUSR | S_IWUSR | O_CLOEXEC);
					if (lockFd == -1)
					{
						if (errno == EPERM)
						{
							g_mappingUidCounter = uid;
							return;
						}
						logger.Warning("Failed to open %s for memory mapping (%s)", lockFile.data, strerror(errno));
						UBA_ASSERTF(false, "Failed to open %s (%s)", lockFile.data, strerror(errno));
						return;
					}

					if (flock(lockFd, LOCK_EX | LOCK_NB) == 0)
					{
						StringBuffer<64> uidName;
						GetMappingHandleName(uidName, uid);
						if (shm_unlink(uidName.data) == 0)
							;// logger.Info("Removed old shared memory %s", uidName.data);
						remove(lockFile.data);
					}
					else
					{
						g_mappingUidCounter = uid;
					}
					close(lockFd);
				});

			if (g_mappingUidCounter)
				logger.Info("Starting shared memory files at %u", g_mappingUidCounter.load());
		}
		lock.Leave();

		// Let's find a free shm name
		StringBuffer<128> lockFile;
		u64 uid;
		int shmFd;
		int lockFd;

		int retryCount = 4;
		while (true)
		{
			uid = ++g_mappingUidCounter;

			lockFile.Clear().Append(lockDir).EnsureEndsWithSlash().AppendValue(uid);

			lockFd = open(lockFile.data, O_CREAT | O_RDWR | O_NOFOLLOW | O_EXCL, S_IRUSR | S_IWUSR | O_CLOEXEC);
			if (lockFd == -1)
			{
				if (errno == EEXIST)
					continue;
				logger.Warning(TC("Failed to open/create %s (%s)"), lockFile.data, strerror(errno));
				UBA_ASSERTF(false, "Failed to open/create %s (%s)", lockFile.data, strerror(errno));
				continue;
			}

			if (flock(lockFd, LOCK_EX | LOCK_NB) != 0) // Some other process is using this one
			{
				close(lockFd);
				continue;
			}

			StringBuffer<64> uidName;
			GetMappingHandleName(uidName, uid);

			int oflags = O_CREAT | O_RDWR | O_NOFOLLOW | O_EXCL;
			shmFd = shm_open(uidName.data, oflags, S_IRUSR | S_IWUSR);
			if (shmFd != -1)
				break;

			bool retry = retryCount-- > 0;

			int err = errno;
			auto logType = retry ? LogEntryType_Warning : LogEntryType_Error;
			logger.Logf(logType, TC("Failed to create shm %s after getting lock-file %s (%s)"), uidName.data, lockFile.data, strerror(err));

			if (retry)
				continue;

			remove(lockFile.data);
			close(lockFd);
			SetLastError(err);
			return {};
		}

		if (maxSize != 0)
		{
			if (ftruncate(shmFd, (s64)maxSize) == -1)
			{
				SetLastError(errno);
				close(shmFd);
				remove(lockFile.data);
				close(lockFd);
				//UBA_ASSERTF(false, "Failed to truncate file mapping '%s' to size %llu (%s)", name, maxSize, strerror(errno));
				return {};
			}
		}
		return { shmFd, lockFd, uid };
#endif
	}

	FileMappingHandle CreateFileMappingW(Logger& logger, FileHandle file, u32 protect, u64 maxSize, const tchar* hint)
	{
		ExtendedTimerScope ts(KernelStats::GetCurrent().createFileMapping);
#if PLATFORM_WINDOWS
		return { 0, InternalCreateFileMappingW(logger, asHANDLE(file), protect, (DWORD)ToHigh(maxSize), ToLow(maxSize), NULL, hint) };
#else
		FileMappingHandle h;
		int fd = asFileDescriptor(file);
		if (maxSize && (protect & (~PAGE_READONLY)) != 0)
		{
#if PLATFORM_MAC // For some reason lseek+write does not work on apple silicon platform
			if (ftruncate(fd, maxSize) == -1)
			{
				logger.Error("ftruncate to %llu on fd %i failed for %s: %s\n", maxSize, fd, hint, strerror(errno));
				return h;
			}
#else
			if (lseek(fd, maxSize - 1, SEEK_SET) != maxSize - 1)
			{
				logger.Error("lseek to %llu failed for %s: %s\n", maxSize - 1, hint, strerror(errno));
				return h;
			}

			errno = 0;
			int res = write(fd, "", 1);
			if (res != 1)
			{
				logger.Error("write one byte at %llu on fd %i (%s) failed (res: %i): %s\n", maxSize - 1, fd, hint, res, strerror(errno));
				return h;
			}
#endif
		}

		h.shmFd = fd;
		return h;
#endif
	}

	u8* MapViewOfFile(Logger& logger, FileMappingHandle fileMappingObject, u32 desiredAccess, u64 offset, u64 bytesToMap)
	{
		ExtendedTimerScope ts(KernelStats::GetCurrent().mapViewOfFile);
#if PLATFORM_WINDOWS
		u8* res = (u8*)::MapViewOfFile(fileMappingObject.mh, desiredAccess, (DWORD)ToHigh(offset), ToLow(offset), bytesToMap);
		if (!res)
			return nullptr;
#else
		int prot = 0;
		if (desiredAccess & FILE_MAP_READ)
			prot |= PROT_READ;
		if (desiredAccess & FILE_MAP_WRITE)
			prot |= PROT_WRITE;
		UBA_ASSERT(fileMappingObject.IsValid());
		int shmFd = fileMappingObject.shmFd;
		void* rptr = mmap(NULL, bytesToMap, prot, MAP_SHARED, shmFd, s64(offset));
		if (rptr == MAP_FAILED)
		{
			SetLastError(errno);
			return nullptr;
		}
		u8* res = (u8*)rptr;
		//UBA_ASSERTF(false, "Failed to map file with fd %i, desiredAccess %u offset %llu, bytesToMap %llu (%s)", fd, desiredAccess, offset, bytesToMap, strerror(errno));
#endif

		#if UBA_DEBUG_FILE_MAPPING
		{
			SCOPED_FUTEX(g_fileMappingsLock, lock);
			auto findIt = g_fileMappings.find(fileMappingObject.handle);
			UBA_ASSERTF(findIt != g_fileMappings.end(), TC("Mapping nonexisting handle: %llu"), u64(fileMappingObject.handle));
			auto insres = g_viewMappings.try_emplace(res, fileMappingObject.handle);
			UBA_ASSERT(insres.second);
			++findIt->second.viewCount;
		}
		#endif

		return res;
	}

	bool MapViewCommit(void* address, u64 size)
	{
#if PLATFORM_WINDOWS
		ExtendedTimerScope ts(KernelStats::GetCurrent().virtualAlloc);
		return ::VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE);
#else
		return true;
#endif
	}

	bool UnmapViewOfFile(Logger& logger, const void* lpBaseAddress, u64 bytesToUnmap, const tchar* hint)
	{
		if (!lpBaseAddress)
			return true;

		ExtendedTimerScope ts(KernelStats::GetCurrent().unmapViewOfFile);

		#if UBA_DEBUG_FILE_MAPPING
		{
			SCOPED_FUTEX(g_fileMappingsLock, lock);
			auto findIt = g_viewMappings.find(lpBaseAddress);
			UBA_ASSERT(findIt != g_viewMappings.end());
			auto findIt2 = g_fileMappings.find(findIt->second);
			g_viewMappings.erase(findIt);
			UBA_ASSERTF(findIt2 != g_fileMappings.end(), TC("Unmap nonexisting handle: %llu"), u64(findIt->second));
			--findIt2->second.viewCount;
		}
		#endif

#if PLATFORM_WINDOWS
		(void)bytesToUnmap;
		return ::UnmapViewOfFile(lpBaseAddress);
#else
		UBA_ASSERTF(bytesToUnmap, TC("bytesToUnmap is zero unmapping %p (%s)"), lpBaseAddress, hint);
		if (munmap((void*)lpBaseAddress, bytesToUnmap) == 0)
			return true;
		UBA_ASSERT(false);
		return false;
#endif
	}

	bool CloseFileMapping(Logger& logger, FileMappingHandle h, const tchar* hint)
	{
		if (!h.IsValid())
			return true;

		#if UBA_DEBUG_FILE_MAPPING
		SCOPED_FUTEX(g_fileMappingsLock, lock);
		auto findIt = g_fileMappings.find(h.handle);
		UBA_ASSERTF(findIt != g_fileMappings.end(), TC("Handle: %llu"), u64(h.handle));
		UBA_ASSERTF(!findIt->second.viewCount.load(), TC("Closing file mapping with %llu open views (%s)"), findIt->second.viewCount.load(), findIt->second.hint.c_str());
		//logger.Info(TC("FILEMAPPING CLOSE: %s %llu (%llu)"), findIt->second.hint.c_str(), u64(h.handle), g_fileMappings.size() - 1);
		g_fileMappings.erase(findIt);
		#endif

#if PLATFORM_WINDOWS
		if (h.fh)
			CloseHandle(h.fh);
		return CloseHandle(h.mh);
#else
		if (h.uid == ~u64(0))
			return true;
		if (close(h.shmFd) != 0)
			UBA_ASSERT(false);

		StringBuffer<64> uidName;
		GetMappingHandleName(uidName, h.uid);
		if (shm_unlink(uidName.data) != 0)
		{
			SetLastError(errno);
			UBA_ASSERTF(false, "Failed to unlink %s (%s)", uidName.data, strerror(errno));
			return false;
		}

		StringBuffer<128> lockFile;
		lockFile.Append("/tmp/uba_shm_locks").EnsureEndsWithSlash().AppendValue(h.uid);
		remove(lockFile.data);
		close(h.lockFd);
		return true;
#endif
	}

	bool DuplicateFileMapping(Logger& logger, ProcHandle sourceProcessHandle, FileMappingHandle source, ProcHandle targetProcessHandle, FileMappingHandle& targetHandle, u32 dwDesiredAccess, bool bInheritHandle, u32 dwOptions, const tchar* hint)
	{
#if PLATFORM_WINDOWS
		if (!DuplicateHandle((HANDLE)sourceProcessHandle, source.mh, (HANDLE)targetProcessHandle, &targetHandle.mh, dwDesiredAccess, bInheritHandle, dwOptions))
			return false;
		if (source.fh)
			if (!DuplicateHandle((HANDLE)sourceProcessHandle, source.fh, (HANDLE)targetProcessHandle, &targetHandle.fh, dwDesiredAccess, bInheritHandle, dwOptions))
				return false;
#else
		UBA_ASSERT(false);
		if (true)
			return false;
#endif

		#if UBA_DEBUG_FILE_MAPPING
		{
			SCOPED_FUTEX(g_fileMappingsLock, lock);
			auto insres = g_fileMappings.try_emplace(targetHandle.mh, hint);(void)insres;
			UBA_ASSERT(insres.second);
			//logger.Info(TC("FILEMAPPING DUPLI: %s %llu (%llu)"), hint, targetHandle.mh, g_fileMappings.size());
		}
		#endif

		return true;
	}

	void MapMemoryCopy(void* dest, const void* source, u64 size)
	{
		auto& stats = KernelStats::GetCurrent();
		ExtendedTimerScope ts(stats.memoryCopy);
		stats.memoryCopy.bytes += size;
		memcpy(dest, source, size);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////

	FileMappingBuffer::FileMappingBuffer(Logger& logger, WorkManager* workManager)
	:	m_logger(logger)
	,	m_workManager(workManager)
	{
		m_pageSize = 64*1024;
	}

	FileMappingBuffer::~FileMappingBuffer()
	{
		CloseMappingStorage(m_storage[MappedView_Transient]);
		CloseMappingStorage(m_storage[MappedView_Persistent]);
	}

	u64 FileMappingBuffer::GetFileMappingCapacity()
	{
		return (IsWindows ? 32ull : 8ull) * 1024 * 1024 * 1024; // Linux can't have larger than 8gb
	}

	bool FileMappingBuffer::AddTransient(const tchar* name, bool keepMapped)
	{
		MappingStorage& storage = m_storage[MappedView_Transient];
		File** prev = &storage.availableFile;
		for (u32 i = 0; i != sizeof_array(storage.files); ++i)
		{
			File& file = storage.files[i];
			file.name = name;
			file.keepMapped = keepMapped;
			*prev = &file;
			prev = &file.next;
		}
		storage.fileCount = sizeof_array(storage.files);
		return true;
	}

	bool FileMappingBuffer::AddPersistent(const tchar* name, FileHandle fileHandle, u64 size, u64 capacity)
	{
		FileMappingHandle sparseMemoryHandle = uba::CreateFileMappingW(m_logger, fileHandle, PAGE_READWRITE, capacity, name);
		if (!sparseMemoryHandle.IsValid())
		{
			m_logger.Error(TC("Failed to create file mapping (%s)"), LastErrorToText().data);
			return false;
		}

		MappingStorage& storage = m_storage[MappedView_Persistent];
		File& f = storage.files[storage.fileCount++];
		f.name = name;
		f.file = fileHandle;
		f.handle = sparseMemoryHandle;
		f.size = size;
		f.capacity = capacity;

		PushFile(storage, &f);
		return true;
	}

	void FileMappingBuffer::CloseDatabase()
	{
		CloseMappingStorage(m_storage[MappedView_Persistent]);
	}

	MappedView FileMappingBuffer::AllocAndMapView(FileMappingType type, u64 size, u64 alignment, const tchar* hint, bool allowShrink, bool isIndependent)
	{
		if (isIndependent)
		{
			UBA_ASSERT(!allowShrink);
			FileMappingHandle mapping = CreateMemoryMappingW(m_logger, PAGE_READWRITE, size, hint, TC("Independent"));
			if (!mapping.IsValid())
				return {};
			u8* memory = MapViewOfFile(m_logger, mapping, FILE_MAP_WRITE, 0, size);
			if (!memory)
			{
				CloseFileMapping(m_logger, mapping, hint);
				return {};
			}
			MappedView res;
			res.handle = mapping;
			res.memory = memory;
			res.size = size;
			res.isIndependent = true;
			return res;
		}

		MappingStorage& storage = m_storage[type];
		File* file = PopFile(storage, size, alignment);
		if (!file)
			return {};

		if (allowShrink)
			return AllocAndMapViewNoLock(*file, size, alignment, hint);

		MappedView res = AllocAndMapViewNoLock(*file, size, alignment, hint);

		PushFile(storage, file);
		return res;
	}

	MappedView FileMappingBuffer::AllocAndMapViewNoLock(File& f, u64 size, u64 alignment, const tchar* hint)
	{
		MappedView res;

		u64 offset = AlignUp(f.size, alignment);
		u64 alignedOffsetStart = AlignUp(offset - (m_pageSize - 1), m_pageSize);

		u64 newOffset = offset + size;
		u64 alignedOffsetEnd = AlignUp(newOffset, m_pageSize);
		
		if (alignedOffsetEnd > f.capacity)
		{
			m_logger.Error(TC("%s - AllocAndMapView has reached max capacity %llu trying to allocate %llu for %s"), f.name, f.capacity, size, hint);
			return res;
		}

		u64 mapSize = alignedOffsetEnd - alignedOffsetStart;
		u8* data;
		if (f.mappedMemory)
		{
			data = f.mappedMemory + alignedOffsetStart;
		}
		else
		{
			data = MapViewOfFile(m_logger, f.handle, FILE_MAP_WRITE, alignedOffsetStart, mapSize);
			if (!data)
			{
				m_logger.Error(TC("%s - AllocAndMapView failed to map view of file for %s with size %llu and offset %llu (%s)"), f.name, hint, size, alignment, LastErrorToText().data);
				return res;
			}
		}

		u64 committedBefore = AlignUp(offset, m_pageSize);
		u64 committedAfter = AlignUp(newOffset, m_pageSize);

		if (f.commitOnAlloc && committedBefore != committedAfter)
		{
			u64 commitStart = committedBefore - alignedOffsetStart;
			u64 commitSize = committedAfter - committedBefore;
			if (!MapViewCommit(data + commitStart, commitSize))
			{
				UnmapViewOfFile(m_logger, data, mapSize, hint);
				m_logger.Error(TC("%s - Failed to allocate/map %s for %s (%s)"), f.name, BytesToText(commitSize).str, hint, LastErrorToText().data);
				return res;
			}

			PrefetchVirtualMemory(data + commitStart, commitSize);
		}

		f.size = newOffset;
		//++f.activeMapCount;

		res.handle = f.handle;
		res.offset = offset;
		res.size = size;
		res.memory = data + (offset - alignedOffsetStart);
		return res;
	}

	FileMappingBuffer::File& FileMappingBuffer::GetFile(FileMappingHandle handle, u8& outStorageIndex)
	{
		for (u8 storageI = 0; storageI != 2; ++storageI)
		{
			MappingStorage& storage = m_storage[storageI];
			for (u32 i = 0; i != storage.fileCount; ++i)
				if (storage.files[i].handle == handle)
				{
					outStorageIndex = storageI;
					return storage.files[i];
				}
		}
		UBA_ASSERT(false);
		static FileMappingBuffer::File error;
		return error;
	}

	FileMappingBuffer::File* FileMappingBuffer::PopFile(MappingStorage& storage, u64 size, u64 alignment)
	{
		while (true)
		{
			SCOPED_FUTEX(storage.availableFilesLock, lock);
			File* file = storage.availableFile;
			if (!file)
			{
				if (storage.fullFileCount == storage.fileCount)
				{
					m_logger.Error(TC("All files are full!"));
					return nullptr;
				}
				lock.Leave();
				storage.availableFilesEvent.IsSet();
				continue;
			}
			storage.availableFile = file->next;
			lock.Leave();

			if (!file->handle.IsValid())
			{
				UBA_ASSERT(&storage == &m_storage[MappedView_Transient]);
				u64 capacity = GetFileMappingCapacity();
				file->handle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE | SEC_RESERVE, capacity, nullptr, TC("FileMappingBuffer"));
				if (!file->handle.IsValid())
				{
					m_logger.Error(TC("%s - Failed to create memory map (%s)"), file->name, LastErrorToText().data);
					return nullptr;
				}
				file->commitOnAlloc = true;
				file->capacity = capacity;

				if (file->keepMapped)
					file->mappedMemory = MapViewOfFile(m_logger, file->handle, FILE_MAP_WRITE, 0, capacity);

			}
			else
			{
				u64 newSize = AlignUp(file->size, alignment) + size;
				if (newSize > file->capacity)
				{
					lock.Enter();
					++storage.fullFileCount; // We will never add this file back to the available file list
					continue;
				}
			}
			return file;
		}
		return nullptr;
	}

	void FileMappingBuffer::PushFile(MappingStorage& storage, File* file)
	{
		SCOPED_FUTEX(storage.availableFilesLock, lock);
		file->next = storage.availableFile;
		storage.availableFile = file;
		storage.availableFilesEvent.Set();
	}

	void FileMappingBuffer::CloseMappingStorage(MappingStorage& storage)
	{
		SCOPED_FUTEX(storage.availableFilesLock, lock);
		u32 filesTaken = storage.fullFileCount;
		while (true)
		{
			while (storage.availableFile)
			{
				storage.availableFile = storage.availableFile->next;
				++filesTaken;
			}
			lock.Leave();
			if (filesTaken == storage.fileCount)
				break;
			storage.availableFilesEvent.IsSet();
			lock.Enter();
		}

		for (u32 i=0; i!=storage.fileCount; ++i)
		{
			File& file = storage.files[i];
			if (file.mappedMemory)
			{
				UnmapViewOfFile(m_logger, file.mappedMemory, file.capacity, TC("FileMappingBuffer"));
				file.mappedMemory = nullptr;
			}
			//UBA_ASSERT(!file.activeMapCount);
			CloseFileMapping(m_logger, file.handle, TC("FileMappingBuffer"));
			CloseFile(nullptr, file.file);
		}
		storage.fileCount = 0;
		storage.availableFile = nullptr;
	}

	MappedView FileMappingBuffer::MapView(FileMappingHandle handle, u64 offset, u64 size, const tchar* hint, bool isIndependent)
	{
		return MapView(handle, offset, size, hint, isIndependent, FILE_MAP_READ);
	}

	MappedView FileMappingBuffer::MapViewWrite(FileMappingHandle handle, u64 offset, u64 size, const tchar* hint, bool isIndependent)
	{
		return MapView(handle, offset, size, hint, isIndependent, FILE_MAP_WRITE);
	}

	MappedView FileMappingBuffer::MapView(FileMappingHandle handle, u64 offset, u64 size, const tchar* hint, bool isIndependent, u32 desiredAccess)
	{
		UBA_ASSERT(handle.IsValid());// && (handle == m_files[0].handle || handle == m_files[1].handle));

		MappedView res;

		if (isIndependent)
		{
			u8* data = MapViewOfFile(m_logger, handle, desiredAccess, offset, size);
			if (!data)
				return {};
			res.handle = handle;
			res.offset = offset;
			res.size = size;
			res.memory = data;
			res.isIndependent = true;
			return res;
		}

		u8 storageIndex = 255;
		File& file = GetFile(handle, storageIndex);

		u64 alignedOffsetStart = 0;

		u8* data;
		if (file.mappedMemory)
		{
			data = file.mappedMemory;
		}
		else
		{
			alignedOffsetStart = AlignUp(offset - (m_pageSize - 1), m_pageSize);
			u64 newOffset = offset + size;
			u64 alignedOffsetEnd = AlignUp(newOffset, m_pageSize);
			data = MapViewOfFile(m_logger, handle, desiredAccess, alignedOffsetStart, alignedOffsetEnd - alignedOffsetStart);
			if (!data)
			{
				m_logger.Error(TC("%s - MapView failed to map view of file for %s with size %llu and offset %llu (%s)"), file.name, hint, size, offset, LastErrorToText().data);
				return res;
			}
		}

		//++file.activeMapCount;

		res.handle = handle;
		res.offset = offset;
		res.size = size;
		res.memory = data + (offset - alignedOffsetStart);
		return res;
	}

	MappedView FileMappingBuffer::MapView(const StringView& str, u64 size, const tchar* hint)
	{
#if PLATFORM_WINDOWS
		const tchar* handleStr = str.data + 1;
		const tchar* handleStrEnd = TStrchr(handleStr, '-');
		u64 mappingOffset = 0;
		if (handleStrEnd)
		{
			const tchar* mappingOffsetStr = handleStrEnd + 1;
			mappingOffset = StringToValueBase62(mappingOffsetStr, wcslen(mappingOffsetStr));
		}
		else
			handleStrEnd = handleStr + wcslen(handleStr);

		FileMappingHandle h = FileMappingHandle::FromU64(StringToValueBase62(handleStr, handleStrEnd - handleStr));
		
		return MapView(h, mappingOffset, size, hint);
#else
		return {};
#endif
	}

	void FileMappingBuffer::UnmapView(MappedView view, const tchar* hint_, u64 newSize)
	{
		if (!view.handle.IsValid())
			return;

		auto unmap = [=, this](const tchar* hint)
			{
				if (view.isIndependent)
				{
					if (!UnmapViewOfFile(m_logger, view.memory, view.size, hint))
						m_logger.Error(TC("Failed to unmap view on address %llx (offset %llu) - %s (%s)"), u64(view.memory), view.offset, hint, LastErrorToText().data);
					return;
				}

				u8 storageIndex = 255;
				File& file = GetFile(view.handle, storageIndex);

				u64 alignedOffsetStart = AlignUp(view.offset - (m_pageSize - 1), m_pageSize);
				u64 alignedOffsetEnd = AlignUp(view.offset + view.size, m_pageSize);

				u8* memory = view.memory - (view.offset - alignedOffsetStart);
				u64 mapSize = alignedOffsetEnd - alignedOffsetStart;
				if (!file.mappedMemory)
					if (!UnmapViewOfFile(m_logger, memory, mapSize, hint))
						m_logger.Error(TC("%s - Failed to unmap view on address %llx (offset %llu) - %s (%s)"), file.name, u64(memory), view.offset, hint, LastErrorToText().data);

				if (newSize != InvalidValue)
				{
					if (newSize != view.size)
					{
						UBA_ASSERT(!file.commitOnAlloc);
						UBA_ASSERTF(newSize < view.size, TC("%s - Reserved too little memory. Reserved %llu, needed %llu for %s"), file.name, view.size, newSize, hint);
						file.size -= view.size - newSize;
					}

					MappingStorage& storage = m_storage[storageIndex];
					PushFile(storage, &file);
				}

				//--file.activeMapCount;
			};
		if (m_workManager && newSize == InvalidValue)
			m_workManager->AddWork([=, h = TString(hint_)](const WorkContext&) { unmap(h.c_str()); }, 1, TC("UnmapView"));
		else
			unmap(hint_);
	}

	void FileMappingBuffer::GetSizeAndCount(FileMappingType type, u64& outSize, u32& outCount)
	{
		MappingStorage& storage = m_storage[type];
		outSize = 0;
		outCount = 0;
		for (u32 i = 0; i != storage.fileCount; ++i)
		{
			if (storage.files[i].handle.IsValid())
				++outCount;
			outSize += storage.files[i].size;
		}
	}


	////////////////////////////////////////////////////////////////////////////////////////////////////

	FileMappingAllocator::FileMappingAllocator(Logger& logger, const tchar* name)
	:	m_logger(logger)
	,	m_name(name)
	{
	}

	FileMappingAllocator::~FileMappingAllocator()
	{
		if (m_mappingHandle.IsValid())
			CloseFileMapping(m_logger, m_mappingHandle, TC("FileMappingAllocator"));
	}

	bool FileMappingAllocator::Init(u64 blockSize, u64 capacity)
	{
		m_mappingHandle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE|SEC_RESERVE, capacity, nullptr, TC("FileMappingAllocator"));
		if (!m_mappingHandle.IsValid())
			return m_logger.Error(TC("%s - Failed to create memory map with capacity %llu (%s)"), m_name, capacity, LastErrorToText().data);

		m_blockSize = blockSize;
		m_pageSize = 64*1024;
		m_capacity = capacity;
		return true;
	}

	FileMappingAllocator::Allocation FileMappingAllocator::Alloc(const tchar* hint)
	{
		SCOPED_FUTEX(m_mappingLock, lock);

		u64 index = m_mappingCount;
		bool needCommit = false;
		if (!m_availableBlocks.empty())
		{
			auto it = m_availableBlocks.begin();
			index = *it;
			m_availableBlocks.erase(it);
		}
		else
		{
			++m_mappingCount;
			needCommit = true;
		}
		lock.Leave();

		u64 offset = index*m_blockSize;
		u8* data = MapViewOfFile(m_logger, m_mappingHandle, FILE_MAP_READ|FILE_MAP_WRITE, offset, m_blockSize);
		if (!data)
		{
			if (m_capacity < m_mappingCount*m_blockSize)
				m_logger.Error(TC("%s - Out of capacity (%llu) need to bump capacity for %s (%s)"), m_name, m_capacity, hint, LastErrorToText().data);
			else
				m_logger.Error(TC("%s - Alloc failed to map view of file for %s (%s)"), m_name, hint, LastErrorToText().data);
			return { {}, 0, 0 };
		}

		if (needCommit)
		{
			if (!MapViewCommit(data, m_blockSize))
			{
				m_logger.Error(TC("%s - Failed to allocate memory for %s (%s)"), m_name, hint, LastErrorToText().data);
				return { {}, 0, 0 };
			}
		}
		return {m_mappingHandle, offset, data};
	}

	void FileMappingAllocator::Free(Allocation allocation)
	{
		UBA_ASSERT(allocation.handle == m_mappingHandle);
		if (!UnmapViewOfFile(m_logger, allocation.memory, m_blockSize, m_name))
			m_logger.Error(TC("%s - Failed to unmap view of file (%s)"), m_name, LastErrorToText().data);
		u64 index = allocation.offset / m_blockSize;
		SCOPED_FUTEX(m_mappingLock, lock);
		m_availableBlocks.insert(index);
	}
}
