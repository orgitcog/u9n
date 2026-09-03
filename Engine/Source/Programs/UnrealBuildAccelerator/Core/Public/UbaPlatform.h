// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"

#if PLATFORM_WINDOWS
	#define WINDOWS_LEAN_AND_MEAN
	#define NOMINMAX
	#include <ntstatus.h>
	#define WIN32_NO_STATUS
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#include <ws2tcpip.h>
	#include <windows.h>

	#if defined( __clang_analyzer__ )
	#define ANALYSIS_NORETURN __attribute__((analyzer_noreturn))
	#else
	#define ANALYSIS_NORETURN __analysis_noreturn
	#endif
#else
	#include <fcntl.h>
	#include <wchar.h>
	#include <stdio.h>
	#include <stdarg.h>
	#include <math.h>
	#include <string.h>
	#include <sys/mman.h>
	#include <sys/syscall.h>
	#include <sys/socket.h>
	#include <sys/stat.h>
	#include <sys/wait.h>
	#include <unistd.h>
	#include <spawn.h>
	#include <signal.h>

	#if PLATFORM_MAC
	#include <sys/random.h>
    #include <sys/posix_shm.h>
	#include  <mach-o/dyld.h>
	#else
	#include <linux/random.h>
	#include <time.h>
	#include <termios.h>
	#include <stropts.h>
	#include <sys/select.h>
	#include <sys/ioctl.h>
	#endif

	#define ANALYSIS_NORETURN // __attribute__((analyzer_noreturn))
#endif

#define UBA_EXPERIMENTAL 0

#if UBA_DEBUG && defined(_MSC_VER)
	#define UBA_FMT_CHECK(Format, ...) do { if (false) { wprintf(Format, ##__VA_ARGS__); } } while(false)
#else
	#define UBA_FMT_CHECK(Format, ...)
#endif

namespace uba
{
	class StringBufferBase;
	struct BinaryReader;
	struct BinaryWriter;

	#ifndef sizeof_array
	#define	sizeof_array(array) int(sizeof(array)/sizeof(array[0]))
	#endif

	#if UBA_DEBUG
	#define UBA_ASSERT(x) do { if (x) break; uba::UbaAssert(TC(""), __FILE__, __LINE__, #x, true, 543221, nullptr, 0); } while(false)
	#define UBA_ASSERTF(x, fmt, ...) do { UBA_FMT_CHECK(fmt, ##__VA_ARGS__); if (x) break; uba::StringBuffer<> _buf; _buf.Appendf(fmt, ##__VA_ARGS__); uba::UbaAssert(_buf.data, __FILE__, __LINE__, #x, true, 543221, nullptr, 0); } while(false)
	#define UBA_ASSERT_MESSAGEBOX 0
	inline constexpr bool IsDebug = true;
	#else
	#define UBA_ASSERT(x) do { } while(false)
	#define UBA_ASSERTF(x, ...) do { } while(false)
	#define UBA_ASSERT_MESSAGEBOX 0
	inline constexpr bool IsDebug = false;
	#endif

	#define UBA_NOT_IMPLEMENTED(x) UBA_ASSERTF(false, TC("%s not implemented!"), #x);

	u32 GetCallstack(void** outCallstack, u32 callstackCapacity, u32 skipCallstack, void* contextPtr);
	bool WriteCallstackInfo(BinaryWriter& out, u32 skipCallstack, void* context);
	bool WriteCallstackInfo(BinaryWriter& out, void** callstack, u32 callstackCount);
	void WriteAssertInfo(StringBufferBase& out, const tchar* text, const char* file, u32 line, const char* expr, void* context = nullptr);
	ANALYSIS_NORETURN void UbaAssert(const tchar* text, const char* file, u32 line, const char* expr, bool allowTerminate, u32 terminateCode, void* context, u32 skipCallstackCount);
	ANALYSIS_NORETURN void FatalError(u32 code, const tchar* format, ...);
	using CustomAssertHandler = void(const tchar* text);
	void SetCustomAssertHandler(CustomAssertHandler* handler);
	bool CreateGuid(Guid& out);
	const char* GetWineVersion();
	bool IsRunningWine();
	bool IsRunningArm();
	bool IsEscapePressed();
	void Sleep(u32 milliseconds);
	u32 GetUserDefaultUILanguage();
	u32 GetLastError();
	void SetLastError(u32 error);
	bool GetComputerNameW(StringBufferBase& out);
	bool GetOsVersion(StringBufferBase& outPretty, u32& outValue);
	u32 GetCurrentProcessId();

	enum FileHandle : u64 {};
	inline constexpr FileHandle InvalidFileHandle = (FileHandle)-1;

	enum MutexHandle : u64 {};
	inline constexpr MutexHandle InvalidMutexHandle = MutexHandle(0);
	MutexHandle CreateMutexW(bool bInitialOwner, const tchar* name);
	void ReleaseMutex(MutexHandle mutex);
	void CloseMutex(MutexHandle mutex);

	enum ProcHandle : u64 {};
	inline constexpr ProcHandle InvalidProcHandle = ((ProcHandle)(u64)-1);
	ProcHandle GetCurrentProcessHandle();

	u32 GetEnvironmentVariableW(const tchar* name, tchar* buffer, u32 nSize);
	bool SetEnvironmentVariableW(const tchar* name, const tchar* value);
	u32 ExpandEnvironmentStringsW(const tchar* lpSrc, tchar* lpDst, u32 nSize);
	u32 GetLogicalProcessorCount();
	u32 GetProcessorGroupCount();

