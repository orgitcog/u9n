// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================
	IOSPlatformProperties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "GenericPlatform/GenericPlatformProperties.h"


/**
 * Implements iOS platform properties.
 */
struct FIOSPlatformProperties
	: public FGenericPlatformProperties
{
	static constexpr FORCEINLINE bool HasEditorOnlyData()
	{
		return false;
	}

	static constexpr FORCEINLINE const char* PlatformName()
	{
		return "IOS";
	}

	static constexpr FORCEINLINE const char* IniPlatformName()
	{
		return "IOS";
	}

	static constexpr FORCEINLINE const TCHAR* GetRuntimeSettingsClassName()
	{
		return TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings");
	}

	static constexpr FORCEINLINE bool IsGameOnly()
	{
		return true;
	}
	
	static constexpr FORCEINLINE bool RequiresCookedData()
	{
		return true;
	}
    
	static FORCEINLINE bool SupportsBuildTarget(EBuildTargetType TargetType)
	{
		return (TargetType == EBuildTargetType::Game);
	}

	static constexpr FORCEINLINE bool SupportsLowQualityLightmaps()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsHighQualityLightmaps()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsTextureStreaming()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsMemoryMappedFiles()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsMemoryMappedAudio()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsMemoryMappedAnimation()
	{
		return true;
	}

	static constexpr FORCEINLINE int64 GetMemoryMappingAlignment()
	{
		return 16384;
	}

	static constexpr FORCEINLINE int GetMaxSupportedVirtualMemoryAlignment()
	{
		return 16384;
	}

	static constexpr FORCEINLINE bool HasFixedResolution()
	{
		return true;
	}

	static constexpr FORCEINLINE bool AllowsFramerateSmoothing()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsAudioStreaming()
	{
		return true;
	}
	
	static constexpr FORCEINLINE bool SupportsMeshLODStreaming()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsQuit()
	{
		return true;
	}
};

struct FTVOSPlatformProperties : public FIOSPlatformProperties
{
	// @todo breaking change here!
	static constexpr FORCEINLINE const char* PlatformName()
	{
		return "TVOS";
	}

	static constexpr FORCEINLINE const char* IniPlatformName()
	{
		return "TVOS";
	}
};

struct FVisionOSPlatformProperties : public FIOSPlatformProperties
{
	static constexpr FORCEINLINE const char* PlatformName()
	{
		return "IOS";
	}

	static constexpr FORCEINLINE const char* IniPlatformName()
	{
		return "VisionOS";
	}
};

#ifdef PROPERTY_HEADER_SHOULD_DEFINE_TYPE

#if PLATFORM_VISIONOS
typedef FVisionOSPlatformProperties FPlatformProperties;
#else
typedef FIOSPlatformProperties FPlatformProperties;
#endif

#endif
