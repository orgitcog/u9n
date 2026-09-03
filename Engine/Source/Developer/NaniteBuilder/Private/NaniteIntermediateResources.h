// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Bounds.h"
#include "ClusterDAG.h"

namespace Nanite
{

struct FMeshDataSection;

/** Intermediate representation of the Nanite mesh data for building. */
struct FIntermediateResources
{
	FClusterDAG	ClusterDAG;

	TArray<FMeshDataSection> Sections;
	
	float	SurfaceArea				= 0.0f;	// Only used for computing relative error for fallback and trim.

	uint32	NumInputTriangles		= 0;
	uint32	NumInputVertices		= 0;
	int32	MaxMipLevel				= 0;
	uint32	ResourceFlags			= 0;
};

} // namespace Nanite