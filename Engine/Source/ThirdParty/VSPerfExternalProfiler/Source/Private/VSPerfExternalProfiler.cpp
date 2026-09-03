// Copyright Epic Games, Inc. All Rights Reserved.

#include "../Public/VSPerfExternalProfiler.h"

#define VSPERF_NO_DEFAULT_LIB	// Don't use #pragma lib to import the library, we'll handle this stuff ourselves
#define PROFILERAPI				// We won't be statically importing anything (we're dynamically binding), so define PROFILERAPI to a empty value
#include <vsperf.h>				// NOTE: This header is in <Visual Studio install directory>/Team Tools/Performance Tools/x64/PerfSDK
#include <Windows.h>

bool VsPerfInitialized;

/** DLL handle for VSPerf.DLL */
HMODULE VsPerfHandle;

/** Pointer to StopProfile function. */
typedef PROFILE_COMMAND_STATUS(*StopProfileFunctionPtr)(PROFILE_CONTROL_LEVEL Level, unsigned int dwId);
StopProfileFunctionPtr VsPerfStopProfileFunction;

/** Pointer to StartProfile function. */
typedef PROFILE_COMMAND_STATUS(*StartProfileFunctionPtr)(PROFILE_CONTROL_LEVEL Level, unsigned int dwId);
StartProfileFunctionPtr VsPerfStartProfileFunction;


bool VSPerfInitialize()
{
	if (VsPerfInitialized)
	{
		return VsPerfHandle != nullptr;
	}
	VsPerfInitialized = true;

	// Try to load the VSPerf DLL
	// NOTE: VSPerfXXX.dll is installed into /Windows/System32 when Visual Studio is installed.  The XXX is the version number of
	// Visual Studio.  For example, for Visual Studio 2013, the file name is VSPerf120.dll.
#if _MSC_VER >= 1930
	VsPerfHandle = LoadLibrary(L"VSPerf170.dll");	// Visual Studio 2022
#elif _MSC_VER >= 1920
	VsPerfHandle = LoadLibrary(L"VSPerf160.dll");	// Visual Studio 2019
#elif _MSC_VER >= 1910
	VsPerfHandle = LoadLibrary(L"VSPerf150.dll");	// Visual Studio 2017
#elif _MSC_VER >= 1900
	VsPerfHandle = LoadLibrary(L"VSPerf140.dll");	// Visual Studio 2015
#elif _MSC_VER >= 1800
	VsPerfHandle = LoadLibrary(L"VSPerf120.dll");	// Visual Studio 2013
#else
	// Older versions of Visual Studio did not support profiling, or did not include the profiling tools with the professional edition
#endif

	if (VsPerfHandle == NULL)
	{
		return false;
	}

	// Get API function pointers of interest
	VsPerfStopProfileFunction = (StopProfileFunctionPtr)GetProcAddress(VsPerfHandle, "StopProfile");
	VsPerfStartProfileFunction = (StartProfileFunctionPtr)GetProcAddress(VsPerfHandle, "StartProfile");

	if(VsPerfStopProfileFunction != NULL && VsPerfStartProfileFunction != NULL)
	{
		return true;
	}

	// Couldn't find the functions we need.  VSPerf support will not be active.
	FreeLibrary(VsPerfHandle);
	VsPerfHandle = NULL;
	VsPerfStopProfileFunction = NULL;
	VsPerfStartProfileFunction = NULL;

	return false;
}

void VSPerfDeinitialize()
{
	VsPerfInitialized = false;

	if(VsPerfHandle == NULL)
	{
		return;
	}

	FreeLibrary(VsPerfHandle);
	VsPerfHandle = NULL;
	VsPerfStopProfileFunction = NULL;
	VsPerfStartProfileFunction = NULL;
}

bool VsPerfStartProfile()
{
	const int ProcessOrThreadId = PROFILE_CURRENTID;
	PROFILE_COMMAND_STATUS Result = VsPerfStartProfileFunction( PROFILE_GLOBALLEVEL, ProcessOrThreadId );
	return Result == PROFILE_OK;
}

bool VsPerfStopProfile()
{
	const int ProcessOrThreadId = PROFILE_CURRENTID;
	PROFILE_COMMAND_STATUS Result = VsPerfStopProfileFunction( PROFILE_GLOBALLEVEL, ProcessOrThreadId );
	return Result == PROFILE_OK;
}