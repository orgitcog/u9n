// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LocateBy.h"

class IElementLocator;

class FSlateWidgetLocatorByDelegateFactory
{
public:

	static TSharedRef<IElementLocator, ESPMode::ThreadSafe> Create(
		const FLocateSlateWidgetElementDelegate& Delegate, FStringView DebugName = { });

	static TSharedRef<IElementLocator, ESPMode::ThreadSafe> Create(
		const FLocateSlateWidgetPathElementDelegate& Delegate, FStringView DebugName = { });
};
