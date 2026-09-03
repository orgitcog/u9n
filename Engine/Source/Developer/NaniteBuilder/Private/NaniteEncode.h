// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NaniteDefinitions.h"
#include "Math/Bounds.h"

struct FMeshNaniteSettings;

namespace Nanite
{
	struct FResources;
	class FCluster;
	class FClusterDAG;
	struct FClusterGroup;
	
	void BuildRayTracingData(FResources& Resources, TArray<FCluster>& Clusters);
	void Encode(
		FResources& Resources,
		FClusterDAG& ClusterDAG,
		const FMeshNaniteSettings& Settings,
		uint32 NumMeshes,
		uint32* OutTotalGPUSize
	);
}
