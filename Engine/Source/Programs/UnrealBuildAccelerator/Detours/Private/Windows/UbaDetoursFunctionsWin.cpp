// Copyright Epic Games, Inc. All Rights Reserved.

#define UBA_IS_DETOURED_INCLUDE 1

#include "UbaDetoursFunctionsWin.h"
#include "UbaDetoursFileMappingTable.h"
#include "UbaDetoursApi.h"

#if !defined(UBA_USE_MIMALLOC)
#define True_malloc malloc
#else
// Taken from mimalloc/types.h
#define MI_ZU(x)  x##ULL
#define MI_SEGMENT_MASK                   ((uintptr_t)(MI_SEGMENT_ALIGN - 1))
#define MI_SEGMENT_ALIGN                  MI_SEGMENT_SIZE
#define MI_SEGMENT_SIZE                   (MI_ZU(1)<<MI_SEGMENT_SHIFT)
#define MI_SEGMENT_SHIFT                  ( 9 + MI_SEGMENT_SLICE_SHIFT)  // 32MiB
#define MI_SEGMENT_SLICE_SHIFT            (13 + MI_INTPTR_SHIFT)
#define MI_INTPTR_SHIFT (3)
#endif

#include <ntstatus.h>
#define WIN32_NO_STATUS

#include <malloc.h>

NTSTATUS NtCopyFileChunk(HANDLE Source, HANDLE Dest, HANDLE Event, PIO_STATUS_BLOCK IoStatusBlock, ULONG Length, PULONG SourceOffset, PULONG DestOffset, PULONG SourceKey, PULONG DestKey, ULONG Flags) { return 0; }
#define NtCurrentProcess() ((HANDLE)-1)

#define DETOURED_FUNCTION(Func) decltype(Func)* True_##Func = ::Func; 
DETOURED_FUNCTIONS
DETOURED_FUNCTIONS_MEMORY
DETOURED_FUNCTIONS_MEMORY_NON_WINE
#undef DETOURED_FUNCTION

#define DETOURED_FUNCTION(Func) void* Local_##Func = ::Func;
DETOURED_FUNCTIONS_MEMORY
DETOURED_FUNCTIONS_MEMORY_NON_WINE
#undef DETOURED_FUNCTION

#include "UbaBinaryParser.h"
#include "UbaDirectoryTable.h"
#include "UbaProcessStats.h"
#include "UbaProtocol.h"
#include "UbaDetoursPayload.h"
#include "UbaApplicationRules.h"
#include "UbaDetoursShared.h"
#include "UbaDetoursUtilsWin.h"

#include "Shlwapi.h"
#include <detours/detours.h>
#include <stdio.h>

namespace uba
{

bool g_useMiMalloc;
constexpr u64  g_pageSize = 64*1024;

thread_local u32 t_disallowCreateFileDetour	 = 0; // Set this to 1 to disallow file detour.. note that this will prevent directory cache from properly being updated

// Beautiful! cl.exe needs an exact address in that range to be able to map in pch file
// So we'll reserve a bigger range than will be requested and give it back when needed.
constexpr uintptr_t g_clExeBaseAddress = 0x6bb00000000;
constexpr u64 g_clExeBaseAddressSize = 0x400000000;
void* g_clExeBaseReservedMemory = 0;

HANDLE PseudoHandle = (HANDLE)0xfffffffffffffffe;
constexpr int StdOutFd = -2;

#if UBA_DEBUG_LOG_ENABLED
bool g_debugFileFlushOnWrite = false;

void WriteDebug(const void* data, u32 dataLen)
{
	auto readMem = (const u8*)data;
	DWORD toWrite = (DWORD)dataLen;
	DWORD lastError = GetLastError();
	while (true)
	{
		DWORD written;
		if (True_WriteFile((HANDLE)g_debugFile, readMem, toWrite, &written, NULL))
			break;
		if (GetLastError() != ERROR_IO_PENDING)
			break;// ExitProcess(1340); // During shutdown this might actually cause error and we just ignore that and break out
		readMem += written;
		toWrite -= written;
	}
	if (g_debugFileFlushOnWrite)
		True_FlushFileBuffers((HANDLE)g_debugFile);
	SetLastError(lastError);
}
void FlushDebugLog()
{
	if (isLogging())
		True_FlushFileBuffers((HANDLE)g_debugFile);
}
#endif

//#define UBA_PROFILE_DETOURED_CALLS

#if defined(UBA_PROFILE_DETOURED_CALLS)
#define DETOURED_FUNCTION(name) Timer timer##name;
DETOURED_FUNCTIONS
DETOURED_FUNCTIONS_MEMORY
DETOURED_FUNCTIONS_MEMORY_NON_WINE
#undef DETOURED_FUNCTION
#define DETOURED_CALL(name) TimerScope _(timer##name)
#else
#define DETOURED_CALL(name) //DEBUG_LOG(TC(#name));
#endif

bool g_isDetachedProcess;
bool g_isRunningWine;
int g_uiLanguage;
u32 g_processId;

constexpr u32 TrackInputsMemCapacity = 512 * 1024;
u8* g_trackInputsMem;
u32 g_trackInputsBufPos;

void SendInput()
{
	u32 left = g_trackInputsBufPos;
	u32 reserveSize = left;
	u32 pos = 0;
	while (left)
	{
		RPC_MESSAGE(InputDependencies, log)
		writer.Write7BitEncoded(reserveSize);
		reserveSize = 0;
		u32 toWrite = Min(left, u32(writer.GetCapacityLeft() - sizeof(u32)));
		writer.WriteU32(toWrite);
		writer.WriteBytes(g_trackInputsMem + pos, toWrite);
		writer.Flush();
		left -= toWrite;
		pos += toWrite;
	}
	g_trackInputsBufPos = 0;
}


void TrackInput(const wchar_t* file)
{
	if (!g_trackInputsMem)
		return;

	if (g_trackInputsBufPos > TrackInputsMemCapacity - 2048)
		SendInput();

	BinaryWriter w(g_trackInputsMem, g_trackInputsBufPos, TrackInputsMemCapacity);
	w.WriteString(file);
	g_trackInputsBufPos = u32(w.GetPosition());
}
void SkipTrackInput(const wchar_t* file)
{
	// Just here to easily log out what we are ignoring in terms of input
}

u8 g_emptyMemoryFileMem;
MemoryFile& g_emptyMemoryFile = *new MemoryFile(&g_emptyMemoryFileMem, true);

constexpr u64 DetouredHandleMaxCount = 200*1024; // ~200000 handles enough?
constexpr u64 DetouredHandleStart = 300000; // Let's hope noone uses the handles starting at 300000! :)
constexpr u64 DetouredHandleEnd = DetouredHandleStart + DetouredHandleMaxCount;
constexpr u64 DetouredHandlesMemReserve = AlignUp(DetouredHandleMaxCount*sizeof(DetouredHandle), 64*1024);
constexpr u64 DetouredHandlesMemStart = 0;
MemoryBlock& g_detouredHandleMemoryBlock = *new MemoryBlock(DetouredHandlesMemReserve, (void*)DetouredHandlesMemStart);
u64 g_detouredHandlesStart = u64(g_detouredHandleMemoryBlock.memory);
u64 g_detouredHandlesEnd = g_detouredHandlesStart + g_detouredHandleMemoryBlock.reserveSize;
BlockAllocator<DetouredHandle> g_detouredHandleAllocator(g_detouredHandleMemoryBlock);
void* DetouredHandle::operator new(size_t size) { return g_detouredHandleAllocator.Allocate(); }
void DetouredHandle::operator delete(void* p) { g_detouredHandleAllocator.Free(p); }

inline bool isDetouredHandle(HANDLE h)
{
	return u64(h) >= DetouredHandleStart && u64(h) < DetouredHandleEnd;
}

inline HANDLE makeDetouredHandle(DetouredHandle* p)
{
	u64 index = (u64(p) - g_detouredHandlesStart) / sizeof(DetouredHandle);
	UBA_ASSERT(index < DetouredHandleMaxCount);
	return HANDLE(DetouredHandleStart + index);
}

inline DetouredHandle& asDetouredHandle(HANDLE h)
{
	u64 index = u64(h) - DetouredHandleStart;
	u64 p = (index * sizeof(DetouredHandle)) + g_detouredHandlesStart;
	return *(DetouredHandle*)p;
}

HANDLE g_stdHandle[3];
HANDLE g_nullFile;

struct ListDirectoryHandle
{
	void* operator new(size_t size);
	void operator delete(void* p);

