// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

#define UE_API COREUOBJECT_API

namespace Verse
{
// NOTE: (yiliang.siew) Right now, these are all errors, but in the future there could be warnings potentially.
#define VERSE_RUNTIME_GLITCH_ENUM_DIAGNOSTICS(v)                                                                                                                                                                                  \
	v(UnrecoverableError, ErrRuntime_Internal, "An internal runtime error occurred. There is no other information available.")                                                                                                    \
	v(UnrecoverableError, ErrRuntime_TransactionAbortedByLanguage, "An internal transactional runtime error occurred. There is no other information available.")                                                                  \
	v(UnrecoverableError, ErrRuntime_NativeInternal, "An internal runtime error occurred in native code that was called from Verse. There is no other information available.")                                                    \
	v(UnrecoverableError, ErrRuntime_GeneratedNativeInternal, "An internal runtime error occurred in (generated) native code that was called from Verse. There is no other information available.")                               \
	v(UnrecoverableError, ErrRuntime_InfiniteLoop, "The runtime terminated prematurely because Verse code was running in an infinite loop.")                                                                                      \
	v(UnrecoverableError, ErrRuntime_ComputationLimitExceeded, "The runtime terminated prematurely because Verse code took too long to execute within a single server tick. Try offloading heavy computation to async contexts.") \
	v(UnrecoverableError, ErrRuntime_Overflow, "Overflow encountered.")                                                                                                                                                           \
	v(UnrecoverableError, ErrRuntime_Underflow, "Underflow encountered.")                                                                                                                                                         \
	v(UnrecoverableError, ErrRuntime_FloatingPointOverflow, "Floating-point overflow encountered.")                                                                                                                               \
	v(UnrecoverableError, ErrRuntime_IntegerOverflow, "Integer overflow encountered.")                                                                                                                                            \
	v(UnrecoverableError, ErrRuntime_ErrorRequested, "A runtime error was explicitly raised from user code.")                                                                                                                     \
	v(UnrecoverableError, ErrRuntime_IntegerBoundsExceeded, "A value does not fall inside the representable range of a Verse integer.")                                                                                           \
	v(UnrecoverableError, ErrRuntime_MemoryLimitExceeded, "Exceeded memory limit(s).")                                                                                                                                            \
	v(UnrecoverableError, ErrRuntime_DivisionByZero, "Division by zero attempted.")                                                                                                                                               \
	v(UnrecoverableError, ErrRuntime_WeakMapInvalidKey, "Invalid key used to access persistent `var` `weak_map`.")                                                                                                                \
	v(UnrecoverableError, ErrRuntime_InvalidVarRead, "Attempted to read a `var` out of an invalid object.")                                                                                                                       \
	v(UnrecoverableError, ErrRuntime_InvalidVarWrite, "Attempted to write to a `var` of an invalid object.")                                                                                                                      \
	v(UnrecoverableError, ErrRuntime_InvalidFunctionCall, "Attempted to call an invalid function.")                                                                                                                               \
	v(UnrecoverableError, ErrRuntime_MathIntrinsicCallFailure, "A math intrinsic failed.")                                                                                                                                        \
	v(UnrecoverableError, ErrRuntime_InvalidArrayLength, "Invalid array length.")                                                                                                                                                 \
	v(UnrecoverableError, ErrRuntime_InvalidStringLength, "Invalid string length.")

enum class ERuntimeDiagnostic : uint16_t
{
#define VISIT_DIAGNOSTIC(Severity, EnumName, Description) EnumName,
	VERSE_RUNTIME_GLITCH_ENUM_DIAGNOSTICS(VISIT_DIAGNOSTIC)
#undef VISIT_DIAGNOSTIC
};

enum class ERuntimeDiagnosticSeverity : uint8_t
{
	UnrecoverableError
};

struct SRuntimeDiagnosticInfo
{
	const char* Name;
	const char* Description;
	ERuntimeDiagnosticSeverity Severity;
};

COREUOBJECT_API const SRuntimeDiagnosticInfo& GetRuntimeDiagnosticInfo(const ERuntimeDiagnostic Diagnostic);

COREUOBJECT_API FString AsFormattedString(const ERuntimeDiagnostic& Diagnostic, const FText& MessageText);

} // namespace Verse

DECLARE_MULTICAST_DELEGATE_TwoParams(FVerseRuntimeErrorReportHandler, const Verse::ERuntimeDiagnostic, const FText&);

class FVerseExceptionReporter
{
public:
	static UE_API FVerseRuntimeErrorReportHandler OnVerseRuntimeError;
};

#undef UE_API
