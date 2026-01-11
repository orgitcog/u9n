// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateReentryGuard.h"
#include "Misc/AssertionMacros.h"
#include "SceneStateExecutionContext.h"
#include "SceneStateLog.h"

namespace UE::SceneState
{

FReentryGuard::FReentryGuard(const FReentryHandle& InCurrent, const FSceneStateExecutionContext& InContext)
	: Reference(InCurrent)
	, Original(InCurrent)
{
#if !NO_LOGGING
	if (IsReentry())
	{
		UE_LOG(LogSceneState, Error, TEXT("[%s] Re-entry detected!"), *InContext.GetExecutionContextName());
		FDebug::DumpStackTraceToLog(TEXT("=== FSceneStateReentryGuard::DumpStackTrace(): ==="), ELogVerbosity::Error);
	}
#endif
	Reference.bValue = true;
}

FReentryGuard::~FReentryGuard()
{
	Reference.bValue = Original.bValue;
}

bool FReentryGuard::IsReentry() const
{
	return Original.bValue;
}

} // UE::SceneState