	StringKey dirNameKey;
	DirectoryTable::Directory& dir;
	int it;
	Vector<u32> fileTableOffsets;
	HANDLE validateHandle;
	TString wildcard;
	const wchar_t* originalName = nullptr;
};

constexpr u64 ListDirHandlesRange = 4*1024*1024;
MemoryBlock& g_listDirHandleMemoryBlock = *new MemoryBlock(ListDirHandlesRange);
constexpr u64 ListDirHandlesStart = DetouredHandleEnd;
constexpr u64 ListDirHandlesEnd = ListDirHandlesStart + ListDirHandlesRange/sizeof(ListDirectoryHandle);
BlockAllocator<ListDirectoryHandle>& g_listDirectoryHandleAllocator = *new BlockAllocator<ListDirectoryHandle>(g_listDirHandleMemoryBlock);
void* ListDirectoryHandle::operator new(size_t size) { return g_listDirectoryHandleAllocator.Allocate(); }
void ListDirectoryHandle::operator delete(void* p) { g_listDirectoryHandleAllocator.Free(p); }
inline bool isListDirectoryHandle(HANDLE h) { return (u64)h >= ListDirHandlesStart && (u64)h < ListDirHandlesEnd; }
inline HANDLE makeListDirectoryHandle(ListDirectoryHandle* p) { return HANDLE(ListDirHandlesStart + (p - (ListDirectoryHandle*)g_listDirHandleMemoryBlock.memory)); }
inline ListDirectoryHandle& asListDirectoryHandle(HANDLE h) { return *(((ListDirectoryHandle*)g_listDirHandleMemoryBlock.memory) + (u64(h) - ListDirHandlesStart)); }

ReaderWriterLock& g_loadedModulesLock = *new ReaderWriterLock();
UnorderedMap<HMODULE, TString>& g_loadedModules = *new UnorderedMap<HMODULE, TString>();
u64 g_memoryFileIndexCounter = ~u64(0) - 1000000; // I really hope this will not collide with anything
bool g_filesCouldBeCompressed;

bool CouldBeCompressedFile(const StringView& fileName)
{
	return g_filesCouldBeCompressed && g_globalRules.FileCanBeCompressed(fileName);
}

bool CanDetour(const tchar* path)
{
	if (t_disallowDetour || !path)
		return false;

	if (path[0] == '\\')
	{
		if (path[1] == '\\')
		{
			if (path[2] == '.' && path[3] == '\\') // \\.\ - Win32 namespace for files and devices
			{
				if (path[5] != ':') // Not file
					return false;
				path += 4;
			}
			else if (path[2] == '?') // \\?\ - Win32 path prefix to send through unmodified to nt layer
			{
				if (path[3] != '\\')
					return false;
				path += 4;
			}
			else
			{
				if (path[2] == '\\' || path[2] == '/' || path[2] == ':' || path[2] == '*' || path[2] == '?' || path[2] == '\"' || path[2] == '<' || path[2] == '>' || path[2] == '|')
					return false; // Unknown
			}
		}
		else if (path[1] == '?' && path[2] == '?' && path[3] == '\\')
		{
			if (path[4] == 'U' && path[5] == 'N' && path[6] == 'C') // All network paths ok?
				return true;
			if (path[5] == ':')
				path += 4;
			else
				return false; // Unknown
		}
	}
	if (Equals(path, TC("nul")))
		return false;

	return g_rules->CanDetour(path, g_runningRemote);
}

struct SuppressCreateFileDetourScope
{
	SuppressCreateFileDetourScope() { ++t_disallowCreateFileDetour; }
	~SuppressCreateFileDetourScope() { --t_disallowCreateFileDetour; }
};

const wchar_t* HandleToName(const DetouredHandle& dh)
{
	if (dh.fileObject)
		if (const wchar_t* name = dh.fileObject->fileInfo->name)
			return name;
	return L"Unknown";
}

BlockAllocator<FileObject>& g_fileObjectAllocator = *new BlockAllocator<FileObject>(g_memoryBlock);

MemoryFile::MemoryFile(u8* data, bool localOnly)
:	baseAddress(data)
,	isLocalOnly(localOnly)
{
}

MemoryFile::MemoryFile(bool localOnly, u64 reserveSize_, bool isThrowAway_, const tchar* fileName)
:	isLocalOnly(localOnly)
,	isThrowAway(isThrowAway_)
{
	if (!isThrowAway)
		Reserve(reserveSize_, fileName);
}

void MemoryFile::Reserve(u64 reserveSize_, const tchar* fileName)
{
	UBA_ASSERT(!isThrowAway);
	reserveSize = reserveSize_;
	if (isLocalOnly)
	{
		baseAddress = (u8*)VirtualAlloc(NULL, reserveSize, MEM_RESERVE, PAGE_READWRITE);
		if (!baseAddress)
			FatalError(1354, L"VirtualAlloc failed trying to reserve %llu for %s. (Error code: %u)", reserveSize, fileName, GetLastError());
		mappedSize = reserveSize;
	}
#if UBA_ENABLE_ON_DISK_FILE_MAPPINGS
	else if (fileName && !g_runningRemote)
	{
		StringBuffer<> tempFileName;
		tempFileName.Appendf(L"\\??\\").Append(fileName);
		if (false)
			tempFileName.Append(L".uba.tmp");

		UNICODE_STRING us;
		us.Length = USHORT(tempFileName.count*sizeof(tchar));
		us.MaximumLength = us.Length;
		us.Buffer = tempFileName.data;

		SuppressDetourScope _;
		{
			TimerScope ts(g_kernelStats.createFile);
			OBJECT_ATTRIBUTES oa;
			InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);
			IO_STATUS_BLOCK iosb;
			ACCESS_MASK desiredAccess = GENERIC_READ | GENERIC_WRITE | DELETE | SYNCHRONIZE | FILE_WRITE_ATTRIBUTES;
			ULONG shareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
			ULONG createOptions = FILE_SYNCHRONOUS_IO_NONALERT;// | FILE_DELETE_ON_CLOSE;
			NTSTATUS status = ZwCreateFile(&mappingHandle.fh, desiredAccess, &oa, &iosb, 0, FILE_ATTRIBUTE_NORMAL, shareAccess, FILE_OVERWRITE_IF, createOptions, 0, 0);
			if (status != STATUS_SUCCESS)
				FatalError(1347, L"ZwCreateFile failed to create %s. (Error code: 0x%x)", us.Buffer, status);
		}
		FILE_DISPOSITION_INFO info;
		info.DeleteFile = true;
		if (!::SetFileInformationByHandle(mappingHandle.fh, FileDispositionInfo, &info, sizeof(info)))
			FatalError(1347, L"SetFileInformationByHandle failed to set delete-on-close on %s. (Error code: 0x%x)", us.Buffer, GetLastError());
		{
			TimerScope ts(g_kernelStats.createFileMapping);
			LARGE_INTEGER liMaxSize; liMaxSize.QuadPart = 2;
			NTSTATUS status = ZwCreateSection(&mappingHandle.mh, SECTION_ALL_ACCESS, 0, &liMaxSize, PAGE_READWRITE, SEC_COMMIT, mappingHandle.fh);
			if (status != STATUS_SUCCESS)
				FatalError(1348, L"NtCreateSection failed to reserve %llu. (Error code: 0x%x)", reserveSize, status);
		}
		TimerScope ts(g_kernelStats.mapViewOfFile);
		NTSTATUS status = ZwMapViewOfSection(mappingHandle.mh, NtCurrentProcess(), (void**)&baseAddress, 0, 0, NULL, &reserveSize, 2, MEM_RESERVE, PAGE_READWRITE);
		if (status != STATUS_SUCCESS)
			FatalError(1348, L"ZwMapViewOfSection failed trying to reserve %llu. (Error code: 0x%x)", reserveSize, status);
		mappedSize = reserveSize;
	}
#endif
	else
	{
		HANDLE handle;
		{
			mappedSize = 32 * 1024 * 1024;
			TimerScope ts(g_kernelStats.createFileMapping);
			handle = True_CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_RESERVE, ToHigh(reserveSize), ToLow(reserveSize), NULL);
			if (!handle)
				FatalError(1348, L"CreateFileMappingW failed trying to reserve %llu for %s. (Error code: %u)", reserveSize, fileName, GetLastError());
		}
		TimerScope ts(g_kernelStats.mapViewOfFile);
		baseAddress = (u8*)True_MapViewOfFile(handle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, mappedSize);
		if (!baseAddress)
			FatalError(1353, L"MapViewOfFile failed trying to map %llu for %s. ReservedSize: %llu (Error code: %u)", mappedSize, fileName, reserveSize, GetLastError());
		mappingHandle = { 0, handle };
	}
}

