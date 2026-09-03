// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformProperties.h"


/**
 * Implements Windows platform properties.
 */
template<bool HAS_EDITOR_DATA, bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY>
struct FWindowsPlatformProperties
	: public FGenericPlatformProperties
{
	static constexpr FORCEINLINE bool HasEditorOnlyData()
	{
		return HAS_EDITOR_DATA;
	}

	static constexpr FORCEINLINE const char* IniPlatformName()
	{
		return "Windows";
	}

	static constexpr FORCEINLINE const TCHAR* GetRuntimeSettingsClassName()
	{
		return TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings");
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

	static constexpr FORCEINLINE const char* PlatformName()
	{
		if (IS_DEDICATED_SERVER)
		{
			return "WindowsServer";
		}
		
		if (HAS_EDITOR_DATA)
		{
			return "WindowsEditor";
		}
		
		if (IS_CLIENT_ONLY)
		{
			return "WindowsClient";
		}

		return "Windows";
	}

	static constexpr FORCEINLINE bool RequiresCookedData()
	{
		return !HAS_EDITOR_DATA;
	}

	static constexpr FORCEINLINE bool HasSecurePackageFormat()
	{
		return IS_DEDICATED_SERVER;
	}

	static constexpr FORCEINLINE bool SupportsMemoryMappedFiles()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsAudioStreaming()
	{
		return !IsServerOnly();
	}

	static constexpr FORCEINLINE bool SupportsMeshLODStreaming()
	{
		return !IsServerOnly() && !HasEditorOnlyData();
	}

	static constexpr FORCEINLINE bool SupportsRayTracing()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsGrayscaleSRGB()
	{
		return false; // Requires expand from G8 to RGBA
	}

	static constexpr FORCEINLINE bool SupportsMultipleGameInstances()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsWindowedMode()
	{
		return true;
	}
	
	static constexpr FORCEINLINE bool HasFixedResolution()
	{
		return false;
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

	static constexpr FORCEINLINE int64 GetMemoryMappingAlignment()
	{
		return 4096;
	}

	static constexpr FORCEINLINE int GetMaxSupportedVirtualMemoryAlignment()
	{
		return 65536;
	}
};

#ifdef PROPERTY_HEADER_SHOULD_DEFINE_TYPE
typedef FWindowsPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE && !WITH_EDITOR> FPlatformProperties;
#endif
