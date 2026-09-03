// Copyright Epic Games, Inc. All Rights Reserved.

// Must include its own header first
#include "VerseVM/VVMProfilingLibrary.h"

FVerseBeginProfilingEventHandler FVerseProfilingDelegates::OnBeginProfilingEvent;
FVerseEndProfilingEventHandler FVerseProfilingDelegates::OnEndProfilingEvent;

void FVerseProfilingDelegates::RaiseBeginProfilingEvent()
{
	OnBeginProfilingEvent.Broadcast();
}

void FVerseProfilingDelegates::RaiseEndProfilingEvent(const char* UserTag, double TimeInMs, const FProfileLocus& Locus)
{
	OnEndProfilingEvent.Broadcast(UserTag, TimeInMs, Locus);
}