void MemoryFile::Unreserve()
{
	if (isLocalOnly)
	{
		VirtualFree(baseAddress, 0, MEM_RELEASE);
	}
	else
	{
		True_UnmapViewOfFile(baseAddress);
		CloseHandle(mappingHandle.mh);
		mappingHandle.mh = nullptr;
	}
	baseAddress = nullptr;
	committedSize = 0;
}

void MemoryFile::Write(DetouredHandle& handle, LPCVOID lpBuffer, u64 nNumberOfBytesToWrite)
{
	u64 newPos = handle.pos + nNumberOfBytesToWrite;

	if (isThrowAway)
	{
		writtenSize = newPos;
		return;
	}

	EnsureCommitted(handle, newPos);
	memcpy(baseAddress + handle.pos, lpBuffer, nNumberOfBytesToWrite);
	handle.pos += nNumberOfBytesToWrite;
	if (writtenSize < newPos)
	{
		writtenSize = newPos;
		isReported = false;
	}
}

void MemoryFile::EnsureCommitted(const DetouredHandle& handle, u64 size)
{
	if (isThrowAway)
		return;

	if (committedSize >= size)
		return;
	if (size > mappedSize)
	{
		bool shouldRemap = true;
		if (size > reserveSize)
		{
			if (writtenSize == 0 && !isReported)
			{
				u64 newReserve = AlignUp(size, g_pageSize);
				if (reserveSize)
					Rpc_WriteLogf(L"TODO: RE-RESERVING MemoryFile. Initial reserve: %llu, New reserve: %llu. Please fix application rules", reserveSize, newReserve);
				Unreserve();
				Reserve(newReserve);
				shouldRemap = false;
			}
			else
				FatalError(1347, L"Reserved size of %ls is smaller than what is requested to be. ReserveSize: %llu Written: %llu Requested: %llu", HandleToName(handle), reserveSize, writtenSize, size);
		}

		if (shouldRemap)
			Remap(handle, size);
	}

#if UBA_ENABLE_ON_DISK_FILE_MAPPINGS
	if (mappingHandle.fh)
	{
		if (!committedSize)
			committedSize = size;
		else
			committedSize = Min(reserveSize, Max(size, committedSize * 2));
		auto lg = ToLargeInteger(committedSize);
		NtExtendSection(mappingHandle.mh, &lg);
	}
	else
#endif
	{
		u64 toCommit = Min(reserveSize, AlignUp(size - committedSize, g_pageSize));
		if (!VirtualAlloc(baseAddress + committedSize, toCommit, MEM_COMMIT, PAGE_READWRITE))
			FatalError(1347, L"Failed to ensure virtual memory for %ls trying to commit %llu at %llx. MappedSize: %llu, CommittedSize: %llu RequestedSize: %llu. (%u)", HandleToName(handle), toCommit, baseAddress + committedSize, mappedSize, committedSize, size, GetLastError());
		committedSize += toCommit;
	}
}

