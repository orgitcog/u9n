// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/ArrayView.h"

#include "RHI.h"
#include "RHIResources.h"
#include "RHICommandList.h"

#if RHI_RAYTRACING

class FRayTracingGeometry;
class FRHIComputeCommandList;
enum class EAccelerationStructureBuildMode;
enum class ERTAccelerationStructureBuildPriority : uint8;

namespace RayTracing
{
	using FGeometryGroupHandle = int32;
	using GeometryGroupHandle UE_DEPRECATED(5.6, "Use FGeometryGroupHandle instead.") = FGeometryGroupHandle;
}

class IRayTracingGeometryManager
{
public:

	using FBuildRequestIndex = int32;
	using BuildRequestIndex UE_DEPRECATED(5.6, "Use FBuildRequestIndex instead.") = FBuildRequestIndex;

	using FGeometryHandle = int32;
	using RayTracingGeometryHandle UE_DEPRECATED(5.6, "Use FGeometryHandle instead.") = FGeometryHandle;

	virtual ~IRayTracingGeometryManager() = default;

	virtual FBuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode) = 0;

	FBuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority)
	{
		return RequestBuildAccelerationStructure(InGeometry, InPriority, EAccelerationStructureBuildMode::Build);
	}

	virtual void RemoveBuildRequest(FBuildRequestIndex InRequestIndex) = 0;
	virtual void BoostPriority(FBuildRequestIndex InRequestIndex, float InBoostValue) = 0;
	virtual void ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries) = 0;
	virtual void ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll = false) = 0;

	virtual FGeometryHandle RegisterRayTracingGeometry(FRayTracingGeometry* InGeometry) = 0;
	virtual void ReleaseRayTracingGeometryHandle(FGeometryHandle Handle) = 0;

	/*
	* RayTracing::FGeometryGroupHandle is used to group multiple FRayTracingGeometry that are associated with the same asset.
	* For example, the FRayTracingGeometry of all the LODs of UStaticMesh should use the same RayTracing::FGeometryGroupHandle.
	* This grouping is useful to keep track which proxies need to be invalidated when a FRayTracingGeometry is built or made resident.
	*/
	virtual RayTracing::FGeometryGroupHandle RegisterRayTracingGeometryGroup(uint32 NumLODs, uint32 CurrentFirstLODIdx = 0) = 0;
	virtual void ReleaseRayTracingGeometryGroup(RayTracing::FGeometryGroupHandle Handle) = 0;

	virtual void RequestUpdateCachedRenderState(RayTracing::FGeometryGroupHandle InRayTracingGeometryGroupHandle) = 0;

	virtual void RefreshRegisteredGeometry(FGeometryHandle Handle) = 0;

	virtual void PreRender() = 0;
	virtual void Tick(FRHICommandList& RHICmdList) = 0;

	virtual bool IsGeometryVisible(FGeometryHandle GeometryHandle) const = 0;
	virtual void AddVisibleGeometry(FGeometryHandle GeometryHandle) = 0;
};

extern RENDERCORE_API IRayTracingGeometryManager* GRayTracingGeometryManager;

#endif // RHI_RAYTRACING
