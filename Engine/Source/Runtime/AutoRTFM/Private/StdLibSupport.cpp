// Copyright Epic Games, Inc. All Rights Reserved.

#if (defined(__AUTORTFM) && __AUTORTFM)

#include "AutoRTFM.h"
#include "BuildMacros.h"
#include "ContextInlines.h"
#include "Memcpy.h"
#include "Utils.h"

#include <algorithm>
#include <charconv>
#include <float.h>
#include <functional> // note: introduces additional math overloads
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#if AUTORTFM_PLATFORM_WINDOWS
#include "WindowsHeader.h"
#endif

#if AUTORTFM_PLATFORM_LINUX
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if __has_include(<sanitizer/asan_interface.h>)
#	include <sanitizer/asan_interface.h>
#	if defined(__SANITIZE_ADDRESS__)
#		define AUTORTFM_ASAN_ENABLED 1
#	elif defined(__has_feature)
#		if __has_feature(address_sanitizer)
#			define AUTORTFM_ASAN_ENABLED 1
#		endif
#	endif
#endif

#ifndef AUTORTFM_ASAN_ENABLED
#define AUTORTFM_ASAN_ENABLED 0
#endif

#ifdef _MSC_VER
// BEGIN: Disable warning about deprecated STD C functions.
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace AutoRTFM
{

namespace
{

// A helper that opens a FILE to "/dev/null" on first call to Get()
// and automatically closes the file on static destruction.
class FNullFile
{
public:
    static FILE* Get()
    {
        static FNullFile Instance;
        return Instance.File;
    }

private:
    FNullFile() : File(fopen("/dev/null", "wb")) {}
    ~FNullFile() { fclose(File); }
    FILE* const File;
};

void ThrowErrorFormatContainsPercentN()
{
    AUTORTFM_WARN("AutoRTFM does not support format strings containing '%%n'");
    FContext* Context = FContext::Get();
    Context->AbortByLanguageAndThrow();
}

// Throws an error if the format string contains a '%n'.
static void ThrowIfFormatContainsPercentN(const char* Format)
{
    for (const char* P = Format; *P != '\0'; ++P)
    {
        if (*P == '%')
        {
            switch (*++P)
            {
                case 'n':
                    ThrowErrorFormatContainsPercentN();
                    break;
                case '\0':
                    return;
            }
        }
    }
}

// Throws an error if the format string contains a '%n'.
static void ThrowIfFormatContainsPercentN(const wchar_t* Format)
{
    for (const wchar_t* P = Format; *P != L'\0'; ++P)
    {
        if (*P == L'%')
        {
            switch (*++P)
            {
                case L'n':
                    ThrowErrorFormatContainsPercentN();
                    break;
                case L'\0':
                    return;
            }
        }
    }
}

#if AUTORTFM_PLATFORM_WINDOWS
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv AUTORTFM_PLATFORM_WINDOWS vvvvvvvvvvvvvvvvvvvvvvvvvvvvv
template<typename RETURN, typename CHAR, RETURN FN(const CHAR*, CHAR**, _locale_t)>
RETURN RTFM_StringToFloat(const CHAR* String, CHAR** EndPtr, _locale_t Locale)
{
    if (nullptr != EndPtr)
    {
        AutoRTFM::RecordOpenWrite(EndPtr);
    }
    return FN(String, EndPtr, Locale);
}

template<typename RETURN, typename CHAR, RETURN FN(const CHAR*, CHAR**, int)>
RETURN RTFM_StringToIntNoLocale(const CHAR* String, CHAR** EndPtr, int Radix)
{
    if (nullptr != EndPtr)
    {
        AutoRTFM::RecordOpenWrite(EndPtr);
    }
    return FN(String, EndPtr, Radix);
}

template<typename RETURN, typename CHAR, RETURN FN(const CHAR*, CHAR**, int, _locale_t)>
RETURN RTFM_StringToInt(const CHAR* String, CHAR** EndPtr, int Radix, _locale_t Locale)
{
    if (nullptr != EndPtr)
    {
        AutoRTFM::RecordOpenWrite(EndPtr);
    }
    return FN(String, EndPtr, Radix, Locale);
}

FILE* RTFM___acrt_iob_func(unsigned Index)
{
    switch (Index)
    {
    case 1:
    case 2:
        return __acrt_iob_func(Index);
    default:
	{
		AUTORTFM_WARN("Attempt to get file descriptor %d (not 1 or 2) in __acrt_iob_func.", Index);
		FContext* Context = FContext::Get();
        Context->AbortByLanguageAndThrow();
        return NULL;
	}
    }
}

// FIXME: Does not currently support %n format specifiers.
int RTFM___stdio_common_vfprintf(
        unsigned __int64 Options,
        FILE*            Stream,
        char const*      Format,
        _locale_t        Locale,
        va_list          ArgList)
{
    ThrowIfFormatContainsPercentN(Format);

    return __stdio_common_vfprintf(Options, Stream, Format, Locale, ArgList);
}

// FIXME: Does not currently support %n format specifiers.
int RTFM___stdio_common_vsprintf(
        unsigned __int64 Options,
        char*            Buffer,
        size_t           BufferCount,
        char const*      Format,
        _locale_t        Locale,
        va_list          ArgList)
{
    ThrowIfFormatContainsPercentN(Format);

    if (nullptr != Buffer && 0 != BufferCount)
    {
        va_list ArgList2;
        va_copy(ArgList2, ArgList);
        int Count = __stdio_common_vsprintf(Options, nullptr, 0, Format, Locale, ArgList2);
        va_end(ArgList2);

        if (Count >= 0)
        {
            size_t NumBytes = std::min(BufferCount, static_cast<size_t>(1 + Count)) * sizeof(char);
            FContext* Context = FContext::Get();
            Context->RecordWrite(Buffer, NumBytes);
        }
    }

    return __stdio_common_vsprintf(Options, Buffer, BufferCount, Format, Locale, ArgList);
}

// FIXME: Does not currently support %n format specifiers.
int RTFM___stdio_common_vswprintf(
        unsigned __int64 Options,
        wchar_t*         Buffer,
        size_t           BufferCount,
        wchar_t const*   Format,
        _locale_t        Locale,
        va_list          ArgList)
{
    ThrowIfFormatContainsPercentN(Format);

    if (nullptr != Buffer && 0 != BufferCount)
    {
        va_list ArgList2;
        va_copy(ArgList2, ArgList);
        int Count = __stdio_common_vswprintf(Options, nullptr, 0, Format, Locale, ArgList2);
        va_end(ArgList2);

        if (Count >= 0)
        {
            size_t NumBytes = std::min(BufferCount, static_cast<size_t>(1 + Count)) * sizeof(wchar_t);
            FContext* Context = FContext::Get();
            Context->RecordWrite(Buffer, NumBytes);
        }
    }

    return __stdio_common_vswprintf(Options, Buffer, BufferCount, Format, Locale, ArgList);
}

// FIXME: Does not currently support %n format specifiers.
int RTFM___stdio_common_vfwprintf(
        unsigned __int64 Options,
        FILE*            Stream,
        wchar_t const*   Format,
        _locale_t        Locale,
        va_list          ArgList)
{
    ThrowIfFormatContainsPercentN(Format);

    return __stdio_common_vfwprintf(Options, Stream, Format, Locale, ArgList);
}

BOOL RTFM_TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue)
{
	LPVOID CurrentValue = TlsGetValue(dwTlsIndex);

	AutoRTFM::OnAbort([dwTlsIndex, CurrentValue]
	{
		TlsSetValue(dwTlsIndex, CurrentValue);
	});

	return TlsSetValue(dwTlsIndex, lpTlsValue);
}

// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ AUTORTFM_PLATFORM_WINDOWS ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#else
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvv !AUTORTFM_PLATFORM_WINDOWS vvvvvvvvvvvvvvvvvvvvvvvvvvvvv
extern "C" size_t _ZNSt3__112__next_primeEm(size_t N) __attribute__((weak));
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ !AUTORTFM_PLATFORM_WINDOWS ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#endif

#if AUTORTFM_PLATFORM_LINUX
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv AUTORTFM_PLATFORM_LINUX vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
int RTFM_stat(const char* Path, struct stat* StatBuf) throw()
{
	FContext* Context = FContext::Get();
	Context->RecordWrite(StatBuf, sizeof(*StatBuf));

	return stat(Path, StatBuf);
}

int RTFM_fstat(int Fd, struct stat* StatBuf) throw()
{
	FContext* Context = FContext::Get();
	Context->RecordWrite(StatBuf, sizeof(*StatBuf));

	return fstat(Fd, StatBuf);
}

int RTFM___xstat(int Ver, const char* Path, struct stat* StatBuf) throw()
{
	FContext* Context = FContext::Get();
	Context->RecordWrite(StatBuf, sizeof(*StatBuf));

	return __xstat(Ver, Path, StatBuf);
}

int RTFM___fxstat(int Ver, int Fd, struct stat* StatBuf) throw()
{
	FContext* Context = FContext::Get();
	Context->RecordWrite(StatBuf, sizeof(*StatBuf));

	return __fxstat(Ver, Fd, StatBuf);
}
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ AUTORTFM_PLATFORM_LINUX ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#endif

void* RTFM_memcpy(void* Dst, const void* Src, size_t Size) throw()
{
    return Memcpy(Dst, Src, Size, FContext::Get());
}

void* RTFM_memmove(void* Dst, const void* Src, size_t Size) throw()
{
    return Memmove(Dst, Src, Size, FContext::Get());
}

void* RTFM_memset(void* Dst, int Value, size_t Size) throw()
{
    return Memset(Dst, Value, Size, FContext::Get());
}

void* RTFM_malloc(size_t Size) throw()
{
    void* Result = malloc(Size);
	FContext* Context = FContext::Get();
    Context->GetCurrentTransaction()->DeferUntilAbort([Result]
    {
        free(Result);
    });
    Context->DidAllocate(Result, Size);
    return Result;
}

void* RTFM_calloc(size_t Count, size_t Size) throw()
{
    void* Result = calloc(Count, Size);
	FContext* Context = FContext::Get();
    Context->GetCurrentTransaction()->DeferUntilAbort([Result]
    {
        free(Result);
    });
    Context->DidAllocate(Result, Count * Size);
    return Result;
}

void RTFM_free(void* Ptr) throw()
{
    if (Ptr)
    {
		FContext* Context = FContext::Get();
        Context->GetCurrentTransaction()->DeferUntilCommit([Ptr]
        {
            free(Ptr);
        });
    }
}

void* RTFM_realloc(void* Ptr, size_t Size) throw()
{
    void* NewObject = RTFM_malloc(Size);
    if (Ptr)
    {
#if defined(__APPLE__)
		const size_t OldSize = malloc_size(Ptr);
#elif defined(_WIN32)
		const size_t OldSize = _msize(Ptr);
#else
		const size_t OldSize = malloc_usable_size(Ptr);
#endif
		FContext* Context = FContext::Get();
        MemcpyToNew(NewObject, Ptr, std::min(OldSize, Size), Context);
        RTFM_free(Ptr);
    }
    return NewObject;
}

char* RTFM_strcpy(char* const Dst, const char* const Src) throw()
{
    const size_t SrcLen = strlen(Src);

	FContext* Context = FContext::Get();
    Context->RecordWrite(Dst, SrcLen + sizeof(char));
    return strcpy(Dst, Src);
}

char* RTFM_strncpy(char* const Dst, const char* const Src, const size_t Num) throw()
{
	FContext* Context = FContext::Get();
    Context->RecordWrite(Dst, Num);
    return strncpy(Dst, Src, Num);
}

char* RTFM_strcat(char* const Dst, const char* const Src) throw()
{
    const size_t DstLen = strlen(Dst);
    const size_t SrcLen = strlen(Src);

	FContext* Context = FContext::Get();
    Context->RecordWrite(Dst + DstLen, SrcLen + 1);
    return strcat(Dst, Src);
}

char* RTFM_strncat(char* const Dst, const char* const Src, const size_t Num) throw()
{
    const size_t DstLen = strlen(Dst);

	FContext* Context = FContext::Get();
    Context->RecordWrite(Dst + DstLen, Num + 1);
    return strncat(Dst, Src, Num);
}

template<typename RETURN, typename CHAR, RETURN FN(const CHAR*, CHAR**)>
RETURN RTFM_StringToFloat(const CHAR* String, CHAR** EndPtr) throw()
{
    if (nullptr != EndPtr)
    {
        AutoRTFM::RecordOpenWrite(EndPtr);
    }
    return FN(String, EndPtr);
}

template<typename RETURN, typename CHAR, RETURN FN(const CHAR*, CHAR**, int)>
RETURN RTFM_StringToInt(const CHAR* String, CHAR** EndPtr, int Radix) throw()
{
    if (nullptr != EndPtr)
    {
        AutoRTFM::RecordOpenWrite(EndPtr);
    }
    return FN(String, EndPtr, Radix);
}

template<typename T, std::to_chars_result FN(char*, char*, T)>
std::to_chars_result RTFM_ToChars(char* First, char* Last, T Value)
{
    AutoRTFM::RecordOpenWrite(First, Last - First);
    return FN(First, Last, Value);
}

template<typename T, std::to_chars_result FN(char*, char*, T, int)>
std::to_chars_result RTFM_ToChars(char* First, char* Last, T Value, int Base)
{
    AutoRTFM::RecordOpenWrite(First, Last - First);
    return FN(First, Last, Value, Base);
}

template<typename T, std::to_chars_result FN(char*, char*, T, std::chars_format)>
std::to_chars_result RTFM_ToChars(char* First, char* Last, T Value, std::chars_format Format)
{
    AutoRTFM::RecordOpenWrite(First, Last - First);
    return FN(First, Last, Value, Format);
}

template<typename T, std::to_chars_result FN(char*, char*, T, std::chars_format, int Precision)>
std::to_chars_result RTFM_ToChars(char* First, char* Last, T Value, std::chars_format Format, int Precision)
{
    AutoRTFM::RecordOpenWrite(First, Last - First);
    return FN(First, Last, Value, Format, Precision);
}

// FIXME: Does not currently support %n format specifiers.
int RTFM_vsnprintf(char* Str, size_t Size, const char* Format, va_list ArgList) throw()
{
    ThrowIfFormatContainsPercentN(Format);

    if (nullptr != Str && 0 != Size)
    {
        va_list ArgList2;
        va_copy(ArgList2, ArgList);
        int Count = vsnprintf(nullptr, 0, Format, ArgList2);
        va_end(ArgList2);

        if (Count >= 0)
        {
            size_t NumBytes = std::min(Size, static_cast<size_t>(1 + Count)) * sizeof(char);
            FContext* Context = FContext::Get();
            Context->RecordWrite(Str, NumBytes);
        }

    }

    return vsnprintf(Str, Size, Format, ArgList);
}

// FIXME: Does not currently support %n format specifiers.
int RTFM_vswprintf(wchar_t* Str, size_t Size, const wchar_t* Format, va_list ArgList)
{
    ThrowIfFormatContainsPercentN(Format);

    if (nullptr != Str && 0 != Size)
    {
        va_list ArgList2;
        va_copy(ArgList2, ArgList);

#if AUTORTFM_PLATFORM_WINDOWS
        int Count = vswprintf(nullptr, 0, Format, ArgList2);
#else
        // vswprintf(nullptr, 0, ...) will return -1.
        int Count = vfwprintf(FNullFile::Get(), Format, ArgList2);
#endif

        va_end(ArgList2);

        size_t NumChars = std::min(Size, static_cast<size_t>(1 + std::max(Count, 0)));
        size_t NumBytes = NumChars * sizeof(wchar_t);
        if (NumBytes >= 0)
        {
            FContext* Context = FContext::Get();
            Context->RecordWrite(Str, NumBytes);
        }
    }

    return vswprintf(Str, Size, Format, ArgList);
}

// FIXME: Does not currently support %n format specifiers.
int RTFM_swprintf(wchar_t* Buffer, size_t BufferCount, wchar_t const* Format, ...)
{
    va_list ArgList;

    va_start(ArgList, Format);
    int Count = RTFM_vswprintf(Buffer, BufferCount, Format, ArgList);
    va_end(ArgList);

    return Count;
}

// FIXME: Does not currently support %n format specifiers.
int RTFM_snprintf(char* Str, size_t Size, const char* Format, ...) throw()
{
    va_list ArgList;

    va_start(ArgList, Format);
    int Count = RTFM_vsnprintf(Str, Size, Format, ArgList);
    va_end(ArgList);

    return Count;
}

// FIXME: Does not currently support %n format specifiers.
int RTFM_printf(const char* Format, ...)
{
    ThrowIfFormatContainsPercentN(Format);

    va_list ArgList;
    va_start(ArgList, Format);
    int Result = vprintf(Format, ArgList);
    va_end(ArgList);
    return Result;
}

// FIXME: Does not currently support %n format specifiers.
int RTFM_wprintf(const wchar_t* Format, ...)
{
    ThrowIfFormatContainsPercentN(Format);

    va_list ArgList;
    va_start(ArgList, Format);
    int Result = vwprintf(Format, ArgList);
    va_end(ArgList);
    return Result;
}

wchar_t* RTFM_wcscpy(wchar_t* Dst, const wchar_t* Src) throw()
{
    const size_t SrcLen = wcslen(Src);

	FContext* Context = FContext::Get();
    Context->RecordWrite(Dst, (SrcLen + 1) * sizeof(wchar_t));
    return wcscpy(Dst, Src);
}

wchar_t* RTFM_wcsncpy(wchar_t* Dst, const wchar_t* Src, size_t Count) throw()
{
	FContext* Context = FContext::Get();
	Context->RecordWrite(Dst, Count * sizeof(wchar_t));
	return wcsncpy(Dst, Src, Count);
}

int RTFM_atexit(void(*Callback)(void)) throw()
{
	FContext* Context = FContext::Get();
	Context->GetCurrentTransaction()->DeferUntilCommit([Callback]
		{
			atexit(Callback);
		});

	return 0;
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// Register all the open -> closed functions
////////////////////////////////////////////////////////////////////////////////
#if AUTORTFM_PLATFORM_LINUX
// LibCxx's string.h uses builtin string functions inside inline functions.
// We can't take the address of these builtins, but we know they map to these C functions.
extern "C"
{
	char* strchr(const char*, int);
	char* strrchr(const char*, int);
	char* strstr(const char*, const char*);
}
#endif

UE_AUTORTFM_REGISTER_OPEN_TO_CLOSED_FUNCTIONS(
#if AUTORTFM_ASAN_ENABLED
	UE_AUTORTFM_MAP_OPEN_TO_SELF(__asan_addr_is_in_fake_stack),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(__asan_get_current_fake_stack),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(__asan_poison_memory_region),
#endif

#if AUTORTFM_PLATFORM_WINDOWS
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_strtoi64, (RTFM_StringToIntNoLocale<long long, char, _strtoi64>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstoi64, (RTFM_StringToInt<long long, wchar_t, _wcstoi64>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstoui64, (RTFM_StringToInt<unsigned long long, wchar_t, _wcstoui64>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstod_l, (RTFM_StringToFloat<double, wchar_t, _wcstod_l>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstof_l, (RTFM_StringToFloat<float, wchar_t, _wcstof_l>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstold_l, (RTFM_StringToFloat<long double, wchar_t, _wcstold_l>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstol_l, (RTFM_StringToInt<long, wchar_t, _wcstol_l>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstoll_l, (RTFM_StringToInt<long long, wchar_t, _wcstoll_l>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstoul_l, (RTFM_StringToInt<unsigned long, wchar_t, _wcstoul_l>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstoull_l, (RTFM_StringToInt<unsigned long long, wchar_t, _wcstoull_l>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstoi64_l, (RTFM_StringToInt<long long, wchar_t, _wcstoi64_l>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(_wcstoui64_l, (RTFM_StringToInt<unsigned long long, wchar_t, _wcstoui64_l>)),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtof),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtof),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtol),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtoll),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtoi),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtoi64),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtof_l),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtol_l),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtoll_l),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtoi_l),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_wtoi64_l),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(__acrt_iob_func, RTFM___acrt_iob_func),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(__stdio_common_vfprintf, RTFM___stdio_common_vfprintf),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(__stdio_common_vsprintf, RTFM___stdio_common_vsprintf),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(__stdio_common_vswprintf, RTFM___stdio_common_vswprintf),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(__stdio_common_vfwprintf, RTFM___stdio_common_vfwprintf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_tcsncmp),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_tcslen),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_tcsnlen),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_isnan),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_fdtest),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_dtest),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_ldtest),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_finite),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(IsDebuggerPresent),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(GetSystemTime),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(QueryPerformanceCounter),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(QueryPerformanceFrequency),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(GetCurrentThreadId),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(GetCurrentProcessId),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(TlsGetValue),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(GetLocalTime),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(GetFileAttributesW),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(TlsSetValue, RTFM_TlsSetValue),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_Query_perf_frequency),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_Query_perf_counter),
#else
	UE_AUTORTFM_MAP_OPEN_TO_SELF(_ZNSt3__112__next_primeEm),