void MemoryFile::Remap(const DetouredHandle& handle, u64 size)
{
	UBA_ASSERT(!mappingHandle.fh);
	True_UnmapViewOfFile(baseAddress);
	mappedSize = Min(reserveSize, AlignUp(Max(size, mappedSize * 4), g_pageSize));
	TimerScope ts(g_kernelStats.mapViewOfFile);
	baseAddress = (u8*)True_MapViewOfFile(mappingHandle.mh, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, mappedSize);
	if (!baseAddress)
		FatalError(1347, L"MapViewOfFile failed trying to map %llu for %ls. ReservedSize: %llu (Error code: %u)", mappedSize, HandleToName(handle), reserveSize, GetLastError());
}

void ToInvestigate(const wchar_t* format, ...)
{
#if UBA_DEBUG_LOG_ENABLED
	va_list arg;
	va_start (arg, format);
	StringBuffer<> buffer;
	buffer.Append(format, arg);
	va_end (arg);
	DEBUG_LOG(buffer.data);
	FlushDebugLog();
	Rpc_WriteLogf(L"%ls\n", buffer.data);
#endif
}

UBA_NOINLINE void UbaAssert(const tchar* text, const char* file, u32 line, const char* expr, bool allowTerminate, u32 terminateCode, void* context, u32 skipCallstackCount)
{
	SuppressDetourScope _;
	static CriticalSection cs;
	ScopedCriticalSection s(cs);

	#if UBA_DEBUG_LOG_ENABLED
	FlushDebugLog();
	#endif

	static auto& sb = *new StringBuffer<16*1024>();
	WriteAssertInfo(sb.Clear(), text, file, line, expr);
	Rpc_ResolveCallstack(sb, 3 + skipCallstackCount, context);
	Rpc_WriteLog(sb.data, sb.count, true, true);

	#if UBA_DEBUG_LOG_ENABLED
	FlushDebugLog();
	#endif

	#if UBA_ASSERT_MESSAGEBOX
	StringBuffer<> title;
	title.Appendf(L"Assert %ls - pid %u", GetApplicationShortName(), GetCurrentProcessId());
	int ret = MessageBoxW(GetConsoleWindow(), sb.data, title.data, MB_ABORTRETRYIGNORE|MB_SYSTEMMODAL);
	if (ret == IDABORT)
		ExitProcess(terminateCode);
	else if (ret == IDRETRY && IsDebuggerPresent())
		DebugBreak();
	#else
	if (allowTerminate)
		ExitProcess(terminateCode);
	#endif
}

extern HANDLE g_hostProcess;

const wchar_t* HandleToName(HANDLE handle)
{
	if (handle == INVALID_HANDLE_VALUE)
		return L"INVALID";
	if (isListDirectoryHandle(handle))
	{
		#if UBA_DEBUG
		return asListDirectoryHandle(handle).originalName;
		#else
		return L"DIRECTORY";
		#endif
	}
	if (!isDetouredHandle(handle))
		return L"UNKNOWN";
	DetouredHandle& dh = asDetouredHandle(handle);
	if (FileObject* fo = dh.fileObject)
		if (FileInfo* fi = fo->fileInfo)
			if (const wchar_t* name = fi->name)
				return name;
	return L"DETOURED";
}


u64 FileTypeMaxSize(const StringBufferBase& file, bool isSystemOrTempFile) { return g_rules->FileTypeMaxSize(file, isSystemOrTempFile); }

bool EnsureMapped(DetouredHandle& handle, DWORD dwFileOffsetHigh = 0, DWORD dwFileOffsetLow = 0, SIZE_T numberOfBytesToMap = 0, void* baseAddress = nullptr)
{
	FileInfo& info = *handle.fileObject->fileInfo;
	
	if (info.memoryFile || info.fileMapMem)
		return true;

	u64 offset = ToLargeInteger(dwFileOffsetHigh, dwFileOffsetLow).QuadPart;
	if (!numberOfBytesToMap)
	{
		UBA_ASSERTF(info.size && info.size != InvalidValue || (info.isFileMap && info.size == 0), L"FileInfo file size is bad: %llu", info.size);
		numberOfBytesToMap = info.size;
	}

	u64 alignedOffsetStart = 0;
	if (info.trueFileMapOffset)
	{
		offset += info.trueFileMapOffset;
		u64 endOffset = offset + numberOfBytesToMap;
		alignedOffsetStart = AlignUp(offset - (g_pageSize - 1), g_pageSize);
		u64 alignedOffsetEnd = AlignUp(endOffset, g_pageSize);
		u64 mapSize = alignedOffsetEnd - alignedOffsetStart;
		TimerScope ts(g_kernelStats.mapViewOfFile);
		info.fileMapMem = (u8*)True_MapViewOfFileEx(info.trueFileMapHandle, info.fileMapViewDesiredAccess, ToHigh(alignedOffsetStart), ToLow(alignedOffsetStart), mapSize, baseAddress);
		
		// TODO: This is ugly.. but in some cases we have created virtual files pointing into segments of real file
		// and real file size is not aligned to page size so we need to set map size to 0 to allow it to map up to size of file
		if (!info.fileMapMem)
			info.fileMapMem = (u8*)True_MapViewOfFileEx(info.trueFileMapHandle, info.fileMapViewDesiredAccess, ToHigh(alignedOffsetStart), ToLow(alignedOffsetStart), 0, baseAddress);
	}
	else
	{
		UBA_ASSERTF(!info.freeFileMapOnClose, TC("File %s has been freed because of earlier close and is now reopened (%s)"), info.name, info.originalName);
		TimerScope ts(g_kernelStats.mapViewOfFile);
		info.fileMapMem = (u8*)True_MapViewOfFileEx(info.trueFileMapHandle, info.fileMapViewDesiredAccess, 0, 0, numberOfBytesToMap, baseAddress);
	}

	if (info.fileMapMem == nullptr || (baseAddress && info.fileMapMem != baseAddress))
	{
		ToInvestigate(L"MapViewOfFileEx failed trying to map %llu bytes on address 0x%llx with offset %llu, using %llu with access %u (%u)", numberOfBytesToMap, u64(baseAddress), alignedOffsetStart, u64(info.trueFileMapHandle), info.fileMapViewDesiredAccess, GetLastError());
		return false;
	}
	info.fileMapMem += (offset - alignedOffsetStart);
	info.fileMapMemSize = info.size;

	DEBUG_LOG_TRUE(L"INTERNAL MapViewOfFileEx", L"(%ls) (size: %llu) (%ls) -> 0x%llx", info.name, numberOfBytesToMap, info.originalName, uintptr_t(info.fileMapMem));
	return true;
}

