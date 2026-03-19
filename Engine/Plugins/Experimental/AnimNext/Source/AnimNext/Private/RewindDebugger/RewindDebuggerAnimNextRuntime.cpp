// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerAnimNextRuntime.h"

#if ANIMNEXT_TRACE_ENABLED

namespace UE::AnimNext
{
	
void FRewindDebuggerAnimNextRuntime::RecordingStarted()
{
	UE::Trace::ToggleChannel(TEXT("UAF"), true);
}

void FRewindDebuggerAnimNextRuntime::RecordingStopped()
{
	UE::Trace::ToggleChannel(TEXT("UAF"), false);
}

void FRewindDebuggerAnimNextRuntime::Clear()
{
	FAnimNextTrace::Reset();
}

}

#endif
	
