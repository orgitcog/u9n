// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMisc.h"


/**
 * Base class for platform properties.
 *
 * These are shared between:
 *     the runtime platform - via FPlatformProperties
 *     the target platforms - via ITargetPlatform
 */
struct FGenericPlatformProperties
{
	/**
	 * Gets the platform's physics format.
	 *
	 * @return The physics format name.
	 */
	static constexpr FORCEINLINE const char* GetPhysicsFormat()
	{
		return "Chaos";
	}

	/**
	 * Gets whether this platform has Editor-only data.
	 *
	 * @return true if the platform has Editor-only data, false otherwise.
	 */
	static constexpr FORCEINLINE bool HasEditorOnlyData()
	{
		return WITH_EDITORONLY_DATA;
	}

	/**
	 * Gets the name of this platform when loading INI files. Defaults to PlatformName.
	 *
	 * Note: MUST be implemented per platform.
	 *
	 * @return Platform name.
	 */
	static const char* IniPlatformName();

	/**
	 * Gets whether this is a game only platform.
	 *
	 * @return true if this is a game only platform, false otherwise.
	 */
	static constexpr FORCEINLINE bool IsGameOnly()
	{
		return UE_GAME;
	}

	/**
	 * Gets whether this is a server only platform.
	 *
	 * @return true if this is a server only platform, false otherwise.
	 */
	static constexpr FORCEINLINE bool IsServerOnly()
	{
		return UE_SERVER;
	}

	/**
	 * Gets whether this is a client only (no capability to run the game without connecting to a server) platform.
	 *
	 * @return true if this is a client only platform, false otherwise.
	 */
	static constexpr FORCEINLINE bool IsClientOnly()
	{
		return !WITH_SERVER_CODE;
	}

	/**
	 *	Gets whether this was a monolithic build or not
	 */
	static constexpr FORCEINLINE bool IsMonolithicBuild()
	{
		return IS_MONOLITHIC;
	}

	/**
	 *	Gets whether this was a program or not
	 */
	static constexpr FORCEINLINE bool IsProgram()
	{
		return IS_PROGRAM;
	}

	/**
	 * Gets whether this is a Little Endian platform.
	 *
	 * @return true if the platform is Little Endian, false otherwise.
	 */
	static constexpr FORCEINLINE bool IsLittleEndian()
	{
		return true;
	}

	/**
	 * Gets the name of this platform
	 *
	 * Note: MUST be implemented per platform.
	 *
	 * @return Platform Name.
	 */
	static FORCEINLINE const char* PlatformName();

	/**
	  * Get the name of the hardware variant of the current platform.
	  * 
	  * Most platforms don't need to provide overrides for this member. This member is intended to be used by the few
	  * which come in different hardware flavours or which may operate in different runtime modes.
	  * 
	  * @return Name of the platform variant.
	  */
	static FORCEINLINE const char* PlatformVariantName()
	{
		return "";
	}

	/**
	 * Checks whether this platform requires cooked data.
	 *
	 * @return true if cooked data is required, false otherwise.
	 */
	static constexpr FORCEINLINE bool RequiresCookedData()
	{
		return !HasEditorOnlyData();
	}

	/**
	* Checks whether shipped data on this platform is secure, and doesn't require extra encryption/signing to protect it.
	*
	* @return true if packaged data is considered secure, false otherwise.
	*/
	static constexpr FORCEINLINE bool HasSecurePackageFormat()
	{
		return false;
	}

	/**
	 * Checks whether this platform requires user credentials (typically server platforms).
	 *
	 * @return true if this platform requires user credentials, false otherwise.
	 */
	static constexpr FORCEINLINE bool RequiresUserCredentials()
	{
		return false;
	}

	/**
	 * Checks whether the specified build target is supported.
	 *
	 * @param TargetType The build target to check.
	 * @return true if the build target is supported, false otherwise.
	 */
	static FORCEINLINE bool SupportsBuildTarget(EBuildTargetType TargetType)
	{
		return true;
	}

	/**
	 * Returns true if platform supports the AutoSDK system
	 */
	static constexpr FORCEINLINE bool SupportsAutoSDK()
	{
		return false;
	}

	/**
	 * Gets whether this platform supports gray scale sRGB texture formats.
	 *
	 * @return true if gray scale sRGB texture formats are supported.
	 */
	static constexpr FORCEINLINE bool SupportsGrayscaleSRGB()
	{
		return true;
	}

	/**
	 * Checks whether this platforms supports running multiple game instances on a single device.
	 *
	 * @return true if multiple instances are supported, false otherwise.
	 */
	static constexpr FORCEINLINE bool SupportsMultipleGameInstances()
	{
		return false;
	}

	/**
	 * Gets whether this platform supports windowed mode rendering.
	 *
	 * @return true if windowed mode is supported.
	 */
	static constexpr FORCEINLINE bool SupportsWindowedMode()
	{
		return false;
	}

	/**
	 * Whether this platform wants to allow framerate smoothing or not.
	 */
	static constexpr FORCEINLINE bool AllowsFramerateSmoothing()
	{
		return true;
	}

	/**
	 * Whether this platform supports streaming audio
	 */
	static constexpr FORCEINLINE bool SupportsAudioStreaming()
	{
		return false;
	}

	static constexpr FORCEINLINE bool SupportsHighQualityLightmaps()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsLowQualityLightmaps()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsDistanceFieldShadows()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsDistanceFieldAO()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsTextureStreaming()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsMeshLODStreaming()
	{
		return false;
	}

	static constexpr FORCEINLINE bool SupportsMemoryMappedFiles()
	{
		return false;
	}

	static constexpr FORCEINLINE bool SupportsMemoryMappedAudio()
	{
		return false;
	}

	static constexpr FORCEINLINE bool SupportsMemoryMappedAnimation()
	{
		return false;
	}

	static constexpr FORCEINLINE int64 GetMemoryMappingAlignment()
	{
		return 0;
	}

	// Guaranteed virtual memory alignment on a given platform, regardless of a specific device
	static constexpr FORCEINLINE int GetMaxSupportedVirtualMemoryAlignment()
	{
		return 4096;
	}
	
	static constexpr FORCEINLINE bool SupportsRayTracing()
	{
		return false;
	}

	static constexpr FORCEINLINE bool SupportsLumenGI()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsHardwareLZDecompression()
	{
		return false;
	}

	/**
	 * Gets whether user settings should override the resolution or not
	 */
	static constexpr FORCEINLINE bool HasFixedResolution()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsMinimize()
	{
		return false;
	}

	// Whether the platform allows an application to quit to the OS
	static constexpr FORCEINLINE bool SupportsQuit()
	{
		return false;
	}

	// Whether the platform allows the call stack to be dumped during an assert
	static constexpr FORCEINLINE bool AllowsCallStackDumpDuringAssert()
	{
		return IsProgram();
	}

	// If this platform wants to replace Zlib with a platform-specific version, set the name of the compression format 
	// plugin (matching its GetCompressionFormatName() function) in an override of this function
	static constexpr FORCEINLINE const char* GetZlibReplacementFormat()
	{
		return nullptr;
	}
	 
	// Whether the platform requires an original release version to make a patch
	static constexpr FORCEINLINE bool RequiresOriginalReleaseVersionForPatch()
	{
		return false;
	}
};
