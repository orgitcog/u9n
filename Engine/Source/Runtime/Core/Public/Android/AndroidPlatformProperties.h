// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================
	AndroidProperties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformProperties.h"


/**
 * Implements Android platform properties.
 */
struct FAndroidPlatformProperties
	: public FGenericPlatformProperties
{
	static constexpr FORCEINLINE bool HasEditorOnlyData()
	{
		return false;
	}

	static constexpr FORCEINLINE const char* PlatformName()
	{
		return "Android";
	}

	static constexpr FORCEINLINE const char* IniPlatformName()
	{
		return "Android";
	}

	static constexpr FORCEINLINE const TCHAR* GetRuntimeSettingsClassName()
	{
		return TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");
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

	static constexpr FORCEINLINE bool SupportsAutoSDK()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsHighQualityLightmaps()
	{
		return true; // always true because of Vulkan
	}

	static constexpr FORCEINLINE bool SupportsLowQualityLightmaps()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsDistanceFieldShadows()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsTextureStreaming()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsMinimize()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsQuit()
	{
		return true;
	}

	static constexpr FORCEINLINE bool HasFixedResolution()
	{
		return true;
	}

	static constexpr FORCEINLINE bool AllowsFramerateSmoothing()
	{
		return true;
	}

	static constexpr FORCEINLINE bool AllowsCallStackDumpDuringAssert()
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
		// Cook for largest page size available.
		// Data cooked for 16kb page size will work for 4kb page size firmware.
		// TODO: THIS IS WRONG!
		return 16384;
	}
};

#ifdef PROPERTY_HEADER_SHOULD_DEFINE_TYPE
typedef FAndroidPlatformProperties FPlatformProperties;
#endif
