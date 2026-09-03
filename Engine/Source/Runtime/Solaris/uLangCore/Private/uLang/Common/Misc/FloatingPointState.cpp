// Copyright Epic Games, Inc. All Rights Reserved.

#include "uLang/Common/Misc/FloatingPointState.h"

#include "AutoRTFM.h"

// The way to access the control registers, and what should go
// into these control registers, depends on the target
// architecture.

#if defined(_x86_64) || defined(__x86_64__) || defined(_M_AMD64)

#include <pmmintrin.h>

UE_AUTORTFM_ALWAYS_OPEN
static uint32_t ReadFloatingPointState()
{
    return _mm_getcsr();
}

UE_AUTORTFM_ALWAYS_OPEN
static void WriteFloatingPointState(uint32_t State)
{
    _mm_setcsr(State);
}

// Our desired state is all floating point exceptions masked, round to nearest, no flush to zero.
static constexpr uint32_t DesiredFloatingPointState = _MM_MASK_MASK | _MM_ROUND_NEAREST | _MM_FLUSH_ZERO_OFF | _MM_DENORMALS_ZERO_OFF;
// Of these fields, we want to check the rounding mode, FTZ and DAZ fields, but don't care about exceptions.
static constexpr uint32_t FloatingPointStateCheckMask = _MM_ROUND_MASK | _MM_FLUSH_ZERO_MASK | _MM_DENORMALS_ZERO_MASK;
// Our problematic state for x86-64 is FTZ enabled, DAZ off (it's SSE3+), rounding mode=RZ
static constexpr uint32_t ProblematicFloatingPointState = _MM_MASK_MASK | _MM_ROUND_TOWARD_ZERO | _MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_OFF;

#elif defined(__aarch64__) || defined(_M_ARM64)

#ifdef _MSC_VER

// With VC++, there are intrinsics
#include <intrin.h>

UE_AUTORTFM_ALWAYS_OPEN
static uint32_t ReadFloatingPointState()
{
    // The system register read/write instructions use 64-bit registers,
    // but the actual register in AArch64 is defined to be 32-bit in the
    // ARMv8 ARM.
    return (uint32_t)_ReadStatusReg(ARM64_FPCR);
}

UE_AUTORTFM_ALWAYS_OPEN
static void WriteFloatingPointState(uint32_t State)
{
    _WriteStatusReg(ARM64_FPCR, State);
}

#elif defined(__GNUC__) || defined(__clang__)

UE_AUTORTFM_ALWAYS_OPEN
static uint32_t ReadFloatingPointState()
{
    uint64_t Value;
    // The system register read/write instructions use 64-bit registers,
    // but the actual register in AArch64 is defined to be 32-bit in the
    // ARMv8 ARM.
    __asm__ volatile("mrs %0, fpcr" : "=r"(Value));
    return (uint32_t)Value;
}

UE_AUTORTFM_ALWAYS_OPEN
static void WriteFloatingPointState(uint32_t State)
{
    uint64_t State64 = State; // Actual reg is 32b, instruction wants a 64b reg
    __asm__ volatile("msr fpcr, %0" : : "r"(State64));
}

#else

#error Unsupported compiler for AArch64!

#endif // Compiler select

// Conveniently, on AArch64, all exceptions masked, round to nearest, IEEE compliant mode is just 0.
static constexpr uint32_t DesiredFloatingPointState = 0;
// We care about FZ (bit 24) = Flush-To-Zero enable and RMode (bits [23:22]) = rounding mode.
static constexpr uint32_t FloatingPointStateCheckMask = 0x01c00000;
// Our problematic state for AArch64 is FZ enabled and RMode=RZ
static constexpr uint32_t ProblematicFloatingPointState = 0x01c00000;

#else

#error Unrecognized target platform!

#endif // Target select

namespace uLang
{

void AssertExpectedFloatingPointState()
{
    const uint32_t CurrentState = ReadFloatingPointState();
    const uint32_t CurrentStateMasked = CurrentState & FloatingPointStateCheckMask;
    const uint32_t DesiredStateMasked = DesiredFloatingPointState & FloatingPointStateCheckMask;
    ULANG_ASSERTF(CurrentStateMasked == DesiredStateMasked, "Unsupported floating-point state set");
}

UE_AUTORTFM_NOAUTORTFM
void SetProblematicFloatingPointStateForTesting()
{
    WriteFloatingPointState(ProblematicFloatingPointState);
}

CFloatStateSaveRestore::CFloatStateSaveRestore()
{
    _SavedState = ReadFloatingPointState();

    WriteFloatingPointState(DesiredFloatingPointState);

    AutoRTFM::PushOnAbortHandler(this, [SavedState = _SavedState]
    {
        WriteFloatingPointState(SavedState);
    });
}

CFloatStateSaveRestore::~CFloatStateSaveRestore()
{
    WriteFloatingPointState(_SavedState);

    AutoRTFM::PopOnAbortHandler(this);
}

}