#endif

#if AUTORTFM_PLATFORM_LINUX
	UE_AUTORTFM_MAP_OPEN_TO_SELF(getpid),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(clock_gettime),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(gettimeofday),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(gmtime_r),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(localtime_r),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(bcmp),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(pthread_getspecific),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(pthread_self),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strtof32, (RTFM_StringToFloat<float, char, strtof32>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strtof64, (RTFM_StringToFloat<double, char, strtof64>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(stat, RTFM_stat),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(fstat, RTFM_fstat),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(__xstat, RTFM___xstat),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(__fxstat, RTFM___fxstat),
#endif

// Linux requires mapping of the C functions, which do not use const on their return pointers.
// See comment about LibCxx builtin functions above.
#if AUTORTFM_PLATFORM_LINUX
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(char* (const char*, int), strchr),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(char* (const char*, int), strrchr),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(char* (const char*, const char*), strstr),
#else
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(const char* (const char*, int), strchr),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(const char* (const char*, int), strrchr),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(const char* (const char*, const char*), strstr),
#endif

	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(memcpy, RTFM_memcpy),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(memmove, RTFM_memmove),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(memset, RTFM_memset),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(malloc, RTFM_malloc),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(calloc, RTFM_calloc),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(free, RTFM_free),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(realloc, RTFM_realloc),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strcpy, RTFM_strcpy),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strncpy, RTFM_strncpy),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strcat, RTFM_strcat),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strncat, RTFM_strncat),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(memcmp),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(strcmp),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(strncmp),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(strlen),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(atof),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(atoi),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(const wchar_t* (const wchar_t*, wchar_t), wcschr),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(wchar_t* (wchar_t*, wchar_t), wcschr),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(wchar_t* (wchar_t*, const wchar_t*), wcsstr),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(const wchar_t*(const wchar_t*, const wchar_t*), wcsstr),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(wchar_t*(wchar_t*, wchar_t), wcsrchr),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(const wchar_t*(const wchar_t*, wchar_t), wcsrchr),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(wcscmp),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(wcslen),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strtol, (RTFM_StringToInt<long, char, strtol>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strtoll, (RTFM_StringToInt<long long, char, strtoll>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strtoul, (RTFM_StringToInt<unsigned long, char, strtoul>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strtoull, (RTFM_StringToInt<unsigned long long, char, strtoull>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strtof, (RTFM_StringToFloat<float, char, strtof>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(strtod, (RTFM_StringToFloat<double, char, strtod>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(wcstod, (RTFM_StringToFloat<double, wchar_t, wcstod>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(wcstof, (RTFM_StringToFloat<float, wchar_t, wcstof>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(wcstold, (RTFM_StringToFloat<long double, wchar_t, wcstold>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(wcstol, (RTFM_StringToInt<long, wchar_t, wcstol>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(wcstoll, (RTFM_StringToInt<long long, wchar_t, wcstoll>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(wcstoul, (RTFM_StringToInt<unsigned long, wchar_t, wcstoul>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(wcstoull, (RTFM_StringToInt<unsigned long long, wchar_t, wcstoull>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, int Value, int Base),
		std::to_chars,
		(RTFM_ToChars<int, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, unsigned int Value, int Base),
		std::to_chars,
		(RTFM_ToChars<unsigned int, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, int8_t Value, int Base),
		std::to_chars,
		(RTFM_ToChars<int8_t, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, uint8_t Value, int Base),
		std::to_chars,
		(RTFM_ToChars<uint8_t, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, int16_t Value, int Base),
		std::to_chars,
		(RTFM_ToChars<int16_t, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, uint16_t Value, int Base),
		std::to_chars,
		(RTFM_ToChars<uint16_t, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, int32_t Value, int Base),
		std::to_chars,
		(RTFM_ToChars<int32_t, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, uint32_t Value, int Base),
		std::to_chars,
		(RTFM_ToChars<uint32_t, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, int64_t Value, int Base),
		std::to_chars,
		(RTFM_ToChars<int64_t, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, uint64_t Value, int Base),
		std::to_chars,
		(RTFM_ToChars<uint64_t, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, float Value),
		std::to_chars,
		(RTFM_ToChars<float, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, float Value, std::chars_format Format),
		std::to_chars,
		(RTFM_ToChars<float, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, float Value, std::chars_format Format, int Precision),
		std::to_chars,
		(RTFM_ToChars<float, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, double Value),
		std::to_chars,
		(RTFM_ToChars<double, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, double Value, std::chars_format Format),
		std::to_chars,
		(RTFM_ToChars<double, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, double Value, std::chars_format Format, int Precision),
		std::to_chars,
		(RTFM_ToChars<double, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, long double Value),
		std::to_chars,
		(RTFM_ToChars<long double, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, long double Value, std::chars_format Format),
		std::to_chars,
		(RTFM_ToChars<long double, std::to_chars>)),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		std::to_chars_result(char* First, char* Last, long double Value, std::chars_format Format, int Precision),
		std::to_chars,
		(RTFM_ToChars<long double, std::to_chars>)),

	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswupper),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswlower),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswalpha),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswgraph),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswprint),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswpunct),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswalnum),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswdigit),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswxdigit),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswspace),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(iswcntrl),

	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), sqrt),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), sqrt),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), sqrt),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), sin),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), sin),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), sin),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), cos),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), cos),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), cos),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), tan),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), tan),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), tan),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), asin),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), asin),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), asin),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), acos),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), acos),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), acos),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), atan),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), atan),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), atan),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float, float), atan2),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double, double), atan2),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double, long double), atan2),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), sinh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), sinh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), sinh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), cosh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), cosh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), cosh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), tanh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), tanh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), tanh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), asinh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), asinh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), asinh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), acosh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), acosh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), acosh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), atanh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), atanh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), atanh),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), exp),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), exp),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), exp),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float), log),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double), log),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double), log),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float, float), pow),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double, double), pow),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double, long double), pow),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long long (float), llrint),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long long (double), llrint),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long long (long double), llrint),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float, float), fmod),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double, double), fmod),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double, long double), fmod),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float, float*), modf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double, double*), modf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(long double (long double, long double*), modf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(float (float, float), powf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF_OVERLOADED(double (double, double), pow),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(sqrtf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(sinf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(cosf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(tanf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(asinf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(asinhf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(acosf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(acoshf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(atanf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(atanhf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(atan2f),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(sinhf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(coshf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(tanhf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(expf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(logf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(powf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(llrintf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(fmodf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(fmodl),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(rand),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(modff),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(modfl),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(vsnprintf, RTFM_vsnprintf),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		int (wchar_t*, size_t, const wchar_t*, va_list),
		vswprintf, RTFM_vswprintf),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED_OVERLOADED(
		int(wchar_t*, size_t, wchar_t const*, ...),
		swprintf, RTFM_swprintf),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(snprintf, RTFM_snprintf),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(printf, RTFM_printf),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(wprintf, RTFM_wprintf),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(putchar),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(puts),
	UE_AUTORTFM_MAP_OPEN_TO_SELF(fflush),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(wcscpy, RTFM_wcscpy),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(wcsncpy, RTFM_wcsncpy),
	UE_AUTORTFM_MAP_OPEN_TO_CLOSED(atexit, RTFM_atexit)
);

#ifdef _MSC_VER
#pragma warning(pop)
// END: Disable warning about deprecated STD C functions.
#endif

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
