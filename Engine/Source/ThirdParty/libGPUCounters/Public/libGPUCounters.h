// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// C only wrapper to avoid C++ ABI issues between different NDK's

#include <stdint.h>

#if !defined(_MSC_VER)
    #define LIBGPUCOUNTERS_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
enum class libGPUCountersLogLevel : uint8_t
{
	Log = 0,
	Error = 1
};
#endif

typedef void (*libGPUCountersLogCallback)(uint8_t Level, const char* Message);

extern "C"
{
	void LIBGPUCOUNTERS_API libGPUCountersInit(libGPUCountersLogCallback Callback);
	void LIBGPUCOUNTERS_API libGPUCountersUpdate();
	void LIBGPUCOUNTERS_API libGPUCountersLog();
}