ReaderWriterLock& g_longPathNameCacheLock = *new ReaderWriterLock();
using LongPathMap = GrowingUnorderedMap<const wchar_t*, const wchar_t*, HashString, EqualString>;
LongPathMap& g_longPathNameCache = *new LongPathMap(g_memoryBlock);

void Rpc_AllocFailed(const wchar_t* allocType, u32 error)
{
	RPC_MESSAGE(VirtualAllocFailed, virtualAllocFailed)
	writer.WriteString(allocType);
	writer.WriteU32(error);
	writer.Flush();
	Sleep(5*1000);
}

void CloseCaches()
{
	for (auto& it : g_mappedFileTable.m_lookup)
	{
		FileInfo& info = it.second;
		if (info.fileMapMem)
		{
			DEBUG_LOG_TRUE(L"INTERNAL UnmapViewOfFile", L"0x%llx (%ls) (%ls)", uintptr_t(info.fileMapMem), info.name, info.originalName);
			True_UnmapViewOfFile(info.fileMapMem);
		}
		if (info.trueFileMapHandle != 0)
		{
			DEBUG_LOG_TRUE(L"INTERNAL CloseHandle", L"%llu (%ls) (%ls)", uintptr_t(info.trueFileMapHandle), info.name, info.originalName);
			CloseHandle(info.trueFileMapHandle);
		}

		// Let them leak
		if (info.memoryFile && !info.memoryFile->isLocalOnly)
		{
			UnmapViewOfFile(info.memoryFile->baseAddress);
			CloseHandle(info.memoryFile->mappingHandle.mh);
			CloseHandle(info.memoryFile->mappingHandle.fh);
		}
		//if (info.memoryFile && info.memoryFile != &g_emptyMemoryFile)
		//	delete info.memoryFile;
	}
}

bool g_exitMessageSent;

void SendExitMessage(DWORD exitCode, u64 startTime);
void OnModuleLoaded(HMODULE moduleHandle, const StringView& name);

// Variables used to communicate state from kernelbase functions to ntdll functions
thread_local const wchar_t* t_renameFileNewName;
thread_local const wchar_t* t_createFileFileName;

const wchar_t* ToString(BOOL b) { return b ? L"Success" : L"Error"; }

#include "UbaDetoursFunctionsMiMalloc.inl"
#include "UbaDetoursFunctionsNtDll.inl"
#include "UbaDetoursFunctionsKernelBase.inl"
#include "UbaDetoursFunctionsUcrtBase.inl"
#include "UbaDetoursFunctionsImagehlp.inl"
#include "UbaDetoursFunctionsDbgHelp.inl"
#include "UbaDetoursFunctionsShell32.inl"
#include "UbaDetoursFunctionsRpcrt4.inl"

extern u32 g_consoleStringIndex;

void SendExitMessage(DWORD exitCode, u64 startTime)
{
	if (g_exitMessageSent)
		return;
	g_exitMessageSent = true;

	if (g_consoleStringIndex)
		Shared_WriteConsole(L"\n", 1, 0);

	#if TRACK_UCRT_ALLOC_ENABLED
	StringBuffer<256> str;
	str.Appendf(L"Alloc: %llu Realloc: %llu Free: %llu\n", g_allocCount.load(), g_reallocCount.load(), g_freeCount.load());
	Shared_WriteConsole(str.data, str.count, 0);
	#endif

	if (g_trackInputsMem)
		SendInput();

	g_stats.detoursMemory = g_memoryBlock.writtenSize; // + g_directoryTable.m_memorySize + g_mappedFileTable.m_memPosition;

	RPC_MESSAGE(Exit, log)
	writer.WriteU32(exitCode);
	writer.WriteString(g_logName);

	g_stats.detach.time += GetTime() - startTime;
	g_stats.detach.count = 1;

	g_stats.Write(writer);
	g_kernelStats.Write(writer);

	// We must flush here if this is a child because,
	// if there is a parent process waiting for this to finish,
	// the parent might move on before Exit message has been processed on session side
	writer.Flush(g_isChild);
}

void DetourAttachFunction(void** trueFunc, void* detouredFunc, const char* funcName)
{
	if (!*trueFunc)
		return;
	auto error = DetourAttach(trueFunc, detouredFunc);
	if (error == NO_ERROR)
		return;
	const char* errorString = "Unknown error";
	switch (error)
	{
	case ERROR_INVALID_BLOCK: errorString = "The function referenced is too small to be detoured."; break;
	case ERROR_INVALID_HANDLE: errorString = "The ppPointer parameter is NULL or points to a NULL pointer."; break;
	case ERROR_INVALID_OPERATION: errorString = "No pending transaction exists."; break;
	case ERROR_NOT_ENOUGH_MEMORY: errorString = "Not enough memory exists to complete the operation."; break;
	}
	Rpc_WriteLogf(L"Failed to detour %hs (%hs)", funcName, errorString);
	ExitProcess(error);
}

void DetourDetachFunction(void** trueFunc, void* detouredFunc, const char* funcName)
{
	if (!*trueFunc)
		return;
	auto error = DetourDetach(trueFunc, detouredFunc);
	if (error == NO_ERROR)
		return;
	Rpc_WriteLogf(L"Failed to detach detoured %hs", funcName);
}

void DetourTransactionBegin()
{
	LONG error = ::DetourTransactionBegin();
	if (error != NO_ERROR)
		FatalError(1357, L"DetourTransactionBegin failed (%ld)", error);
	error = ::DetourUpdateThread(GetCurrentThread());
	if (error != NO_ERROR)
		FatalError(1358, L"DetourUpdateThread failed (%ld)", error);
}

