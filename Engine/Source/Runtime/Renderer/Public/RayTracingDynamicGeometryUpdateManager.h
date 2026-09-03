// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "RHI.h"
#include "RHIUtilities.h"
#include "RenderGraphDefinitions.h"
#include "MeshPassProcessor.h"

class FPrimitiveSceneProxy;
class FScene;
class FSceneView;
class FViewInfo;
class FRDGBuilder;
class FRayTracingGeometry;
class FRayTracingDynamicGeometryConverterCS;
struct FRayTracingDynamicGeometryUpdateParams;

DECLARE_UNIFORM_BUFFER_STRUCT(FSceneUniformParameters, RENDERER_API)

struct FMeshComputeDispatchCommand
{
	FMeshDrawShaderBindings ShaderBindings;
	TShaderRef<FRayTracingDynamicGeometryConverterCS> MaterialShader;

	uint32 NumThreads;
	uint32 NumCPUVertices;
	FRWBuffer* TargetBuffer;
};

class FRayTracingDynamicGeometryUpdateManager
{
public:

	RENDERER_API FRayTracingDynamicGeometryUpdateManager();
	virtual RENDERER_API ~FRayTracingDynamicGeometryUpdateManager();
	
	/** Add dynamic geometry to update including CS shader to deform the vertices */
	RENDERER_API void AddDynamicGeometryToUpdate(
		FRHICommandListBase& RHICmdList,
		const FScene* Scene, 
		const FSceneView* View, 
		const FPrimitiveSceneProxy* PrimitiveSceneProxy, 
		const FRayTracingDynamicGeometryUpdateParams& Params,
		uint32 PrimitiveId 
	);

	/** Starts an update batch and returns the current shared buffer generation ID which is used for validation. */
	RENDERER_API int64 BeginUpdate();

	/** Add RDG pass which will dispatch all dynamic geometry vertex updates and then request BLAS build and update for all pending requests */
	RENDERER_API void AddDynamicGeometryUpdatePass(
		FRDGBuilder& GraphBuilder,
		ERDGPassFlags ComputePassFlags,
		const TRDGUniformBufferRef<FSceneUniformParameters>& SceneUB,
		bool bUseTracingFeedback, 
		ERHIPipeline ResourceAccessPipelines,
		FRDGBufferRef& OutDynamicGeometryScratchBuffer);

	UE_DEPRECATED(5.6, "Provide SceneUB and bUseTracingFeedback instead of View.")
	RENDERER_API void AddDynamicGeometryUpdatePass(const FViewInfo& View, FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags, ERHIPipeline ResourceAccessPipelines, FRDGBufferRef& OutDynamicGeometryScratchBuffer);

	/** Clears the working arrays to not hold any references. */
	RENDERER_API void Clear();

	UE_DEPRECATED(5.5, "Use AddDynamicGeometryUpdatePass instead.")
	RENDERER_API void DispatchUpdates(FRHICommandList& RHICmdList, FRHIBuffer* ScratchBuffer, ERHIPipeline SrcResourceAccessPipelines = ERHIPipeline::Graphics, ERHIPipeline DstResourceAccessPipelines = ERHIPipeline::Graphics);

	UE_DEPRECATED(5.5, "Use AddDynamicGeometryUpdatePass instead.")
	RENDERER_API void EndUpdate();

	UE_DEPRECATED(5.5, "Use AddDynamicGeometryUpdatePass instead which allocates scratch buffer internally.")
	RENDERER_API uint32 ComputeScratchBufferSize();

	UE_DEPRECATED(5.6, "Use AddDynamicGeometryUpdatePass(...) which internally handles the full update.")
	RENDERER_API void Update(const FViewInfo& View);
private:

	void ScheduleUpdates(bool bUseTracingFeedback);
	static void DispatchUpdates(FRHICommandList& RHICmdList, TConstArrayView<FMeshComputeDispatchCommand> DispatchCommands, ERHIPipeline SrcResourceAccessPipelines = ERHIPipeline::Graphics, ERHIPipeline DstResourceAccessPipelines = ERHIPipeline::Graphics);
	
	struct FRayTracingDynamicGeometryBuildParams
	{
		TArray<FMeshComputeDispatchCommand> DispatchCommands;
		FRHIUniformBuffer* ViewUniformBuffer = nullptr;
		FRayTracingGeometry* Geometry = nullptr;
		int32 SegmentOffset = -1;
	};

	void AddDispatchCommands(FRHICommandListBase& RHICmdList,
							 const FScene* Scene, 
	                         const FSceneView* View, 
	                         const FPrimitiveSceneProxy* PrimitiveSceneProxy, 
	                         const FRayTracingDynamicGeometryUpdateParams& Params,
	                         uint32 PrimitiveId,
							 FRWBuffer* RWBuffer,
							 uint32 NumVertices,
							 uint32 VertexBufferOffset,
							 uint32 VertexBufferSize,
	                         FRayTracingDynamicGeometryBuildParams& BuildParams);
	FRWBuffer* AllocateSharedBuffer(FRHICommandListBase& RHICmdList, uint32 VertexBufferSize, uint32& OutVertexBufferOffset);


	TArray<FRayTracingDynamicGeometryBuildParams> DynamicGeometryBuilds;
	TArray<FRayTracingDynamicGeometryBuildParams> DynamicGeometryUpdates;

	// Group dispatch commands per view uniform buffer since it is specified when creating the RDG passes
	TMap<FRHIUniformBuffer*, TArray<FMeshComputeDispatchCommand>> DispatchCommandsPerView;
	TArray<FRayTracingGeometryBuildParams> BuildParams;
	TArray<FRayTracingGeometrySegment> Segments;

	struct FVertexPositionBuffer
	{
		FRWBuffer RWBuffer;
		uint32 UsedSize = 0;
		uint32 LastUsedGenerationID = 0;
	};
	TArray<FVertexPositionBuffer*> VertexPositionBuffers;

	// Any uniform buffers that must be kept alive until EndUpdate (after DispatchUpdates is called)
	TArray<FUniformBufferRHIRef> ReferencedUniformBuffers;

	// Generation ID when the shared vertex buffers have been reset. The current generation ID is stored in the FRayTracingGeometry to keep track
	// if the vertex buffer data is still valid for that frame - validated before generation the TLAS
	int64 SharedBufferGenerationID = 0;

	uint32 ScratchBufferSize = 0;
};

UE_DEPRECATED(5.6, "Use FRayTracingDynamicGeometryUpdateManager instead of FRayTracingDynamicGeometryCollection");
class FRayTracingDynamicGeometryCollection : public FRayTracingDynamicGeometryUpdateManager
{
};

#endif // RHI_RAYTRACING