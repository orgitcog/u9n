// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"


/**
 * Hello message strings
 */
namespace DisplayClusterHelloMessageStrings
{
	constexpr static const TCHAR* ArgumentsDefaultCategory = TEXT("HC");

	namespace Hello
	{
		constexpr static const TCHAR* Name        = TEXT("Hello");
		constexpr static const TCHAR* TypeRequest = TEXT("Request");
		constexpr static const TCHAR* ArgNodeId   = TEXT("NodeId");
	};
};