void DetourTransactionCommit()
{
	LONG error = ::DetourTransactionCommit();
	if (error != NO_ERROR)
		FatalError(1343, L"DetourTransactionCommit failed (%ld)", error);
}

int DetourAttachFunctions(bool runningRemote)
{
	DetourTransactionBegin();

	#define DETOURED_FUNCTION(Func) True_##Func = (decltype(True_##Func))GetProcAddress(moduleHandle, #Func);

	if (HMODULE moduleHandle = GetModuleHandleW(L"kernelbase.dll"))
	{
		DETOURED_FUNCTIONS_KERNELBASE
	}

	if (HMODULE moduleHandle = GetModuleHandleW(L"kernel32.dll"))
	{
		DETOURED_FUNCTIONS_KERNEL32
	}

	if (HMODULE moduleHandle = GetModuleHandleW(L"ntdll.dll"))
	{
		DETOURED_FUNCTIONS_NTDLL
	}

	if (HMODULE moduleHandle = GetModuleHandleW(L"ucrtbase.dll"))
	{
		DETOURED_FUNCTIONS_UCRTBASE
		if (g_useMiMalloc)
		{
			DETOURED_FUNCTIONS_MEMORY
			if (!g_isRunningWine)
			{
				DETOURED_FUNCTIONS_MEMORY_NON_WINE
			}
		}
	}

	if (HMODULE moduleHandle = GetModuleHandleW(L"shlwapi.dll"))
	{
		DETOURED_FUNCTIONS_SHLWAPI
	}

	#if UBA_SUPPORT_MSPDBSRV
	if (HMODULE moduleHandle = GetModuleHandleW(L"rpcrt4.dll"))
	{
		DETOURED_FUNCTIONS_RPCRT4
	}
	#endif

#undef DETOURED_FUNCTION

	// Can't attach to these when running through debugger with some vs extensions (Microsoft child process debugging)
#if UBA_DEBUG
	if (IsDebuggerPresent())
	{
		True_CreateProcessW = nullptr;
		#if defined(DETOURED_INCLUDE_DEBUG)
		True_CreateProcessA = nullptr;
		True_CreateProcessAsUserW = nullptr;
		#endif
	}
#endif

	#define DETOURED_FUNCTION(Func) DetourAttachFunction((PVOID*)&True_##Func, Detoured_##Func, #Func);
	DETOURED_FUNCTIONS
	if (g_useMiMalloc)
	{
		DETOURED_FUNCTIONS_MEMORY
		if (!g_isRunningWine)
		{
			DETOURED_FUNCTIONS_MEMORY_NON_WINE
		}
	}
	#undef DETOURED_FUNCTION


	DetourTransactionCommit();

	#if UBA_SUPPORT_MSPDBSRV
	True2_NdrClientCall2 = True_NdrClientCall2;
	#endif

	return 0;
}

void OnModuleLoaded(HMODULE moduleHandle, const StringView& name)
{
	// SymLoadModuleExW do something bad that cause remote wine to fail everything after this call.. TODO: Revisit
	if (g_isRunningWine && !True_SymLoadModuleExW && name.Contains(L"dbghelp.dll"))
	{
		True_SymLoadModuleExW = (SymLoadModuleExWFunc*)GetProcAddress(moduleHandle, "SymLoadModuleExW");
		UBA_ASSERT(True_SymLoadModuleExW);
		DetourTransactionBegin();
		DetourAttachFunction((PVOID*)&True_SymLoadModuleExW, Detoured_SymLoadModuleExW, "SymLoadModuleExW");
		DetourTransactionCommit();
	}

	// ImageGetDigestStream is buggy in wine so we have to detour it for ShaderCompileWorker
	if (g_isRunningWine && !True_ImageGetDigestStream && name.Contains(L"imagehlp.dll"))
	{
		True_ImageGetDigestStream = (ImageGetDigestStreamFunc*)GetProcAddress(moduleHandle, "ImageGetDigestStream");
		UBA_ASSERT(True_ImageGetDigestStream);
		DetourTransactionBegin();
		DetourAttachFunction((PVOID*)&True_ImageGetDigestStream, Detoured_ImageGetDigestStream, "ImageGetDigestStream");
		DetourTransactionCommit();
	}

	// SHGetKnownFolderPath is used by Metal.exe and must always execute on host
	if (!True_SHGetKnownFolderPath && name.Contains(L"shell32.dll"))
	{
		True_SHGetKnownFolderPath = (SHGetKnownFolderPathFunc*)GetProcAddress(moduleHandle, "SHGetKnownFolderPath");
		UBA_ASSERT(True_SHGetKnownFolderPath);
		DetourTransactionBegin();
		DetourAttachFunction((PVOID*)&True_SHGetKnownFolderPath, Detoured_SHGetKnownFolderPath, "SHGetKnownFolderPath");
		DetourTransactionCommit();
	}
}

int DetourDetachFunctions()
{
	if (g_directoryTable.m_memory)
		True_UnmapViewOfFile(g_directoryTable.m_memory);

	if (g_mappedFileTable.m_mem)
		True_UnmapViewOfFile(g_mappedFileTable.m_mem);

	//assert(g_wantsOnCloseLookup.empty());
	//UBA_ASSERT(g_mappedFileTable.m_memLookup.empty());

	CloseCaches();

	#define DETOURED_FUNCTION(Func) DetourDetachFunction((PVOID*)&True_##Func, Detoured_##Func, #Func);
	if (g_useMiMalloc)
	{
		DETOURED_FUNCTIONS_MEMORY
		if (!g_isRunningWine)
		{
			DETOURED_FUNCTIONS_MEMORY_NON_WINE
		}
	}
	DETOURED_FUNCTIONS
	#undef DETOURED_FUNCTION
	return 0;
}

extern bool g_reportAllExceptions;

