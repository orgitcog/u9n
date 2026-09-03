// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "RenderGraphDefinitions.h"

#if RHI_RAYTRACING

struct FRayTracingAccelerationStructureSize;
class FRayTracingGeometry;
class FRDGBuilder;
class FRHIBuffer;
class FRHICommandList;

UE_DEPRECATED(5.6, "The class FRayTracingSkinnedGeometryUpdateQueue has been deprecated - Skinned geometries are collected for update during GetDynamicRayTracingInstances on the proxy automatically.");
class FRayTracingSkinnedGeometryUpdateQueue
{
	UE_DEPRECATED(5.6, "This function has been deprecated. Skinned geometries are collected for update during GetDynamicRayTracingInstances on the proxy automatically.")
	void Add(FRayTracingGeometry* InRayTracingGeometry, const FRayTracingAccelerationStructureSize& StructureSize)
	{
	}

	UE_DEPRECATED(5.6, "This function has been deprecated. Geometries don't need to be manually removed anymore.")
	void Remove(FRayTracingGeometry* RayTracingGeometry, uint32 EstimatedMemory = 0)
	{
	}

	UE_DEPRECATED(5.6, "This function has been deprecated - scratch buffer size is auto computed during AddDynamicGeometryUpdatePass")
	uint32 ComputeScratchBufferSize() const
	{
		return 0;
	}
	
	UE_DEPRECATED(5.5, "This function has been deprecated. Use the version that takes GraphBuilder as input.")
	void Commit(FRHICommandList& RHICmdList, FRHIBuffer* ScratchBuffer) {}

	UE_DEPRECATED(5.6, "This function has been deprecated because logic is merged with the dynamic geometry manager and is not needed anymore.")
	void Commit(FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags = ERDGPassFlags::Compute) {}
};

#endif // RHI_RAYTRACING
