// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef WITH_EDITOR
#include <Misc/DirectHeaderCompiling.h>
#define UE_DIRECT_HEADER_COMPILING_AFTER_OPENCV_HEADERS UE_DIRECT_HEADER_COMPILING(AfterOpenCvHeaders)
#endif

// don't include the functions if doing direct header compiling as they will fail
#if defined(UE_DIRECT_HEADER_COMPILING_AFTER_OPENCV_HEADERS) && UE_DIRECT_HEADER_COMPILING_AFTER_OPENCV_HEADERS
#else

#ifdef OPENCV_HEADERS_GUARD
#undef OPENCV_HEADERS_GUARD
#else
#error Mismatched AfterOpenCVHeaders.h detected.
#endif

// HEADER_UNIT_SKIP - Special include (UE requirement)

#include <calib/Defs.h>

CALIB_POP_MACRO("check")

#endif
