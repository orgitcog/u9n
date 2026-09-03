// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"
#include "RewindDebugger/AnimNextTrace.h"

#if ANIMNEXT_TRACE_ENABLED
// Rewind debugger extension for Chooser support

namespace UE::AnimNext
{

class FRewindDebuggerAnimNextRuntime : public IRewindDebuggerRuntimeExtension
{
public:
	virtual void RecordingStarted() override;
	virtual void RecordingStopped() override;
	
	virtual void Clear() override;
};

}

#endif