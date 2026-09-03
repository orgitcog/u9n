// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelFormat.h"
#include "Containers/Array.h"
#include "Misc/EnumClassFlags.h"

static constexpr uint32 MaterialCacheMaxABuffers = 3u;

enum class EMaterialCacheAttribute : uint8
{
	BaseColor,
	WorldNormal,
	Normal,
	Roughness,
	Specular,
	Metallic,
	Opacity,
	WorldPosition
};

enum class EMaterialCacheAttributeMask
{
	// TODO: Conditional targets
};

ENUM_CLASS_FLAGS(EMaterialCacheAttributeMask);

static void GetMaterialCacheABufferFormats(EMaterialCacheAttributeMask Mask, TArray<EPixelFormat, TInlineAllocator<MaterialCacheMaxABuffers>>& OutFormats) {
	OutFormats.Add(PF_R8G8B8A8);
	OutFormats.Add(PF_A2B10G10R10);
	OutFormats.Add(PF_R8G8B8A8);
}
