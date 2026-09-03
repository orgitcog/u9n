// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaEvent.h"
#include "UbaHash.h"
#include "UbaSynchronization.h"

namespace uba
{
	class Logger;
	class WorkManager;

	////////////////////////////////////////////////////////////////////////////////////////////////////

#if !PLATFORM_WINDOWS
	inline constexpr u32 FILE_MAP_WRITE = 0x0002;
	inline constexpr u32 FILE_MAP_READ = 0x0004;
	inline constexpr u32 FILE_MAP_ALL_ACCESS = FILE_MAP_WRITE | FILE_MAP_READ;
	inline constexpr u32 SEC_RESERVE = 0x04000000;
	inline constexpr u32 PAGE_READWRITE = 0x04;

	#if PLATFORM_MAC
	inline constexpr u32 SHM_MAX_FILENAME = PSHMNAMLEN;
	#else
	inline constexpr u32 SHM_MAX_FILENAME = 38;
	#endif
#endif

	FileMappingHandle CreateMemoryMappingW(Logger& logger, u32 flProtect, u64 maxSize, const tchar* name, const tchar* hint);
	FileMappingHandle CreateFileMappingW(Logger& logger, FileHandle file, u32 flProtect, u64 maxSize, const tchar* hint);
	u8* MapViewOfFile(Logger& logger, FileMappingHandle fileMappingObject, u32 dwDesiredAccess, u64 offset, u64 dwNumberOfBytesToMap);
	bool MapViewCommit(void* address, u64 size);
	bool UnmapViewOfFile(Logger& logger, const void* lpBaseAddress, u64 bytesToUnmap, const tchar* hint);
	bool CloseFileMapping(Logger& logger, FileMappingHandle h, const tchar* hint);
	bool DuplicateFileMapping(Logger& logger, ProcHandle sourceProcessHandle, FileMappingHandle sourceHandle, ProcHandle targetProcessHandle, FileMappingHandle& targetHandle, u32 dwDesiredAccess, bool bInheritHandle, u32 dwOptions, const tchar* hint);
	void MapMemoryCopy(void* dest, const void* source, u64 size);

	////////////////////////////////////////////////////////////////////////////////////////////////////

	struct MappedView
	{
		FileMappingHandle handle;
		u64 offset = 0;
		u64 size = 0;
		u8* memory = nullptr;
		bool isCompressed = true;
		bool isIndependent = false;
	};

	enum FileMappingType : u8
	{
		MappedView_Transient = 0,
		MappedView_Persistent = 1,
	};

	class FileMappingBuffer
	{
	public:
		FileMappingBuffer(Logger& logger, WorkManager* workManager = nullptr);
		~FileMappingBuffer();

		bool AddTransient(const tchar* name, bool keepMapped);
		bool AddPersistent(const tchar* name, FileHandle fileHandle, u64 size, u64 capacity);
		void CloseDatabase();

		MappedView AllocAndMapView(FileMappingType type, u64 size, u64 alignment, const tchar* hint, bool allowShrink = false, bool isIndependent = false);
		MappedView MapView(FileMappingHandle handle, u64 offset, u64 size, const tchar* hint, bool isIndependent = false);
		MappedView MapViewWrite(FileMappingHandle handle, u64 offset, u64 size, const tchar* hint, bool isIndependent = false);
		MappedView MapView(const StringView& str, u64 size, const tchar* hint);
		void UnmapView(MappedView view, const tchar* hint, u64 newSize = InvalidValue);
		
		void GetSizeAndCount(FileMappingType type, u64& outSize, u32& outCount);
		FileMappingHandle GetPersistentHandle(u32 index) { return m_storage[MappedView_Persistent].files[index].handle; }
		FileHandle GetPersistentFile(u32 index) { return m_storage[MappedView_Persistent].files[index].file; }
		u64 GetPersistentSize(u32 index) { return m_storage[MappedView_Persistent].files[index].size; }

		u64 GetFileMappingCapacity();

	private:
		struct File
		{
			File* next = nullptr;
			const tchar* name = nullptr;
			FileHandle file = InvalidFileHandle;
			FileMappingHandle handle;
			u64 size = 0;
			u64 capacity = 0;
			u8* mappedMemory = nullptr;
			//Atomic<u32> activeMapCount;
			bool commitOnAlloc = false;
			bool keepMapped = false;
		};

		MappedView MapView(FileMappingHandle handle, u64 offset, u64 size, const tchar* hint, bool isIndependent, u32 desiredAccess);
		MappedView AllocAndMapViewNoLock(File& file, u64 size, u64 alignment, const tchar* hint);
		File& GetFile(FileMappingHandle handle, u8& outStorageIndex);

		Logger& m_logger;
		WorkManager* m_workManager;
		u64 m_pageSize = 0;

		struct MappingStorage
		{
			File files[128];
			u32 fileCount = 0;
			u32 fullFileCount = 0;

			Futex availableFilesLock;
			Event availableFilesEvent;
			File* availableFile = nullptr;

			MappingStorage() : availableFilesEvent(false) {}
		};

		File* PopFile(MappingStorage& storage, u64 size, u64 alignment);
		void PushFile(MappingStorage& storage, File* file);
		void CloseMappingStorage(MappingStorage& storage);

		MappingStorage m_storage[2];

		FileMappingBuffer(const FileMappingBuffer&) = delete;
		void operator=(const FileMappingBuffer&) = delete;
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////

	class FileMappingAllocator
	{
	public:
		FileMappingAllocator(Logger& logger, const tchar* name);
		~FileMappingAllocator();

		bool Init(u64 blockSize, u64 capacity);

		struct Allocation
		{
			FileMappingHandle handle;
			u64 offset = 0;
			u8* memory = nullptr;
		};

		Allocation Alloc(const tchar* hint);
		void Free(Allocation allocation);

		u64 GetSize();

	private:
		Logger& m_logger;
		const tchar* m_name;
		u64 m_pageSize = 0;
		u64 m_capacity = 0;
		u64 m_blockSize = 0;

		Futex m_mappingLock;
		FileMappingHandle m_mappingHandle;
		u64 m_mappingCount = 0;

		Set<u64> m_availableBlocks;

		FileMappingAllocator(const FileMappingAllocator&) = delete;
		void operator=(const FileMappingAllocator&) = delete;
	};
}
