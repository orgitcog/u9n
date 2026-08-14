// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================
	LinuxPlatformProperties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformProperties.h"


/**
 * Implements Linux platform properties.
 */
template<bool HAS_EDITOR_DATA, bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY, bool IS_ARM64>
struct FLinuxPlatformProperties
	: public FGenericPlatformProperties
{
	static constexpr FORCEINLINE bool HasEditorOnlyData()
	{
		return HAS_EDITOR_DATA;
	}

	static constexpr FORCEINLINE const char* IniPlatformName()
	{
		return IS_ARM64 ? "LinuxArm64" : "Linux";
	}

	static constexpr FORCEINLINE const TCHAR* GetRuntimeSettingsClassName()
	{
		return TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings");
	}

	static constexpr FORCEINLINE bool IsGameOnly()
	{
		return UE_GAME;
	}

	static constexpr FORCEINLINE bool IsServerOnly()
	{
		return IS_DEDICATED_SERVER;
	}

	static constexpr FORCEINLINE bool IsClientOnly()
	{
		return IS_CLIENT_ONLY;
	}

	static constexpr FORCEINLINE bool IsArm64()
	{
		return IS_ARM64;
	}

	static constexpr FORCEINLINE const char* PlatformName()
	{
		if (IS_DEDICATED_SERVER)
		{
			return IS_ARM64 ? "LinuxArm64Server" : "LinuxServer";
		}

		if (HAS_EDITOR_DATA)
		{
			return "LinuxEditor";
		}

		if (IS_CLIENT_ONLY)
		{
			return IS_ARM64 ? "LinuxArm64Client" : "LinuxClient";
		}

		return IS_ARM64 ? "LinuxArm64" : "Linux";
	}

	static constexpr FORCEINLINE bool RequiresCookedData()
	{
		return !HAS_EDITOR_DATA;
	}

	static constexpr FORCEINLINE bool HasSecurePackageFormat()
	{
		return IS_DEDICATED_SERVER;
	}

	static constexpr FORCEINLINE bool RequiresUserCredentials()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsAutoSDK()
	{
// linux cross-compiling / cross-building from windows supports AutoSDK.  But hosted linux doesn't yet.
#if PLATFORM_WINDOWS
		return true;
#else
		return false;
#endif
	}

	static constexpr FORCEINLINE bool SupportsMultipleGameInstances()
	{
		return true;
	}

	static constexpr FORCEINLINE bool HasFixedResolution()
	{
		return false;
	}

	static constexpr FORCEINLINE bool SupportsWindowedMode()
	{
		return !IS_DEDICATED_SERVER;
	}

	static constexpr FORCEINLINE bool AllowsFramerateSmoothing()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsRayTracing()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsQuit()
	{
		return true;
	}

	static constexpr FORCEINLINE float GetVariantPriority()
	{
		if (IS_DEDICATED_SERVER)
		{
			return 0.0f;
		}

		if (HAS_EDITOR_DATA)
		{
			return 0.0f;
		}

		if (IS_CLIENT_ONLY)
		{
			return 0.0f;
		}

		return 1.0f;
	}

	static constexpr FORCEINLINE bool AllowsCallStackDumpDuringAssert()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsAudioStreaming()
	{
		return !IsServerOnly();
	}

	static constexpr FORCEINLINE int64 GetMemoryMappingAlignment()
	{
		return 4096;
	}
};

#ifdef PROPERTY_HEADER_SHOULD_DEFINE_TYPE
typedef FLinuxPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE && !WITH_EDITOR, !!PLATFORM_CPU_ARM_FAMILY> FPlatformProperties;
#endif
