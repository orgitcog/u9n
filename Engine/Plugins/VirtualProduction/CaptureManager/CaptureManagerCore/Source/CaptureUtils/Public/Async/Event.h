// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/ManagedDelegate.h"

#include "Containers/UnrealString.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Delegates/Delegate.h"
#include "HAL/CriticalSection.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

#include <atomic>

namespace UE::CaptureManager
{

// Base class for all event sublasses
class CAPTUREUTILS_API FCaptureEvent
{
public:
	const FString& GetName() const;
	virtual ~FCaptureEvent();

protected:
	FCaptureEvent(FString InName);


private:
	FString Name;
};

// Macro for easy definition of events that don't carry any data
#define CAPTURE_DEFINE_EMPTY_EVENT(ClassName, EventName) \
	struct CAPTUREUTILS_API ClassName : public FCaptureEvent \
	{ \
		inline static const FString Name = TEXT(EventName); \
		ClassName() : FCaptureEvent(Name) \
		{ \
		} \
	};

// The SharedPtr points to a `const` event object because the shared event might end up on multiple
// threads in which case we'd have a thread safety issue without the event being `const`
using FCaptureEventHandler = TManagedDelegate<TSharedPtr<const FCaptureEvent>>;

// Interface for classes that wish to provide capture event subscription to their clients
class ICaptureEventSource
{
public:
	virtual TArray<FString> GetAvailableEvents() const = 0;
	virtual void SubscribeToEvent(const FString& InEventName, FCaptureEventHandler InHandler) = 0;
	virtual void UnsubscribeAll() = 0;
};

}