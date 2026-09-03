// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_STATETREE_TRACE_DEBUGGER

#include "IRewindDebugger.h"
#include "IRewindDebuggerExtension.h"
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"

namespace UE::StateTreeDebugger
{

class FRewindDebuggerExtension : public IRewindDebuggerExtension
{
public:

	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) override;

private:

	/** Last scrub time we received. Used to avoid redundant updates. */
	double LastScrubTime = 0.0;
};

class FRewindDebuggerRuntimeExtension : public IRewindDebuggerRuntimeExtension
{
public:

	virtual void RecordingStarted() override;
	virtual void RecordingStopped() override;
};

} // namespace UE::StateTreeDebugger

#endif // WITH_STATETREE_TRACE_DEBUGGER