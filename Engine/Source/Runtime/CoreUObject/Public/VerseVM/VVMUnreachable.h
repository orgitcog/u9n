// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#define VERSE_UNREACHABLE()   \
	do                        \
	{                         \
		while (true)          \
		{                     \
			PLATFORM_BREAK(); \
		}                     \
	}                         \
	while (false)
