// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_SUPERLUMINAL

#define PERFORMANCEAPI_ENABLED 1
#include "Superluminal/PerformanceAPI_capi.h"


void SuperLuminalStartScopedEventWide(const wchar_t* Text, uint32_t Color)
{
	PerformanceAPI_BeginEvent_Wide(Text, nullptr, Color);
}

void SuperLuminalStartScopedEvent(const char* Text, uint32_t Color)
{
	PerformanceAPI_BeginEvent(Text, nullptr, Color);
}

void SuperLuminalEndScopedEvent()
{
	PerformanceAPI_EndEvent();
}

#else

#include <stdint.h>

void SuperLuminalStartScopedEventWide(const wchar_t* Text, uint32_t Color)
{
}

void SuperLuminalStartScopedEvent(const char* Text, uint32_t Color)
{
}

void SuperLuminalEndScopedEvent()
{
}

#endif