// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIShaderPlatform.h"

RENDERER_API uint32 GetMaterialCacheTileWidth();
RENDERER_API uint32 GetMaterialCacheTileBorderWidth();

/** Is object space supported (e.g., cook data) for the given platform? */
RENDERER_API bool IsMaterialCacheSupported(FStaticShaderPlatform Platform);

/** Is object space enabled at runtime? May be toggled. */
RENDERER_API bool IsMaterialCacheEnabled(FStaticShaderPlatform Platform);
