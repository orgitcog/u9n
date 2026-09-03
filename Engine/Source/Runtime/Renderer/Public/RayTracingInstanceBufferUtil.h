// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphDefinitions.h"
#include "RHIUtilities.h"
#include "CoreMinimal.h"

#if RHI_RAYTRACING

class FGPUScene;
struct FRayTracingCullingParameters;
struct FDFVector3;

struct UE_DEPRECATED(5.6, "Use FRayTracingInstanceBufferBuilder instead.") FRayTracingInstanceDescriptor
{
	uint32 GPUSceneInstanceOrTransformIndex;
	uint32 OutputDescriptorIndex;
	uint32 AccelerationStructureIndex;
	uint32 InstanceId;
	uint32 InstanceMaskAndFlags;
	uint32 InstanceContributionToHitGroupIndex;
	union
	{
		uint32 SceneInstanceIndexAndApplyLocalBoundsTransform;
		uint32 bApplyLocalBoundsTransform;
	};
};

struct UE_DEPRECATED(5.6, "Use FRayTracingInstanceBufferBuilder instead.") FRayTracingInstanceGroupEntryRef
{
	uint32 GroupIndex;
	uint32 BaseIndexInGroup;
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS
struct UE_DEPRECATED(5.6, "Use FRayTracingInstanceBufferBuilder instead.") FRayTracingSceneInitializationData
{
	uint32 NumGPUInstanceGroups;
	uint32 NumCPUInstanceGroups;
	uint32 NumGPUInstanceDescriptors;
	uint32 NumCPUInstanceDescriptors;
	uint32 NumNativeGPUSceneInstances;
	uint32 NumNativeCPUInstances;

	UE_DEPRECATED(5.6, "No longer used. Use FShaderBindingTable instead.")
	uint32 TotalNumSegments;

	// index of each instance geometry in ReferencedGeometries
	TArray<uint32> InstanceGeometryIndices;
	// base offset of each instance entries in the instance upload buffer
	TArray<uint32> BaseUploadBufferOffsets;
	// prefix sum of `Instance.NumTransforms` for all instances in this scene
	TArray<uint32> BaseInstancePrefixSum;
	// reference to corresponding instance group entry
	TArray<FRayTracingInstanceGroupEntryRef> InstanceGroupEntryRefs;

	// Unique list of geometries referenced by all instances in this scene.
	// Any referenced geometry is kept alive while the scene is alive.
	TArray<FRHIRayTracingGeometry*> ReferencedGeometries;

	UE_DEPRECATED(5.5, "No longer used as 'inline SBT' is managed using FShaderBindingTable.")
	TArray<FRHIRayTracingGeometry*> PerInstanceGeometries;

	FRayTracingSceneInitializationData() = default;
	FRayTracingSceneInitializationData(const FRayTracingSceneInitializationData&) = default;
	FRayTracingSceneInitializationData& operator=(const FRayTracingSceneInitializationData&) = default;
	FRayTracingSceneInitializationData(FRayTracingSceneInitializationData&&) = default;
	FRayTracingSceneInitializationData& operator=(FRayTracingSceneInitializationData&&) = default;
	~FRayTracingSceneInitializationData() = default;
};

UE_DEPRECATED(5.6, "Use FRayTracingInstanceBufferBuilder instead.")
RENDERER_API FRayTracingSceneInitializationData BuildRayTracingSceneInitializationData(TConstArrayView<FRayTracingGeometryInstance> Instances);

UE_DEPRECATED(5.6, "Use FRayTracingInstanceBufferBuilder instead.")
RENDERER_API void FillRayTracingInstanceUploadBuffer(
	FRayTracingSceneRHIRef RayTracingSceneRHI,
	FVector PreViewTranslation,
	TConstArrayView<FRayTracingGeometryInstance> Instances,
	TConstArrayView<uint32> InstanceGeometryIndices,
	TConstArrayView<uint32> BaseUploadBufferOffsets,
	TConstArrayView<uint32> BaseInstancePrefixSum,
	uint32 NumNativeGPUSceneInstances,
	uint32 NumNativeCPUInstances,
	TArrayView<FRayTracingInstanceDescriptor> OutInstanceUploadData,
	TArrayView<FVector4f> OutTransformData);

UE_DEPRECATED(5.6, "Use FRayTracingInstanceBufferBuilder instead.")
RENDERER_API void BuildRayTracingInstanceBuffer(
	FRHICommandList& RHICmdList,
	const FGPUScene* GPUScene,
	const FDFVector3& PreViewTranslation,
	FRHIUnorderedAccessView* InstancesUAV,
	FRHIShaderResourceView* InstanceUploadSRV,
	FRHIShaderResourceView* AccelerationStructureAddressesSRV,
	FRHIShaderResourceView* CPUInstanceTransformSRV,
	uint32 NumNativeGPUSceneInstances,
	uint32 NumNativeCPUInstances,
	const FRayTracingCullingParameters* CullingParameters,
	FRHIUnorderedAccessView* OutputStatsUAV,
	FRHIUnorderedAccessView* InstanceExtraDataUAV);

PRAGMA_ENABLE_DEPRECATION_WARNINGS

class FRayTracingInstanceBufferBuilder
{
public:

	RENDERER_API void Init(TConstArrayView<FRayTracingGeometryInstance> InInstances, FVector InPreViewTranslation);

	RENDERER_API void FillRayTracingInstanceUploadBuffer(FRHICommandList& RHICmdList);
	RENDERER_API void FillAccelerationStructureAddressesBuffer(FRHICommandList& RHICmdList);

	RENDERER_API void BuildRayTracingInstanceBuffer(
		FRHICommandList& RHICmdList,
		const FGPUScene* GPUScene,
		const FRayTracingCullingParameters* CullingParameters,
		FRHIUnorderedAccessView* InstancesUAV,
		uint32 MaxNumInstances,
		bool bCompactOutput,
		FRHIUnorderedAccessView* OutputStatsUAV,
		uint32 OutputStatsOffset,
		FRHIUnorderedAccessView* InstanceExtraDataUAV);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	RENDERER_API uint32 GetMaxNumInstances() const { return Data.NumNativeGPUSceneInstances + Data.NumNativeCPUInstances; }

	RENDERER_API TConstArrayView<FRHIRayTracingGeometry*> GetReferencedGeometries() const { return Data.ReferencedGeometries; }
	RENDERER_API TConstArrayView<uint32> GetInstanceGeometryIndices() const { return Data.InstanceGeometryIndices; }
	RENDERER_API TConstArrayView<uint32> GetBaseInstancePrefixSum() const { return Data.BaseInstancePrefixSum; }

	PRAGMA_ENABLE_DEPRECATION_WARNINGS

private:

	TConstArrayView<FRayTracingGeometryInstance> Instances;
	FVector PreViewTranslation;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FRayTracingSceneInitializationData Data;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FBufferRHIRef InstanceGroupUploadBuffer;
	FShaderResourceViewRHIRef InstanceGroupUploadSRV;

	FBufferRHIRef InstanceUploadBuffer;
	FShaderResourceViewRHIRef InstanceUploadSRV;

	FBufferRHIRef TransformUploadBuffer;
	FShaderResourceViewRHIRef TransformUploadSRV;

	FByteAddressBuffer AccelerationStructureAddressesBuffer;
};

#endif // RHI_RAYTRACING