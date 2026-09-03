// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef __cplusplus
#include "HLSLTypeAliases.h"

namespace UE::HLSL
{
#endif

struct FMaterialCacheBinData
{
	// DW0
	uint3 ABufferPhysicalPosition;
	uint  PrimitiveData;
	// DW4
	float4 UVMinAndThreadAdvance;
	// DW8
	float4 UVMinAndInvSize;
	// DW12
	uint4 Pad16;
};

struct FMaterialCachePageWriteData
{
	// DW0
	uint3 ABufferPhysicalPosition;
	uint  Pad;
	// DW4
	uint2 VTPhysicalPosition;
	uint  Pad2;
};

enum EMaterialCacheFlag
{
	MatCache_None = 0u,
	MatCache_DefaultBottomLayer = 1u << 0u,
};

#ifdef __cplusplus
} // namespace UE::HLSL
#endif
