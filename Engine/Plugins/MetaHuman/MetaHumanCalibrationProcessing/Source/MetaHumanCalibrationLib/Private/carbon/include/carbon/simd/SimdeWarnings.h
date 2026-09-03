// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if defined(__clang__) // note that _MSC_VER can also be defined for clang on Windows so do this first
#define CARBON_DISABLE_SIMDE_WARNINGS
#define CARBON_REENABLE_SIMDE_WARNINGS

#elif defined(_MSC_VER)
// disable warning: sse.h(3979) : warning C6001: Using uninitialized memory 'r_'.: Lines: 3971, 3979
#define CARBON_DISABLE_SIMDE_WARNINGS \
    __pragma(warning(push)) \
    __pragma(warning(disable : 6001))
#define CARBON_REENABLE_SIMDE_WARNINGS __pragma(warning(pop))
#elif !defined(__APPLE__) && defined(__unix__)
#define CARBON_DISABLE_SIMDE_WARNINGS
#define CARBON_REENABLE_SIMDE_WARNINGS
#else
#define CARBON_DISABLE_SIMDE_WARNINGS
#define CARBON_REENABLE_SIMDE_WARNINGS
#endif