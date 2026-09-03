// Copyright Epic Games, Inc. All Rights Reserved.

#if (defined(__AUTORTFM) && __AUTORTFM)

#include "AutoRTFM.h"
#include "BuildMacros.h"
#include "ContextInlines.h"
#include "ExternAPI.h"
#include "FunctionMapInlines.h"
#include "Memcpy.h"

namespace AutoRTFM
{

void AbortDueToBadAlignment(FContext* Context, void* Ptr, size_t Alignment, const char* Message = nullptr)
{
    Context->DumpState();
    fprintf(stderr, "Aborting because alignment error: expected alignment %zu, got pointer %p.\n", Alignment, Ptr);
    if (Message)
    {
        fprintf(stderr, "%s\n", Message);
    }
    abort();
}

void CheckAlignment(FContext* Context, void* Ptr, size_t AlignmentMask)
{
    if (reinterpret_cast<uintptr_t>(Ptr) & AlignmentMask)
    {
        AbortDueToBadAlignment(Context, Ptr, AlignmentMask + 1);
    }
}

#if !AUTORTFM_BUILD_SHIPPING

// Check for writes to null in development code, so that the inevitable crash will occur
// in the caller's code rather than in the AutoRTFM runtime.
#define UE_AUTORTFM_HANDLE_NULL_WRITE(Ptr)     \
    do                                         \
    {                                          \
        if (AUTORTFM_UNLIKELY(Ptr == nullptr)) \
        {                                      \
            return;                            \
        }                                      \
    }                                          \
    while (0)

#else

// In shipping code, we don't want to spend any cycles on a redundant check.
// We do want the compiler to optimize as if the pointer is non-null, though.
#define UE_AUTORTFM_HANDLE_NULL_WRITE(Ptr)  \
    UE_ASSUME(Ptr != nullptr)

#endif

extern "C" UE_AUTORTFM_API AUTORTFM_NO_ASAN void autortfm_record_write(void* Ptr, size_t Size)
{
	UE_AUTORTFM_HANDLE_NULL_WRITE(Ptr);
	FContext* Context = FContext::Get();
	Context->RecordWrite(Ptr, Size);
}

extern "C" UE_AUTORTFM_API AUTORTFM_NO_ASAN void autortfm_record_write_1(void* Ptr)
{
	UE_AUTORTFM_HANDLE_NULL_WRITE(Ptr);
	FContext* Context = FContext::Get();
	Context->RecordWrite<1>(Ptr);
}

extern "C" UE_AUTORTFM_API AUTORTFM_NO_ASAN void autortfm_record_write_2(void* Ptr)
{
	UE_AUTORTFM_HANDLE_NULL_WRITE(Ptr);
	FContext* Context = FContext::Get();
	Context->RecordWrite<2>(Ptr);
}

extern "C" UE_AUTORTFM_API AUTORTFM_NO_ASAN void autortfm_record_write_4(void* Ptr)
{
	UE_AUTORTFM_HANDLE_NULL_WRITE(Ptr);
	FContext* Context = FContext::Get();
	Context->RecordWrite<4>(Ptr);
}

extern "C" UE_AUTORTFM_API AUTORTFM_NO_ASAN void autortfm_record_write_8(void* Ptr)
{
	UE_AUTORTFM_HANDLE_NULL_WRITE(Ptr);
	FContext* Context = FContext::Get();
	Context->RecordWrite<8>(Ptr);
}

extern "C" UE_AUTORTFM_API AUTORTFM_NO_ASAN void autortfm_record_masked_write(void* Ptr, uintptr_t Mask, int MaskWidthBits, int ValueSizeBytes)
{
	UE_AUTORTFM_HANDLE_NULL_WRITE(Ptr);

	char* IncrementablePtr = static_cast<char*>(Ptr);
	for(int i = 0; i < MaskWidthBits; i++)
	{
		if (Mask & (1u << i))
		{
			autortfm_record_write(IncrementablePtr, ValueSizeBytes);
		}

		IncrementablePtr += ValueSizeBytes;
	}
}

// Mark autortfm_lookup_function() with UE_AUTORTFM_NOAUTORTFM and use
// UE_AUTORTFM_MAP_OPEN_TO_SELF to map the closed function to the open.
// We do this to prevent calls to autortfm_lookup_function() being wrapped
// with calls to autortfm_pre_open() and autortfm_post_open(), which is
// expensive for hot code like this.
UE_AUTORTFM_REGISTER_OPEN_TO_CLOSED_FUNCTIONS(
	UE_AUTORTFM_MAP_OPEN_TO_SELF(autortfm_lookup_function)
);
extern "C" UE_AUTORTFM_NOAUTORTFM UE_AUTORTFM_API void* autortfm_lookup_function(void* OriginalFunction, const char* Where)
{
	return FunctionMapLookup(OriginalFunction, Where);
}

extern "C" UE_AUTORTFM_API void autortfm_memcpy(void* Dst, const void* Src, size_t Size)
{
	FContext* Context = FContext::Get();
    Memcpy(Dst, Src, Size, Context);
}

extern "C" UE_AUTORTFM_API void autortfm_memmove(void* Dst, const void* Src, size_t Size)
{
	FContext* Context = FContext::Get();
    Memmove(Dst, Src, Size, Context);
}

extern "C" UE_AUTORTFM_API void autortfm_memset(void* Dst, int Value, size_t Size)
{
	FContext* Context = FContext::Get();
    Memset(Dst, Value, Size, Context);
}

extern "C" UE_AUTORTFM_API void autortfm_unreachable(const char* Message)
{
	AUTORTFM_REPORT_ERROR("AutoRTFM Unreachable: %s", Message);
}

extern "C" UE_AUTORTFM_API void autortfm_llvm_fail(const char* Message)
{
	if (Message)
	{
		AUTORTFM_REPORT_ERROR("AutoRTFM LLVM Failure: %s", Message);
	}
	else
	{
		AUTORTFM_REPORT_ERROR("AutoRTFM LLVM Failure");
	}
}

extern "C" UE_AUTORTFM_API void autortfm_llvm_missing_function()
{
	if (ForTheRuntime::GetInternalAbortAction() == ForTheRuntime::EAutoRTFMInternalAbortActionState::Crash)
	{
		AUTORTFM_FATAL("Transaction failing because of missing function");
	}
	else
	{
		AUTORTFM_ENSURE_MSG(!ForTheRuntime::GetEnsureOnInternalAbort(), "Transaction failing because of missing function");
	}

	FContext* Context = FContext::Get();
    Context->AbortByLanguageAndThrow();
}

extern "C" UE_AUTORTFM_API void autortfm_called_no_autortfm()
{
	AUTORTFM_FATAL("inlined UE_AUTORTFM_NOAUTORTFM function called from the closed");
}

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