	void ElevateCurrentThreadPriority();
	void PrefetchVirtualMemory(const void* mem, u64 size);
	void AddExceptionHandler();
	void InitMemory();

#if PLATFORM_WINDOWS
	inline constexpr bool CaseInsensitiveFs = true;
	inline constexpr tchar PathSeparator = '\\';
	inline constexpr tchar NonPathSeparator = '/';
	inline constexpr u32 MaxPath = 1024;
	inline DWORD ToLow(u64 v) { LARGE_INTEGER li; li.QuadPart = (LONGLONG)v; return li.LowPart; }
	inline LONG ToHigh(u64 v) { LARGE_INTEGER li; li.QuadPart = (LONGLONG)v; return li.HighPart; }
	inline LARGE_INTEGER ToLargeInteger(u64 v) { LARGE_INTEGER li; li.QuadPart = (LONGLONG)v; return li; }
	inline LARGE_INTEGER ToLargeInteger(u32 high, u32 low) { LARGE_INTEGER li; li.HighPart = (LONG)high; li.LowPart = low; return li; }
	inline bool IsDirectory(u32 attributes) { return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
	inline bool IsExecutable(u32 attributes) { return false; }
	inline bool IsUncPath(const tchar* path) { return path[0] == '\\' && path[1] == '\\'; }
	#define TStrlen(s) u32(wcslen(s))
	#define TStrchr(a, b) wcschr(a, b)
	#define TStrrchr(a, b) wcsrchr(a, b)
	#define TStrstr(a, b) wcsstr(a, b)
	#define TStrcmp(a, b) wcscmp(a, b)
	#define TSprintf_s swprintf_s
	#define Tvsprintf_s(...) vswprintf_s(__VA_ARGS__)
	#define TStrcpy_s wcscpy_s
	#define TStrcat_s wcscat_s
	#define TStrdup _wcsdup
	#define TFputs fputws
	#define UBA_NOINLINE __declspec(noinline)
	struct FileMappingHandle
	{
		bool operator==(const FileMappingHandle& o) const { return mh == o.mh; }
		bool IsValid() const { return mh != 0; }
		u64 ToU64() const { return u64(mh) | (u64(fh) << 32); }
		static FileMappingHandle FromU64(u64 v) { return { (HANDLE)(v >> 32), (HANDLE)(v & ~0u) }; }
		HANDLE fh = 0;
		HANDLE mh = 0;
	};
#else
	inline constexpr tchar PathSeparator = '/';
	inline constexpr tchar NonPathSeparator = '\\';
	inline constexpr u32 MaxPath = 1024;
	#define TStrlen(s) u32(strlen(s))
	#define TStrchr(a, b) strchr(a, b)
	#define TStrrchr(a, b) strrchr(a, b)
	#define TStrstr(a, b) strstr(a, b)
	#define TStrcmp(a, b) strcmp(a, b)
	#define TSprintf_s(...) snprintf(__VA_ARGS__)
	#define Tvsprintf_s(a, b, c, d) vsnprintf(a, b, c, d)
	#define TStrcpy_s(a, b, c) strcpy_s(a, b, c)
	#define TStrcat_s(a, b, c) strcat_s(a, b, c)
	#define TStrdup strdup
	#define TFputs fputs
	#define localtime_s(a,b) localtime_r(b,a)
	void strcpy_s(tchar* dest, u64 destCapacity, const tchar* source);
	void strcat_s(tchar* dest, u64 destCapacity, const tchar* source);
	void GetMappingHandleName(StringBufferBase& out, u64 uid);
	bool GetPhysicallyInstalledSystemMemory(u64& outKb);
	inline u64 FromTimeSpec(const timespec& ts) { return u64(ts.tv_sec) * 10'000'000ull + u64(ts.tv_nsec/100); }
	inline timespec ToTimeSpec(u64 time) { timespec ts; ts.tv_sec = time / 10'000'000ull; ts.tv_nsec = (time - (u64(ts.tv_sec) * 10'000'000ull)) * 100; return ts; }
	inline bool IsDirectory(u32 attributes) { return S_ISDIR(attributes); }
	inline bool IsExecutable(u32 attributes) { return (attributes & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0; }
	inline bool IsUncPath(const tchar* path) { return false; }
	#if PLATFORM_LINUX
	inline constexpr bool CaseInsensitiveFs = false;
	#define st_mtimespec st_mtim
	#else
	inline constexpr bool CaseInsensitiveFs = true;
	#endif
	#define UBA_NOINLINE __attribute__((noinline))
    #define ERROR_SUCCESS 0
	#define DUPLICATE_SAME_ACCESS 0
	struct FileMappingHandle
	{
		FileMappingHandle(int shmFd_ = -1, int lockFd_ = -1, u64 uid_ = ~u64(0)) : shmFd(shmFd_), lockFd(lockFd_), uid(uid_) {};
		FileMappingHandle(const FileMappingHandle& o) { shmFd = o.shmFd; uid = o.uid; lockFd = o.lockFd; }
		bool operator==(const FileMappingHandle& o) const { return shmFd == o.shmFd; }
		bool IsValid() const { return shmFd != -1; }
		u64 ToU64() const { return uid; }
		static FileMappingHandle FromU64(u64 v) { return FileMappingHandle(-1, -1, v); }
		int shmFd;
		int lockFd;
		u64 uid;
	};
#endif // PLATFORM_WINDOWS
}