void PreInit(const DetoursPayload& payload)
{
	#if UBA_USE_MIMALLOC
	//mi_option_enable(mi_option_large_os_pages);
	mi_option_disable(mi_option_abandoned_page_reset);
	#endif

	InitSharedVariables();

	g_reportAllExceptions = payload.reportAllExceptions;

	g_rulesIndex = payload.rulesIndex;
	g_rules = GetApplicationRules()[payload.rulesIndex].rules;
	g_useMiMalloc = payload.useCustomAllocator;
	g_runningRemote = payload.runningRemote;
	g_isChild = payload.isChild;
	g_allowKeepFilesInMemory = payload.allowKeepFilesInMemory;
	g_allowOutputFiles = g_allowKeepFilesInMemory && payload.allowOutputFiles;
	g_suppressLogging = payload.suppressLogging;
	g_isDetachedProcess = g_rules->AllowDetach();
	g_isRunningWine = payload.isRunningWine;
	g_uiLanguage = payload.uiLanguage;

	if (g_isRunningWine) // There are crashes when running in Wine and really hard to debug
	{
		//g_useMiMalloc = false;
		//g_checkRtlHeap = false;
	}

	#if UBA_DEBUG_VALIDATE
	if (g_runningRemote)
		g_validateFileAccess = false;
	#endif

	{
		if (!payload.logFile.IsEmpty())
		{
			g_logName.Append(payload.logFile);
			HANDLE debugFile = CreateFileW(payload.logFile.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			#if UBA_DEBUG_LOG_ENABLED
			g_debugFile = (FileHandle)(u64)debugFile;
			#else
			if (debugFile != INVALID_HANDLE_VALUE)
			{
				const char str[] = "Run in debug to get this file populated";
				DWORD written = 0;
				WriteFile(debugFile, str, sizeof(str), &written, NULL);
				CloseHandle(debugFile);
			}
			#endif
		}
	}

	if (g_runningRemote)
	{
		ULONG languageCount = 1;
		wchar_t languageBuffer[6];
		swprintf_s(languageBuffer, 6, L"%04x", g_uiLanguage);
		languageBuffer[5] = 0;
		if (!SetProcessPreferredUILanguages(MUI_LANGUAGE_ID, languageBuffer, &languageCount))
		{
			DEBUG_LOG(L"Failed to set locale");
		}
	}

	{
		wchar_t exeFullName[256];
		if (!GetModuleFileNameW(NULL, exeFullName, sizeof_array(exeFullName)))
			FatalError(1350, L"GetModuleFileNameW failed (%u)", GetLastError());
		wchar_t* lastSlash = wcsrchr(exeFullName, '\\');
		*lastSlash = 0;
		FixPath(g_exeDir, exeFullName);
		g_exeDir.Append('\\');
	}

	// Special cl.exe handling..  this is needed for compiles using pch files where this address _must_ be available.
	if (payload.rulesIndex == SpecialRulesIndex_ClExe)
	{
		g_clExeBaseReservedMemory = VirtualAlloc((void*)g_clExeBaseAddress, g_clExeBaseAddressSize, MEM_RESERVE, PAGE_READWRITE);
		DEBUG_LOG(L"Reserving %llu bytes at 0x%llx for cl.exe", g_clExeBaseAddressSize, g_clExeBaseAddress);
		if (!g_clExeBaseReservedMemory)
			FatalError(1349, L"Failed to reserve memory for cl.exe (%u)", GetLastError());
	}

	if (const tchar* const* preloads = g_rules->LibrariesToPreload())
		for (auto it = preloads; *it; ++it)
			if (!LoadLibraryExW(*it, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32))
				FatalError(1351, L"Failed to preload %s (%u)", *it, GetLastError());
}

void Init(const DetoursPayload& payload, u64 startTime)
{
	AddExceptionHandler();

	DetourAttachFunctions(g_runningRemote);

	if (!g_isDetachedProcess)
	{
		// If GetStdHandle returns 0 it is likely that there is a parent process and that one has detached process set (which means now conhost is created)
		// .. solve this by detaching this process too
		HANDLE stdoutHandle = True_GetStdHandle(STD_OUTPUT_HANDLE);
		if (stdoutHandle == 0)
		{
			g_isDetachedProcess = true;
		}
		else
		{
			HANDLE stderrHandle = True_GetStdHandle(STD_ERROR_HANDLE);
			g_stdHandle[0] = GetFileType(stderrHandle) == FILE_TYPE_CHAR ? stderrHandle : 0;
			g_stdHandle[1] = GetFileType(stdoutHandle) == FILE_TYPE_CHAR ? stdoutHandle : 0;
		}
	}

	if (g_isDetachedProcess)
	{
		g_stdHandle[0] = makeDetouredHandle(new DetouredHandle(HandleType_StdErr)); // STD_ERR
		g_stdHandle[1] = makeDetouredHandle(new DetouredHandle(HandleType_StdOut)); // STD_OUT
		g_stdHandle[2] = makeDetouredHandle(new DetouredHandle(HandleType_StdIn)); // STD_IN
	}

	if (payload.trackInputs)
		g_trackInputsMem = (u8*)g_memoryBlock.Allocate(TrackInputsMemCapacity, 1, L"TrackInputs");
	
	g_systemRoot.count = GetEnvironmentVariableW(L"SystemRoot", g_systemRoot.data, g_systemRoot.capacity);
	g_systemRoot.MakeLower();

	wchar_t systemTemp[256];
	GetEnvironmentVariableW(L"TEMP", systemTemp, 256);
	FixPath(g_systemTemp, systemTemp);

	StringBuffer<512> applicationBuffer;
	StringBuffer<512> workingDirBuffer;

	HANDLE directoryTableHandle;
	u32 directoryTableSize;
	u32 directoryTableCount;
	HANDLE mappedFileTableHandle;
	u32 mappedFileTableSize;
	u32 mappedFileTableCount;

	{
		RPC_MESSAGE(Init, init)
		writer.Flush();
		BinaryReader reader;

		g_processId = reader.ReadU32();
		g_echoOn = reader.ReadBool();
		g_isChild = reader.ReadBool();

		reader.ReadString(applicationBuffer);
		reader.ReadString(workingDirBuffer);

		directoryTableHandle = FileMappingHandle::FromU64(reader.ReadU64()).mh;
		directoryTableSize = reader.ReadU32();
		directoryTableCount = reader.ReadU32();
		mappedFileTableHandle = FileMappingHandle::FromU64(reader.ReadU64()).mh;
		mappedFileTableSize = reader.ReadU32();
		mappedFileTableCount = reader.ReadU32();

		if (u16 vfsSize = reader.ReadU16())
		{
			BinaryReader vfsReader(reader.GetPositionData(), 0, vfsSize);
			PopulateVfs(vfsReader);
		}
		DEBUG_LOG_PIPE(L"Init", L"");
	}

	TrackInput(applicationBuffer.data);

	VirtualizePath(applicationBuffer);
	VirtualizePath(workingDirBuffer);
	VirtualizePath(g_exeDir);

	Shared_SetCurrentDirectory(workingDirBuffer.data);

	{
		FixPath(applicationBuffer.data, g_virtualWorkingDir.data, g_virtualWorkingDir.count, g_virtualApplication);

		if (const wchar_t* lastBackslash = g_virtualApplication.Last('\\'))
			g_virtualApplicationDir.Append(g_virtualApplication.data, (lastBackslash + 1 - g_virtualApplication.data));
		else
			FatalError(4444, L"What the heck: %s (%s)", g_virtualApplication.data, applicationBuffer.data);
	}

	#if UBA_DEBUG_LOG_ENABLED
	if (isLogging())
	{
		StringView sv(ToView(True_GetCommandLineW()));
		LogHeader(sv);
		LogVfsInfo();
	}
	#endif

	if (!True_DuplicateHandle(g_hostProcess, mappedFileTableHandle, GetCurrentProcess(), &mappedFileTableHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
		UBA_ASSERTF(false, L"Failed to duplicate filetable handle (%u)", GetLastError());

	u8* mappedFileTableMem;
	{
		TimerScope ts(g_kernelStats.mapViewOfFile);
		mappedFileTableMem = (u8*)True_MapViewOfFile(mappedFileTableHandle, FILE_MAP_READ, 0, 0, 0);
		UBA_ASSERT(mappedFileTableMem);
	}
	{
		TimerScope ts2(g_stats.fileTable);
		g_mappedFileTable.Init(mappedFileTableMem, mappedFileTableCount, mappedFileTableSize);
	}

	if (!True_DuplicateHandle(g_hostProcess, directoryTableHandle, GetCurrentProcess(), &directoryTableHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
		UBA_ASSERTF(false, L"Failed to duplicate directorytable handle (%u)", GetLastError());

	u8* directoryTableMem;
	{
		TimerScope ts(g_kernelStats.mapViewOfFile);
		directoryTableMem = (u8*)True_MapViewOfFile(directoryTableHandle, FILE_MAP_READ, 0, 0, 0);
		UBA_ASSERT(directoryTableMem);
	}
	{
		TimerScope ts2(g_stats.dirTable);
		g_directoryTable.Init(directoryTableMem, directoryTableCount, directoryTableSize);
	}

	if (g_isChild)
		Rpc_GetWrittenFiles();

	g_stats.attach.time += GetTime() - startTime;
	g_stats.attach.count = 1;

	g_filesCouldBeCompressed = payload.readIntermediateFilesCompressed && g_rules->CanDependOnCompressedFiles();
}

void Deinit(u64 startTime)
{
	if (g_isRunningWine) // mt.exe etc fails if detaching is not done during shutdown
	{
		DetourTransactionBegin();
		DetourDetachFunctions();
		::DetourTransactionCommit(); // Ignore errors
	}

	#if defined(UBA_PROFILE_DETOURED_CALLS)
	#define DETOURED_FUNCTION(name) if (timer##name.count != 0) { char sb[1024]; sprintf_s(sb, sizeof(sb), "%s: %u %llu\n", #name, timer##name.count.load(), TimeToMs(timer##name.time.load())); WriteDebug(sb); }
	DETOURED_FUNCTIONS
	DETOURED_FUNCTIONS_MEMORY
	DETOURED_FUNCTIONS_MEMORY_NON_WINE
	#undef DETOURED_FUNCTION
	#endif

	DWORD exitCode = STILL_ACTIVE;
	if (!True_GetExitCodeProcess(GetCurrentProcess(), &exitCode))
		exitCode = STILL_ACTIVE;

	if (!g_exitMessageSent)
		SendExitMessage(exitCode, startTime); // This should never happen? ExitProcess is always called after main function
}

void PostDeinit()
{
	DEBUG_LOG(L"Finished");
	#if UBA_DEBUG_LOG_ENABLED
	if (isLogging())
	{
		FlushDebugLog();
		HANDLE debugFile = (HANDLE)g_debugFile;
		g_debugFile = InvalidFileHandle;
		CloseHandle(debugFile);
	}
	#endif
}

} // namespace uba

extern "C"
{
	using namespace uba;

	UBA_DETOURED_API u32 UbaSendCustomMessage(const void* send, u32 sendSize, void* recv, u32 recvCapacity)
	{
		RPC_MESSAGE(Custom, log)
		writer.WriteU32(sendSize);
		writer.WriteBytes(send, sendSize);
		writer.Flush();
		BinaryReader reader;
		u32 recvSize = reader.ReadU32();
		UBA_ASSERT(recvSize < recvCapacity);
		reader.ReadBytes(recv, recvSize);
		return recvSize;
	}

	UBA_DETOURED_API bool UbaFlushWrittenFiles()
	{
		RPC_MESSAGE(FlushWrittenFiles, log)
		writer.Flush();
		BinaryReader reader;
		return reader.ReadBool();
	}

	UBA_DETOURED_API bool UbaUpdateEnvironment(const wchar_t* reason, bool resetStats)
	{
		{
			RPC_MESSAGE(UpdateEnvironment, log)
			writer.WriteString(reason ? reason : L"");
			writer.WriteBool(resetStats);
			writer.Flush();
			BinaryReader reader;
			if (!reader.ReadBool())
				return false;
		}
		Rpc_UpdateTables();
		return true;
	}

	UBA_DETOURED_API bool UbaRunningRemote()
	{
		return g_runningRemote;
	}

	UBA_DETOURED_API bool UbaRequestNextProcess(u32 prevExitCode, wchar_t* outArguments, u32 outArgumentsCapacity)
	{
		#if UBA_DEBUG_LOG_ENABLED
		FlushDebugLog();
		#endif

		*outArguments = 0;
		bool newProcess;
		{
			RPC_MESSAGE(GetNextProcess, log)
			writer.WriteU32(prevExitCode);
			g_stats.Write(writer);
			g_kernelStats.Write(writer);

			writer.Flush();
			BinaryReader reader;
			newProcess = reader.ReadBool();
			if (newProcess)
			{
				reader.ReadString(outArguments, outArgumentsCapacity);
				reader.SkipString(); // workingDir
				reader.SkipString(); // description
				reader.ReadString(g_logName.Clear());
			}
		}

		if (newProcess)
		{
			g_kernelStats = {};
			g_stats = {};

			#if UBA_DEBUG_LOG_ENABLED
			SuppressCreateFileDetourScope scope;
			HANDLE debugFile = (HANDLE)g_debugFile;
			g_debugFile = InvalidFileHandle;
			CloseHandle(debugFile);
			debugFile = CreateFileW(g_logName.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			g_debugFile = (FileHandle)(u64)debugFile;
			LogHeader(ToView(outArguments));
			#endif
		}

		Rpc_UpdateTables();
		return newProcess;
	}
}
