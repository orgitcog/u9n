// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/Event.h"
#include "Misc/ScopeRWLock.h"

namespace UE::CaptureManager
{

FCaptureEvent::FCaptureEvent(FString InName) : Name(MoveTemp(InName))
{
}

const FString& FCaptureEvent::GetName() const
{
	return Name;
}

FCaptureEvent::~FCaptureEvent() = default;

}