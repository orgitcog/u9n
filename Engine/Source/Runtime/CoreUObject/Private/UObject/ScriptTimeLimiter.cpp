// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ScriptTimeLimiter.h"
#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "VerseVM/VVMVerseHangDetection.h"

UE::FScriptTimeLimiter& UE::FScriptTimeLimiter::Get()
{
	thread_local FScriptTimeLimiter TimeLimiterSingleton;
	return TimeLimiterSingleton;
}

void UE::FScriptTimeLimiter::StartTimer()
{
	check(IsInGameThread());

	if (NestingDepth == 0)
	{
		StartingTime = FPlatformTime::Seconds();
	}

	NestingDepth++;
}

void UE::FScriptTimeLimiter::StopTimer()
{
	check(IsInGameThread());

	NestingDepth--;
	check(NestingDepth >= 0);

	if (NestingDepth == 0)
	{
		StartingTime = 0.0;
	}
}

bool UE::FScriptTimeLimiter::HasExceededTimeLimit()
{
	// If no time limit is specified, we default to the Verse hang threshold.
	return HasExceededTimeLimit(VerseHangDetection::VerseHangThreshold());
}

bool UE::FScriptTimeLimiter::HasExceededTimeLimit(double TimeLimit)
{
	// Note that `IsComputationLimitExceeded` with a StartingTime of 0.0 will always return false.
	return VerseHangDetection::IsComputationLimitExceeded(StartingTime, TimeLimit);
}