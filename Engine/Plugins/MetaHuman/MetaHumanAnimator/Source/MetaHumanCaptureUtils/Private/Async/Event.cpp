// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/Event.h"
#include "Misc/ScopeRWLock.h"

FCaptureEvent::FCaptureEvent(FString InName) : Name(MoveTemp(InName))
{
}

const FString& FCaptureEvent::GetName() const
{
	return Name;
}

FCaptureEvent::~FCaptureEvent() = default;