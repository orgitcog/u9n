// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMVerseHangDetection.h"
#include "AutoRTFM.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "VerseVM/VVMLog.h"

// TODO: Once the new VM lands, we should attempt to reduce GVerseHangDetectionThresholdSeconds back to 3.0. #jira SOL-7622
static float GVerseHangDetectionThresholdSeconds = 9.0f;
static float GVerseHangDetectionThresholdSecondsDuringCook = 120.0f;
static bool GVerseHangDetectionDuringDebugging = false;

static void OnVerseHangDetectionThresholdChanged()
{
	UE_LOG(LogVerseVM, Log, TEXT("Verse hang detection threshold changed to '%f'"), GVerseHangDetectionThresholdSeconds);
}

static FAutoConsoleVariableRef CVarVerseHangDetectionThreshold(
	TEXT("verse.HangDetectionThresholdSeconds"),
	GVerseHangDetectionThresholdSeconds,
	TEXT("Maximum time a Verse script is permitted to run before a runtime error is triggered.\n"),
	FConsoleVariableDelegate::CreateStatic([](IConsoleVariable*) { OnVerseHangDetectionThresholdChanged(); }),
	ECVF_Default);

static FAutoConsoleVariableRef CVarVerseHangDetectionThresholdDuringCook(
	TEXT("verse.HangDetectionThresholdSecondsDuringCook"),
	GVerseHangDetectionThresholdSecondsDuringCook,
	TEXT("Maximum time a Verse script is permitted to run before a runtime error is triggered - in the cooker.\n"),
	ECVF_Default);

static FAutoConsoleVariableRef CVarVerseHangDetectionDuringDebugging(
	TEXT("verse.HangDetectionDuringDebugging"),
	GVerseHangDetectionDuringDebugging,
	TEXT("True if verse hang detection should be enabled during debugging.\n"),
	ECVF_Default);

namespace VerseHangDetection
{

float VerseHangThreshold()
{
#if !UE_BUILD_SHIPPING
	if (::IsRunningCommandlet())
	{
		return GVerseHangDetectionThresholdSecondsDuringCook;
	}

	const float VeryLargeHangThreshold = 120.0f;

	// If we are using the memory validator, that can significantly increase the runtime of Verse code as we are
	// performing additional checks that transactional code behaves correctly with respect to memory, and thus
	// we bump the hang threshold to compensate for this additional checking.
	if (AutoRTFM::ForTheRuntime::GetMemoryValidationLevel() != AutoRTFM::EMemoryValidationLevel::Disabled)
	{
		return VeryLargeHangThreshold;
	}

	// If we are using the aborteroonie 'retry all transactions at least once to check we abort correctly' mode, we
	// bump the hang threshold as we are running every transaction nest at least *twice*.
	if (AutoRTFM::ForTheRuntime::GetRetryTransaction() != AutoRTFM::ForTheRuntime::EAutoRTFMRetryTransactionState::NoRetry)
	{
		return VeryLargeHangThreshold;
	}
#endif // UE_BUILD_SHIPPING

	return GVerseHangDetectionThresholdSeconds;
}

bool IsComputationLimitExceeded(const double StartTime, double HangThreshold)
{
	if (StartTime == 0.0f)
	{
		return false;
	}

	double RunningTime = ::FPlatformTime::Seconds() - StartTime;
	if (RunningTime < HangThreshold || (!GVerseHangDetectionDuringDebugging && ::FPlatformMisc::IsDebuggerPresent()))
	{
		return false;
	}
	return true;
}

} // namespace VerseHangDetection
