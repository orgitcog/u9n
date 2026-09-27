// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "HAL/Platform.h"

namespace mu
{
	class FMesh;
	class FLayout;

	MUTABLERUNTIME_API void MeshPrepareLayout(
		FMesh& Mesh,
		const FLayout& InLayout,
		int32 LayoutChannel,
		bool bNormalizeUVs,
		bool bClampUVIslands,
		bool bEnsureAllVerticesHaveLayoutBlock,
		bool bUseAbsoluteBlockIds
	);

}
