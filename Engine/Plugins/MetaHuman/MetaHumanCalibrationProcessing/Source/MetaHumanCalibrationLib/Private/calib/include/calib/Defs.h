// Copyright Epic Games, Inc. All Rights Reserved.

/**
    Multiple-View-Geometry Toolbox.

    Project definitions file.
 */

#pragma once

#include <carbon/Common.h>

#define CALIB_STRINGIZE(Token) #Token

#if defined(__clang__) or defined(__GNUC__)

#define CALIB_PUSH_MACRO(name) _Pragma(CALIB_STRINGIZE(push_macro(name)))
#define CALIB_POP_MACRO(name) _Pragma(CALIB_STRINGIZE(pop_macro(name)))

#else

#define CALIB_PUSH_MACRO(name) __pragma(push_macro(name))
#define CALIB_POP_MACRO(name) __pragma(pop_macro(name))

#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CALIB_STATIC_API
#define CALIB_API
#else
    #ifdef CALIB_DYNAMIC_API
        #if defined(_MSC_VER)
#define CALIB_API __declspec(dllexport)
        #else
#define CALIB_API __attribute__ ((visibility("default")))
        #endif
    #else
        #if defined(_MSC_VER)
#define CALIB_API __declspec(dllimport)
        #else
#define CALIB_API __attribute__ ((visibility("default")))
        #endif
    #endif
#endif

namespace mvg
{

#define real_t double
} // namespace mvg
  // Floating point precision.

#ifdef __cplusplus
}
#endif
