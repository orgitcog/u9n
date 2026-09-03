// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"


/**
 * JSON cluster event protocol strings
 */
namespace DisplayClusterClusterEventsJsonStrings
{
	constexpr static const TCHAR* ProtocolName = TEXT("EventsJSON");
	constexpr static const TCHAR* TypeRequest  = TEXT("Request");

	constexpr static auto ArgName       = TEXT("Name");
	constexpr static auto ArgType       = TEXT("Type");
	constexpr static auto ArgCategory   = TEXT("Category");
};
