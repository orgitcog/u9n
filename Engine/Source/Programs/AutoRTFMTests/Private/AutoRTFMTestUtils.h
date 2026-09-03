// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutoRTFM.h"
#include "Misc/FeedbackContext.h"

#include <vector>

namespace AutoRTFMTestUtils
{

// Temporarily changes the AutoRTFM retry mode for the lifetime of the FScopedRetry object.
struct FScopedRetry
{
	FScopedRetry(AutoRTFM::ForTheRuntime::EAutoRTFMRetryTransactionState NewRetry)
		: OldRetry(AutoRTFM::ForTheRuntime::GetRetryTransaction())
	{
		AutoRTFM::ForTheRuntime::SetRetryTransaction(NewRetry);
	}

	~FScopedRetry()
	{
		AutoRTFM::ForTheRuntime::SetRetryTransaction(OldRetry);
	}

	AutoRTFM::ForTheRuntime::EAutoRTFMRetryTransactionState OldRetry;
};

// A helper class that for the lifetime of the object, intercepts and records UE_LOG warnings.
class FCaptureWarningContext : private FFeedbackContext
{
public:
	FCaptureWarningContext() : OldContext(GWarn) { GWarn = this; }
	~FCaptureWarningContext() { GWarn = OldContext; }

	void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (Verbosity == ELogVerbosity::Warning)
		{
			Warnings.push_back(V);
		}
		else
		{
			OldContext->Serialize(V, Verbosity, Category);
		}
	}

	void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time) override
	{
		if (Verbosity == ELogVerbosity::Warning)
		{
			Warnings.push_back(V);
		}
		else
		{
			OldContext->Serialize(V, Verbosity, Category, Time);
		}
	}

	const std::vector<FString>& GetWarnings() const
	{
		return Warnings;
	}

private:
	FCaptureWarningContext(const FCaptureWarningContext&) = delete;
	FCaptureWarningContext& operator = (const FCaptureWarningContext&) = delete;

	FFeedbackContext* OldContext = nullptr;
	std::vector<FString> Warnings;
};

// A helper class that temporarily changes the memory validation level for the lifetime of the
// object, restoring the original level on destruction.
class ScopedMemoryValidationLevel
{
public:
	ScopedMemoryValidationLevel(AutoRTFM::EMemoryValidationLevel NewLevel)
		: PrevLevel(AutoRTFM::ForTheRuntime::GetMemoryValidationLevel())
	{
		AutoRTFM::ForTheRuntime::SetMemoryValidationLevel(NewLevel);
	}

	~ScopedMemoryValidationLevel()
	{
		AutoRTFM::ForTheRuntime::SetMemoryValidationLevel(PrevLevel);
	}
private:
	ScopedMemoryValidationLevel(const ScopedMemoryValidationLevel&) = delete;
	ScopedMemoryValidationLevel& operator = (const ScopedMemoryValidationLevel&) = delete;

	const AutoRTFM::EMemoryValidationLevel PrevLevel;
};

#define AUTORTFM_SCOPED_DISABLE_MEMORY_VALIDATION() \
	AutoRTFMTestUtils::ScopedMemoryValidationLevel DisableMemoryValidation \
		{AutoRTFM::EMemoryValidationLevel::Disabled}; \
	static_assert(true) /* require semicolon */

#define AUTORTFM_SCOPED_ENABLE_MEMORY_VALIDATION_AS_WARNING() \
	AutoRTFMTestUtils::ScopedMemoryValidationLevel EnableMemoryValidationAsWarning \
		{AutoRTFM::EMemoryValidationLevel::Warn}; \
	static_assert(true) /* require semicolon */

static constexpr const TCHAR* kMemoryModifiedWarning =
    TEXT("Memory modified in a transaction was also modified in an call to AutoRTFM::Open(). ")
    TEXT("This may lead to memory corruption if the transaction is aborted.");

class FScopedEnsureOnInternalAbort final
{
public:
	FScopedEnsureOnInternalAbort(const bool bState)
		: bOriginal(AutoRTFM::ForTheRuntime::GetEnsureOnInternalAbort())
	{
		AutoRTFM::ForTheRuntime::SetEnsureOnInternalAbort(bState);
	}

	~FScopedEnsureOnInternalAbort()
	{
		AutoRTFM::ForTheRuntime::SetEnsureOnInternalAbort(bOriginal);
	}

private:
	FScopedEnsureOnInternalAbort(const FScopedEnsureOnInternalAbort&) = delete;
	FScopedEnsureOnInternalAbort& operator = (const FScopedEnsureOnInternalAbort&) = delete;

	const bool bOriginal;
};

class FScopedInternalAbortAction final
{
public:
	FScopedInternalAbortAction(AutoRTFM::ForTheRuntime::EAutoRTFMInternalAbortActionState State)
		: Original(AutoRTFM::ForTheRuntime::GetInternalAbortAction())
	{
		AutoRTFM::ForTheRuntime::SetInternalAbortAction(State);
	}

	~FScopedInternalAbortAction()
	{
		AutoRTFM::ForTheRuntime::SetInternalAbortAction(Original);
	}

private:
	FScopedInternalAbortAction(const FScopedInternalAbortAction&) = delete;
	FScopedInternalAbortAction& operator = (const FScopedInternalAbortAction&) = delete;

	const AutoRTFM::ForTheRuntime::EAutoRTFMInternalAbortActionState Original;
};

}
