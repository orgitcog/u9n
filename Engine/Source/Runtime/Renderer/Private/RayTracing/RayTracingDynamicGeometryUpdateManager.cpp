// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingDynamicGeometryUpdateManager.h"
#include "MeshMaterialShader.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ScenePrivate.h"
#include "RayTracingInstance.h"
#include "RayTracingGeometry.h"
#include "RenderGraphBuilder.h"
#include "PSOPrecacheMaterial.h"
#include "PSOPrecacheValidation.h"
#include "Rendering/RayTracingGeometryManager.h"

#if RHI_RAYTRACING

#include "Materials/MaterialRenderProxy.h"

DECLARE_GPU_STAT(RayTracingDynamicGeometry);

DECLARE_DWORD_COUNTER_STAT(TEXT("Ray tracing dynamic build primitives"), STAT_RayTracingDynamicBuildPrimitives, STATGROUP_SceneRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Ray tracing dynamic update primitives"), STAT_RayTracingDynamicUpdatePrimitives, STATGROUP_SceneRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Ray tracing dynamic skipped primitives"), STAT_RayTracingDynamicSkippedPrimitives, STATGROUP_SceneRendering);

static int32 GRTDynGeomSharedVertexBufferSizeInMB = 4;
static FAutoConsoleVariableRef CVarRTDynGeomSharedVertexBufferSizeInMB(
	TEXT("r.RayTracing.DynamicGeometry.SharedVertexBufferSizeInMB"),
	GRTDynGeomSharedVertexBufferSizeInMB,
	TEXT("Size of the a single shared vertex buffer used during the BLAS update of dynamic geometries (default 4MB)"),
	ECVF_RenderThreadSafe
);

static int32 GRTDynGeomSharedVertexBufferGarbageCollectLatency = 30;
static FAutoConsoleVariableRef CVarRTDynGeomSharedVertexBufferGarbageCollectLatency(
	TEXT("r.RayTracing.DynamicGeometry.SharedVertexBufferGarbageCollectLatency"),
	GRTDynGeomSharedVertexBufferGarbageCollectLatency,
	TEXT("Amount of update cycles before a heap is deleted when not used (default 30)."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRTDynGeomMaxUpdatePrimitivesPerFrame(
	TEXT("r.RayTracing.DynamicGeometry.MaxUpdatePrimitivesPerFrame"),
	-1,
	TEXT("Sets the dynamic ray tracing acceleration structure build budget in terms of maximum number of updated triangles per frame (<= 0 then disabled and all acceleration structures are updated - default)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRTDynGeomForceBuildMaxUpdatePrimitivesPerFrame(
	TEXT("r.RayTracing.DynamicGeometry.ForceBuild.MaxPrimitivesPerFrame"),
	0,
	TEXT("Sets the dynamic ray tracing acceleration structure build budget in terms of maximum number of triangles that are rebuild per frame (default 0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRTDynGeomForceBuildMinUpdatesSinceLastBuild(
	TEXT("r.RayTracing.DynamicGeometry.ForceBuild.MinUpdatesSinceLastBuild"),
	-1,
	TEXT("Sets minimum number of updates before the dynamic geometry acceleration structure will be considered for rebuild (default INT_MAX)"),
	ECVF_RenderThreadSafe
);

class FRayTracingDynamicGeometryConverterCS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FRayTracingDynamicGeometryConverterCS, MeshMaterial);

	class FVertexMask : SHADER_PERMUTATION_BOOL("USE_VERTEX_MASK");

	using FPermutationDomain = TShaderPermutationDomain<FVertexMask>;

public:
	FRayTracingDynamicGeometryConverterCS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTextureUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());

		RWVertexPositions.Bind(Initializer.ParameterMap, TEXT("RWVertexPositions"));
		UsingIndirectDraw.Bind(Initializer.ParameterMap, TEXT("UsingIndirectDraw"));
		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
		MaxNumThreads.Bind(Initializer.ParameterMap, TEXT("MaxNumThreads"));
		MinVertexIndex.Bind(Initializer.ParameterMap, TEXT("MinVertexIndex"));
		PrimitiveId.Bind(Initializer.ParameterMap, TEXT("PrimitiveId"));
		OutputVertexBaseIndex.Bind(Initializer.ParameterMap, TEXT("OutputVertexBaseIndex"));
		bApplyWorldPositionOffset.Bind(Initializer.ParameterMap, TEXT("bApplyWorldPositionOffset"));
		InstanceId.Bind(Initializer.ParameterMap, TEXT("InstanceId"));
		WorldToInstance.Bind(Initializer.ParameterMap, TEXT("WorldToInstance"));
		IndexBuffer.Bind(Initializer.ParameterMap, TEXT("IndexBuffer"));
		IndexBufferOffset.Bind(Initializer.ParameterMap, TEXT("IndexBufferOffset"));
	}

	FRayTracingDynamicGeometryConverterCS() = default;

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (!IsRayTracingEnabledForProject(Parameters.Platform))
		{
			return false;
		}

		if (!Parameters.VertexFactoryType->SupportsRayTracingDynamicGeometry())
		{
			return false;
		}

		if (PermutationVector.Get<FVertexMask>())
		{
			return Parameters.MaterialParameters.BlendMode == BLEND_Masked;
		}
		
		return true;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), 1);
		OutEnvironment.SetDefine(TEXT("RAYTRACING_DYNAMIC_GEOMETRY_CONVERTER"), 1);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);
	}

	void GetElementShaderBindings(
		const FShaderMapPointerTable& PointerTable,
		const FScene* Scene,
		const FSceneView* ViewIfDynamicMeshCommand,
		const FVertexFactory* VertexFactory,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& MeshBatch, 
		const FMeshBatchElement& BatchElement,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		FMeshMaterialShader::GetElementShaderBindings(PointerTable, Scene, ViewIfDynamicMeshCommand, VertexFactory, InputStreamType, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, ShaderBindings, VertexStreams);
	}

	LAYOUT_FIELD(FShaderResourceParameter, RWVertexPositions);
	LAYOUT_FIELD(FShaderParameter, UsingIndirectDraw);
	LAYOUT_FIELD(FShaderParameter, MaxNumThreads);
	LAYOUT_FIELD(FShaderParameter, NumVertices);
	LAYOUT_FIELD(FShaderParameter, MinVertexIndex);
	LAYOUT_FIELD(FShaderParameter, PrimitiveId);
	LAYOUT_FIELD(FShaderParameter, bApplyWorldPositionOffset);
	LAYOUT_FIELD(FShaderParameter, OutputVertexBaseIndex);
	LAYOUT_FIELD(FShaderParameter, InstanceId);
	LAYOUT_FIELD(FShaderParameter, WorldToInstance);
	LAYOUT_FIELD(FShaderResourceParameter, IndexBuffer);
	LAYOUT_FIELD(FShaderParameter, IndexBufferOffset);
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FRayTracingDynamicGeometryConverterCS, TEXT("/Engine/Private/RayTracing/RayTracingDynamicMesh.usf"), TEXT("RayTracingDynamicGeometryConverterCS"), SF_Compute);

static const TCHAR* RayTracingDynamicGeometryPSOCollectorName = TEXT("RayTracingDynamicGeometry");

class FRayTracingDynamicGeometryPSOCollector : public IPSOCollector
{
public:
	FRayTracingDynamicGeometryPSOCollector(ERHIFeatureLevel::Type InFeatureLevel) : 
		IPSOCollector(FPSOCollectorCreateManager::GetIndex(GetFeatureLevelShadingPath(InFeatureLevel), RayTracingDynamicGeometryPSOCollectorName)),
		FeatureLevel(InFeatureLevel)
	{
	}

	virtual void CollectPSOInitializers(
		const FSceneTexturesConfig& SceneTexturesConfig,
		const FMaterial& Material,
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		const FPSOPrecacheParams& PreCacheParams,
		TArray<FPSOPrecacheData>& PSOInitializers
	) override final;

private:

	ERHIFeatureLevel::Type FeatureLevel;
};


void FRayTracingDynamicGeometryPSOCollector::CollectPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& Material,
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FPSOPrecacheParams& PreCacheParams,
	TArray<FPSOPrecacheData>& PSOInitializers
)
{
	if (!VertexFactoryData.VertexFactoryType->SupportsRayTracingDynamicGeometry())
	{
		return;
	}

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FRayTracingDynamicGeometryConverterCS>();

	FMaterialShaders MaterialShaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryData.VertexFactoryType, MaterialShaders))
	{
		return;
	}

	TShaderRef<FRayTracingDynamicGeometryConverterCS> Shader;
	if (!MaterialShaders.TryGetShader(SF_Compute, Shader))
	{
		return;
	}

	FPSOPrecacheData RTPrecacheData;
	RTPrecacheData.Type = FPSOPrecacheData::EType::Compute;
	RTPrecacheData.SetComputeShader(Shader);
#if PSO_PRECACHING_VALIDATE
	RTPrecacheData.PSOCollectorIndex = PSOCollectorIndex;
	RTPrecacheData.VertexFactoryType = VertexFactoryData.VertexFactoryType;
#endif // PSO_PRECACHING_VALIDATE
	PSOInitializers.Add(MoveTemp(RTPrecacheData));
}

IPSOCollector* CreateRayTracingDynamicGeometryPSOCollector(ERHIFeatureLevel::Type FeatureLevel)
{
	return new FRayTracingDynamicGeometryPSOCollector(FeatureLevel);
}
FRegisterPSOCollectorCreateFunction RegisterRayTracingDynamicGeometryPSOCollector(&CreateRayTracingDynamicGeometryPSOCollector, EShadingPath::Deferred, RayTracingDynamicGeometryPSOCollectorName);

FRayTracingDynamicGeometryUpdateManager::FRayTracingDynamicGeometryUpdateManager() 
{
}

FRayTracingDynamicGeometryUpdateManager::~FRayTracingDynamicGeometryUpdateManager()
{
	for (FVertexPositionBuffer* Buffer : VertexPositionBuffers)
	{
		delete Buffer;
	}
	VertexPositionBuffers.Empty();
}

void FRayTracingDynamicGeometryUpdateManager::Clear()
{
	DispatchCommandsPerView = {};

	// Clear working arrays - keep max size allocated
	BuildParams.Empty(BuildParams.Max());
	Segments.Empty(Segments.Max());

	DynamicGeometryBuilds.Empty(DynamicGeometryBuilds.Max());
	DynamicGeometryUpdates.Empty(DynamicGeometryUpdates.Max());

	ScratchBufferSize = 0;
}

int64 FRayTracingDynamicGeometryUpdateManager::BeginUpdate()
{
	check(DispatchCommandsPerView.IsEmpty());
	check(BuildParams.IsEmpty());
	check(Segments.IsEmpty());
	check(ReferencedUniformBuffers.IsEmpty());
	check(DynamicGeometryBuilds.IsEmpty());
	check(DynamicGeometryUpdates.IsEmpty());

	// Vertex buffer data can be immediatly reused the next frame, because it's already 'consumed' for building the AccelerationStructure data
	// Garbage collect unused buffers for n generations
	for (int32 BufferIndex = 0; BufferIndex < VertexPositionBuffers.Num(); ++BufferIndex)
	{
		FVertexPositionBuffer* Buffer = VertexPositionBuffers[BufferIndex];
		Buffer->UsedSize = 0;

		if (Buffer->LastUsedGenerationID + GRTDynGeomSharedVertexBufferGarbageCollectLatency <= SharedBufferGenerationID)
		{
			VertexPositionBuffers.RemoveAtSwap(BufferIndex);
			delete Buffer;
			BufferIndex--;
		}
	}

	// Increment generation ID used for validation
	SharedBufferGenerationID++;

	return SharedBufferGenerationID;
}

void FRayTracingDynamicGeometryUpdateManager::AddDynamicGeometryToUpdate(
	FRHICommandListBase& RHICmdList,
	const FScene* Scene,
	const FSceneView* View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FRayTracingDynamicGeometryUpdateParams& UpdateParams,
	uint32 PrimitiveId
)
{
	FRayTracingGeometry& Geometry = *UpdateParams.Geometry;

	uint32 NumVertices = UpdateParams.NumVertices;
	uint32 VertexBufferSize = UpdateParams.VertexBufferSize;

	if (UpdateParams.bAlphaMasked)
	{
		check(UpdateParams.IndexBuffer);
		check(!UpdateParams.bUsingIndirectDraw);

		NumVertices = UpdateParams.NumTriangles * 3;
		VertexBufferSize = NumVertices * (uint32)sizeof(FVector3f);
	}	

	FRWBuffer* RWBuffer = UpdateParams.Buffer;
	uint32 VertexBufferOffset = 0;
	bool bUseSharedVertexBuffer = false;

	if (ReferencedUniformBuffers.Num() == 0 || ReferencedUniformBuffers.Last() != View->ViewUniformBuffer)
	{
		// Keep ViewUniformBuffer alive until EndUpdate()
		ReferencedUniformBuffers.Add(View->ViewUniformBuffer);
	}

	FRayTracingDynamicGeometryBuildParams GeometryBuildParams;
	GeometryBuildParams.ViewUniformBuffer = View->ViewUniformBuffer;
	GeometryBuildParams.DispatchCommands.Reserve(UpdateParams.MeshBatches.Num());

	// Only update when we have mesh batches
	if (!UpdateParams.MeshBatches.IsEmpty())
	{
		// If update params didn't provide a buffer then use a shared vertex position buffer
		if (RWBuffer == nullptr)
		{
			RWBuffer = AllocateSharedBuffer(RHICmdList, VertexBufferSize, VertexBufferOffset);
			bUseSharedVertexBuffer = true;
		}
		check(IsAligned(VertexBufferOffset, RHI_RAW_VIEW_ALIGNMENT));
		
		AddDispatchCommands(RHICmdList, Scene, View, PrimitiveSceneProxy, UpdateParams, PrimitiveId, RWBuffer, NumVertices, VertexBufferOffset, VertexBufferSize, GeometryBuildParams);
	}

	bool bRefit = true;

	// Optionally resize the buffer when not shared (could also be lazy allocated and still empty)
	if (!bUseSharedVertexBuffer && RWBuffer && RWBuffer->NumBytes != VertexBufferSize)
	{
		RWBuffer->Initialize(RHICmdList, TEXT("FRayTracingDynamicGeometryUpdateManager::RayTracingDynamicVertexBuffer"), sizeof(float), VertexBufferSize / sizeof(float), PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);
		bRefit = false;
	}

	if (!Geometry.IsValid() || Geometry.IsEvicted())
	{
		bRefit = false;
	}

	if (!Geometry.Initializer.bAllowUpdate)
	{
		bRefit = false;
	}

	check(Geometry.IsInitialized());

	if (Geometry.Initializer.TotalPrimitiveCount != UpdateParams.NumTriangles && UpdateParams.NumTriangles > 0)
	{
		checkf(Geometry.Initializer.Segments.Num() <= 1, TEXT("Dynamic ray tracing geometry '%s' has an unexpected number of segments."), *Geometry.Initializer.DebugName.ToString());
		Geometry.Initializer.TotalPrimitiveCount = UpdateParams.NumTriangles;
		Geometry.Initializer.Segments.Empty();
		FRayTracingGeometrySegment Segment;
		Segment.NumPrimitives = UpdateParams.NumTriangles;
		Segment.MaxVertices = NumVertices;
		Geometry.Initializer.Segments.Add(Segment);
		bRefit = false;
	}

	if (UpdateParams.bAlphaMasked)
	{
		Geometry.Initializer.IndexBuffer = nullptr;
	}

	if (RWBuffer)
	{
		for (FRayTracingGeometrySegment& Segment : Geometry.Initializer.Segments)
		{
			Segment.VertexBuffer = RWBuffer->Buffer;
			Segment.VertexBufferOffset = VertexBufferOffset;
		}
	}

	if (!bRefit)
	{
		checkf(Geometry.RawData.IsEmpty() && Geometry.Initializer.OfflineData == nullptr, TEXT("Dynamic geometry is not expected to have offline acceleration structure data"));
		Geometry.CreateRayTracingGeometry(RHICmdList, ERTAccelerationStructureBuildPriority::Skip);
	}

	EAccelerationStructureBuildMode BuildMode = Geometry.GetRequiresBuild()
		? EAccelerationStructureBuildMode::Build
		: EAccelerationStructureBuildMode::Update;

	GeometryBuildParams.Geometry = UpdateParams.Geometry;

	if (bUseSharedVertexBuffer)
	{
		GeometryBuildParams.SegmentOffset = Segments.Num();
		Segments.Append(Geometry.Initializer.Segments);
	}

	Geometry.SetRequiresBuild(false);

	if (BuildMode == EAccelerationStructureBuildMode::Build)
	{
		DynamicGeometryBuilds.Add(GeometryBuildParams);
	}
	else
	{
		DynamicGeometryUpdates.Add(GeometryBuildParams);
	}
	
	if (bUseSharedVertexBuffer)
	{
		Geometry.DynamicGeometrySharedBufferGenerationID = SharedBufferGenerationID;
	}
	else
	{
		Geometry.DynamicGeometrySharedBufferGenerationID = FRayTracingGeometry::NonSharedVertexBuffers;
	}
}

FRWBuffer* FRayTracingDynamicGeometryUpdateManager::AllocateSharedBuffer(FRHICommandListBase& RHICmdList, uint32 VertexBufferSize, uint32& OutVertexBufferOffset)
{
	FVertexPositionBuffer* VertexPositionBuffer = nullptr;
	for (FVertexPositionBuffer* Buffer : VertexPositionBuffers)
	{
		if (Buffer->RWBuffer.NumBytes >= (VertexBufferSize + Buffer->UsedSize))
		{
			VertexPositionBuffer = Buffer;
			break;
		}
	}

	// Allocate a new buffer?
	if (VertexPositionBuffer == nullptr)
	{
		VertexPositionBuffer = new FVertexPositionBuffer;
		VertexPositionBuffers.Add(VertexPositionBuffer);

		static const uint32 VertexBufferCacheSize = GRTDynGeomSharedVertexBufferSizeInMB * 1024 * 1024;
		uint32 AllocationSize = FMath::Max(VertexBufferCacheSize, VertexBufferSize);

		VertexPositionBuffer->RWBuffer.Initialize(RHICmdList, TEXT("FRayTracingDynamicGeometryUpdateManager::RayTracingDynamicVertexBuffer"), sizeof(float), AllocationSize / sizeof(float), PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);
		VertexPositionBuffer->UsedSize = 0;
	}

	// Update the last used generation ID
	VertexPositionBuffer->LastUsedGenerationID = SharedBufferGenerationID;

	// Get the offset and update used size
	OutVertexBufferOffset = VertexPositionBuffer->UsedSize;
	VertexPositionBuffer->UsedSize += VertexBufferSize;

	// Make sure vertex buffer offset is aligned to 16 (required for Raw SRV views)
	VertexPositionBuffer->UsedSize = Align(VertexPositionBuffer->UsedSize, RHI_RAW_VIEW_ALIGNMENT);

	return &VertexPositionBuffer->RWBuffer;
}

void FRayTracingDynamicGeometryUpdateManager::AddDispatchCommands(
	FRHICommandListBase& RHICmdList,
	const FScene* Scene, 
	const FSceneView* View, 
	const FPrimitiveSceneProxy* PrimitiveSceneProxy, 
	const FRayTracingDynamicGeometryUpdateParams& UpdateParams,
	uint32 PrimitiveId,
	FRWBuffer* RWBuffer,
	uint32 NumVertices,
	uint32 VertexBufferOffset,
	uint32 VertexBufferSize,
	FRayTracingDynamicGeometryUpdateManager::FRayTracingDynamicGeometryBuildParams& GeometryBuildParams)
{	
	const int32 PSOCollectorIndex = FPSOCollectorCreateManager::GetIndex(EShadingPath::Deferred, RayTracingDynamicGeometryPSOCollectorName);

	for (const FMeshBatch& MeshBatch : UpdateParams.MeshBatches)
	{
		if (!ensureMsgf(MeshBatch.VertexFactory->GetType()->SupportsRayTracingDynamicGeometry(),
		                TEXT("FRayTracingDynamicGeometryConverterCS doesn't support %s. Skipping rendering of %s.  This can happen when the skinning cache runs out of space and falls back to GPUSkinVertexFactory."),
		                MeshBatch.VertexFactory->GetType()->GetName(), *PrimitiveSceneProxy->GetOwnerName().ToString()))
		{
			continue;
		}

		const FMaterialRenderProxy* MaterialRenderProxyPtr = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxyPtr)
		{
			const FMaterial* MaterialPtr = MaterialRenderProxyPtr->GetMaterialNoFallback(Scene->GetFeatureLevel());
			if (MaterialPtr && MaterialPtr->GetRenderingThreadShaderMap())
			{
				const FMaterial& Material = *MaterialPtr;
				const FMaterialRenderProxy& MaterialRenderProxy = *MaterialRenderProxyPtr;

				auto* MaterialInterface = Material.GetMaterialInterface();

				FMeshComputeDispatchCommand DispatchCmd;

				FRayTracingDynamicGeometryConverterCS::FPermutationDomain PermutationVectorCS;
				PermutationVectorCS.Set<FRayTracingDynamicGeometryConverterCS::FVertexMask>(UpdateParams.bAlphaMasked);

				FMaterialShaderTypes ShaderTypes;
				ShaderTypes.AddShaderType<FRayTracingDynamicGeometryConverterCS>(PermutationVectorCS.ToDimensionValueId());

				FMaterialShaders MaterialShaders;
				if (Material.TryGetShaders(ShaderTypes, MeshBatch.VertexFactory->GetType(), MaterialShaders))
				{
					TShaderRef<FRayTracingDynamicGeometryConverterCS> Shader;
					MaterialShaders.TryGetShader(SF_Compute, Shader);

					FMeshProcessorShaders MeshProcessorShaders;
					MeshProcessorShaders.ComputeShader = Shader;

					DispatchCmd.MaterialShader = Shader;
					FMeshDrawShaderBindings& ShaderBindings = DispatchCmd.ShaderBindings;
					ShaderBindings.Initialize(MeshProcessorShaders);

					FMeshMaterialShaderElementData ShaderElementData;
					ShaderElementData.InitializeMeshMaterialData(View, PrimitiveSceneProxy, MeshBatch, -1, false);

					FMeshDrawSingleShaderBindings SingleShaderBindings = ShaderBindings.GetSingleShaderBindings(SF_Compute);
					Shader->GetShaderBindings(Scene, Scene->GetFeatureLevel(), PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, SingleShaderBindings);

					FVertexInputStreamArray DummyArray;
					FMeshMaterialShader::GetElementShaderBindings(Shader, Scene, View, MeshBatch.VertexFactory, EVertexInputStreamType::Default, Scene->GetFeatureLevel(), PrimitiveSceneProxy, MeshBatch, MeshBatch.Elements[0], ShaderElementData, SingleShaderBindings, DummyArray);

					DispatchCmd.TargetBuffer = RWBuffer;					

					// Setup the loose parameters directly on the binding
					uint32 OutputVertexBaseIndex = VertexBufferOffset / sizeof(float);
					uint32 MinVertexIndex = MeshBatch.Elements[0].MinVertexIndex;
					uint32 NumCPUVertices = NumVertices;
					if (MeshBatch.Elements[0].MinVertexIndex < MeshBatch.Elements[0].MaxVertexIndex)
					{
						NumCPUVertices = 1 + MeshBatch.Elements[0].MaxVertexIndex - MeshBatch.Elements[0].MinVertexIndex;
					}

					const uint32 VertexBufferNumElements = VertexBufferSize / sizeof(FVector3f) - MinVertexIndex;
					if (!ensureMsgf(NumCPUVertices <= VertexBufferNumElements,
					                TEXT("%s: Vertex buffer contains %d vertices, but RayTracingDynamicGeometryConverterCS dispatch command expects at least %d."),
					                *PrimitiveSceneProxy->GetOwnerName().ToString(), VertexBufferNumElements, NumCPUVertices))
					{
						NumCPUVertices = VertexBufferNumElements;
					}

					DispatchCmd.NumCPUVertices = NumCPUVertices;

					SingleShaderBindings.Add(Shader->UsingIndirectDraw, UpdateParams.bUsingIndirectDraw ? 1 : 0);
					SingleShaderBindings.Add(Shader->NumVertices, NumCPUVertices);
					SingleShaderBindings.Add(Shader->MinVertexIndex, MinVertexIndex);
					SingleShaderBindings.Add(Shader->PrimitiveId, PrimitiveId);
					SingleShaderBindings.Add(Shader->OutputVertexBaseIndex, OutputVertexBaseIndex);
					SingleShaderBindings.Add(Shader->bApplyWorldPositionOffset, UpdateParams.bApplyWorldPositionOffset ? 1 : 0);
					SingleShaderBindings.Add(Shader->InstanceId, UpdateParams.InstanceId);
					SingleShaderBindings.Add(Shader->WorldToInstance, UpdateParams.WorldToInstance);

					if (UpdateParams.bAlphaMasked)
					{
						FRHIBuffer* IndexBufferRHI = UpdateParams.IndexBuffer;

						const uint32 IndexStride = IndexBufferRHI->GetStride();
						const uint32 NumTriangles = UpdateParams.NumTriangles;
						const uint32 IndexBufferOffset = UpdateParams.Geometry->Initializer.IndexBufferOffset / IndexStride + MeshBatch.Elements[0].FirstIndex;

						SingleShaderBindings.Add(Shader->IndexBuffer,
							RHICmdList.CreateShaderResourceView(IndexBufferRHI,
								FRHIViewDesc::CreateBufferSRV()
								.SetType(FRHIViewDesc::EBufferType::Typed)
								.SetFormat(IndexStride == 4 ? PF_R32_UINT : PF_R16_UINT))
						);

						SingleShaderBindings.Add(Shader->MaxNumThreads, NumTriangles);
						SingleShaderBindings.Add(Shader->IndexBufferOffset, IndexBufferOffset);

						DispatchCmd.NumThreads = NumTriangles;
					}
					else
					{
						SingleShaderBindings.Add(Shader->MaxNumThreads, NumCPUVertices);
						SingleShaderBindings.Add(Shader->IndexBufferOffset, 0);

						DispatchCmd.NumThreads = NumVertices;
					}				

				#if MESH_DRAW_COMMAND_DEBUG_DATA
					ShaderBindings.Finalize(&MeshProcessorShaders);
				#endif

				#if PSO_PRECACHING_VALIDATE
					FRHIComputeShader* ComputeShader = DispatchCmd.MaterialShader.GetComputeShader();
					if (ComputeShader != nullptr)
					{
						EPSOPrecacheResult PSOPrecacheResult = PipelineStateCache::CheckPipelineStateInCache(ComputeShader);
						PSOCollectorStats::CheckComputePipelineStateInCache(*ComputeShader, PSOPrecacheResult, &MaterialRenderProxy, PSOCollectorIndex);
					}
				#endif

					GeometryBuildParams.DispatchCommands.Add(DispatchCmd);

					break;
				}
			}

			MaterialRenderProxyPtr = MaterialRenderProxyPtr->GetFallback(Scene->GetFeatureLevel());
		}
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FRayTracingDynamicGeometryUpdatePassParams, )
	RDG_BUFFER_ACCESS(DynamicGeometryScratchBuffer, ERHIAccess::UAVCompute)

	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
END_SHADER_PARAMETER_STRUCT()

void FRayTracingDynamicGeometryUpdateManager::ScheduleUpdates(bool bUseTracingFeedback)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingDynamicGeometryUpdateManager::Update);
	
	// Early out of nothing to do
	const int32 TotalNumGeometryBuilds = DynamicGeometryBuilds.Num() + DynamicGeometryUpdates.Num();
	if (TotalNumGeometryBuilds == 0)
	{
		return;
	}

	checkf(DispatchCommandsPerView.IsEmpty(), TEXT("DispatchCommandsPerView is not empty. Previous frame updates were not dispatched."));
	checkf(BuildParams.IsEmpty(), TEXT("BuildParams is not empty. Previous frame updates were not dispatched."));

	// reserve for worst case (might be wasteful if there are too many views)
	for (FRHIUniformBuffer* ViewUniformBuffer : ReferencedUniformBuffers)
	{
		TArray<FMeshComputeDispatchCommand>& ViewDispatchCommands = DispatchCommandsPerView.FindOrAdd(ViewUniformBuffer);
		ViewDispatchCommands.Reserve(TotalNumGeometryBuilds);
	}

	BuildParams.Reserve(TotalNumGeometryBuilds);

	FRayTracingGeometrySegment* SegmentData = Segments.GetData();

	const uint32 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;

	uint32 BLASScratchSize = 0;
	int32 NumBuildPrimitives = 0;

	auto AddGeometryBuildParamsToBuildList = [this, SegmentData, &BLASScratchSize](const FRayTracingDynamicGeometryBuildParams& InBuildParams)
		{
			FRHIRayTracingGeometry* RayTracingGeometry = InBuildParams.Geometry->GetRHI();

			const uint32 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
			const uint32 NumBuildPrimitives = InBuildParams.Geometry->Initializer.TotalPrimitiveCount;

			const uint32 ScratchSize = RayTracingGeometry->GetSizeInfo().BuildScratchSize;
			BLASScratchSize = Align(BLASScratchSize + ScratchSize, ScratchAlignment);

			InBuildParams.Geometry->NumUpdatesSinceLastBuild = 0;
			InBuildParams.Geometry->SetRequiresUpdate(false);

			FRayTracingGeometryBuildParams RTGeoBuildParams;
			RTGeoBuildParams.Geometry = RayTracingGeometry;
			RTGeoBuildParams.BuildMode = EAccelerationStructureBuildMode::Build;

			if (InBuildParams.SegmentOffset >= 0)
			{
				RTGeoBuildParams.Segments = MakeArrayView(&SegmentData[InBuildParams.SegmentOffset], InBuildParams.Geometry->Initializer.Segments.Num());
			}
			else
			{
				RTGeoBuildParams.Segments = InBuildParams.Geometry->Initializer.Segments;
			}

			BuildParams.Add(MoveTemp(RTGeoBuildParams));

			if (!InBuildParams.DispatchCommands.IsEmpty())
			{
				DispatchCommandsPerView[InBuildParams.ViewUniformBuffer].Append(InBuildParams.DispatchCommands);
			}
		};

	for (const FRayTracingDynamicGeometryBuildParams& Build : DynamicGeometryBuilds)
	{
		AddGeometryBuildParamsToBuildList(Build);
		NumBuildPrimitives += Build.Geometry->Initializer.TotalPrimitiveCount;
	}

	const uint32 MaxUpdatePrimitivesPerFrame = uint32(CVarRTDynGeomMaxUpdatePrimitivesPerFrame.GetValueOnRenderThread());
	const uint32 MaxForceBuildPrimitivesPerFrame = uint32(CVarRTDynGeomForceBuildMaxUpdatePrimitivesPerFrame.GetValueOnRenderThread());
	const uint32 MinUpdatesSinceLastBuild = uint32(CVarRTDynGeomForceBuildMinUpdatesSinceLastBuild.GetValueOnRenderThread());

	uint32 NumUpdatedPrimitives = 0;
	uint32 NumForceBuildPrimitives = 0;

	const bool bNeedsSorting = (int32(MaxUpdatePrimitivesPerFrame) != -1) || (MaxForceBuildPrimitivesPerFrame != 0);
	if (bNeedsSorting)
	{
		DynamicGeometryUpdates.Sort([](const FRayTracingDynamicGeometryBuildParams& InLHS, const FRayTracingDynamicGeometryBuildParams& InRHS)
			{
				if (InLHS.Geometry->LastUpdatedFrame == InRHS.Geometry->LastUpdatedFrame)
				{
					return InLHS.Geometry->NumUpdatesSinceLastBuild > InRHS.Geometry->NumUpdatesSinceLastBuild;
				}

				return InLHS.Geometry->LastUpdatedFrame < InRHS.Geometry->LastUpdatedFrame;
			});
	}	

	for (const FRayTracingDynamicGeometryBuildParams& Update : DynamicGeometryUpdates)
	{
		FRHIRayTracingGeometry* RayTracingGeometry = Update.Geometry->GetRHI();
		const uint32 TotalPrimitiveCount = Update.Geometry->Initializer.TotalPrimitiveCount;

		if (bUseTracingFeedback && !GRayTracingGeometryManager->IsGeometryVisible(Update.Geometry->GetGeometryHandle()))
		{
			INC_DWORD_STAT_BY(STAT_RayTracingDynamicSkippedPrimitives, TotalPrimitiveCount);
			continue;
		}

		if (MaxForceBuildPrimitivesPerFrame > 0)
		{
			if (Update.Geometry->NumUpdatesSinceLastBuild > MinUpdatesSinceLastBuild && NumForceBuildPrimitives <= MaxForceBuildPrimitivesPerFrame)
			{
				AddGeometryBuildParamsToBuildList(Update);
				NumBuildPrimitives += TotalPrimitiveCount;
				NumForceBuildPrimitives += TotalPrimitiveCount;
				continue;
			}
		}

		Update.Geometry->LastUpdatedFrame = GFrameCounterRenderThread;
		Update.Geometry->NumUpdatesSinceLastBuild++;
		Update.Geometry->SetRequiresUpdate(false);

		NumUpdatedPrimitives += TotalPrimitiveCount;

		const uint32 ScratchSize = RayTracingGeometry->GetSizeInfo().UpdateScratchSize;
		BLASScratchSize = Align(BLASScratchSize + ScratchSize, ScratchAlignment);

		FRayTracingGeometryBuildParams BuildParam;
		BuildParam.Geometry = RayTracingGeometry;
		BuildParam.BuildMode = EAccelerationStructureBuildMode::Update;
		if (Update.SegmentOffset >= 0)
		{
			BuildParam.Segments = MakeArrayView(&SegmentData[Update.SegmentOffset], Update.Geometry->Initializer.Segments.Num());
		}
		else
		{
			BuildParam.Segments = Update.Geometry->Initializer.Segments;
		}
		BuildParams.Add(MoveTemp(BuildParam));

		if (!Update.DispatchCommands.IsEmpty())
		{
			DispatchCommandsPerView[Update.ViewUniformBuffer].Append(Update.DispatchCommands);
		}

		if (NumUpdatedPrimitives > MaxUpdatePrimitivesPerFrame)
		{
			break;
		}
	}

	INC_DWORD_STAT_BY(STAT_RayTracingDynamicUpdatePrimitives, NumUpdatedPrimitives);
	INC_DWORD_STAT_BY(STAT_RayTracingDynamicBuildPrimitives, NumBuildPrimitives);

	ScratchBufferSize = BLASScratchSize;
}

void FRayTracingDynamicGeometryUpdateManager::Update(const FViewInfo& View)
{
	ScheduleUpdates(View.bRayTracingFeedbackEnabled);
}

void FRayTracingDynamicGeometryUpdateManager::AddDynamicGeometryUpdatePass(FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags, const TRDGUniformBufferRef<FSceneUniformParameters>& SceneUB, bool bUseTracingFeedback, ERHIPipeline ResourceAccessPipelines, FRDGBufferRef& OutDynamicGeometryScratchBuffer)
{
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());
	RDG_EVENT_SCOPE_STAT(GraphBuilder, RayTracingDynamicGeometry, "RayTracingDynamicGeometry");
	RDG_GPU_STAT_SCOPE(GraphBuilder, RayTracingDynamicGeometry);

	ScheduleUpdates(bUseTracingFeedback);

	const uint32 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
	const uint32 BLASScratchSize = ScratchBufferSize;

	if (BLASScratchSize > 0)
	{
		FRDGBufferDesc ScratchBufferDesc;
		ScratchBufferDesc.Usage = EBufferUsageFlags::RayTracingScratch | EBufferUsageFlags::StructuredBuffer;
		ScratchBufferDesc.BytesPerElement = ScratchAlignment;
		ScratchBufferDesc.NumElements = FMath::DivideAndRoundUp(BLASScratchSize, ScratchAlignment);

		OutDynamicGeometryScratchBuffer = GraphBuilder.CreateBuffer(ScratchBufferDesc, TEXT("DynamicGeometry.BLASSharedScratchBuffer"));
	}

	const ERHIPipeline SrcResourceAccessPipelines = ComputePassFlags == ERDGPassFlags::AsyncCompute ? ERHIPipeline::AsyncCompute : ERHIPipeline::Graphics;

	for(TPair<FRHIUniformBuffer*, TArray<FMeshComputeDispatchCommand>>& ViewDispatchCommands : DispatchCommandsPerView)
	{
		if (ViewDispatchCommands.Value.IsEmpty())
		{
			continue;
		}

		FRayTracingDynamicGeometryUpdatePassParams* PassParams = GraphBuilder.AllocParameters<FRayTracingDynamicGeometryUpdatePassParams>();
		PassParams->View.View = TUniformBufferRef<FViewUniformShaderParameters>(ViewDispatchCommands.Key);
		PassParams->View.InstancedView = TUniformBufferRef<FInstancedViewUniformShaderParameters>(nullptr); // TODO: Is this necessary?
		PassParams->Scene = SceneUB;

		// DynamicGeometryScratchBuffer is not directly used in this pass but set so RDG orders passes correctly
		// (TODO: this might also prevent dispatches for different views from overlapping, so investigate better solution)
		PassParams->DynamicGeometryScratchBuffer = OutDynamicGeometryScratchBuffer; 

		GraphBuilder.AddPass(RDG_EVENT_NAME("RayTracingDynamicUpdate"), PassParams, ComputePassFlags | ERDGPassFlags::NeverCull,
			[this, SrcResourceAccessPipelines, ResourceAccessPipelines, DispatchCommands = MakeArrayView(ViewDispatchCommands.Value)](FRHICommandList& RHICmdList)
			{
				DispatchUpdates(RHICmdList, DispatchCommands, SrcResourceAccessPipelines, ResourceAccessPipelines);
			});
	}

	if (BuildParams.Num() > 0)
	{
		FRayTracingDynamicGeometryUpdatePassParams* PassParams = GraphBuilder.AllocParameters<FRayTracingDynamicGeometryUpdatePassParams>();
		PassParams->View = {};
		PassParams->Scene = nullptr;
		PassParams->DynamicGeometryScratchBuffer = OutDynamicGeometryScratchBuffer;

		GraphBuilder.AddPass(RDG_EVENT_NAME("RayTracingDynamicUpdateBuild"), PassParams, ComputePassFlags | ERDGPassFlags::NeverCull,
			[this, PassParams](FRHICommandList& RHICmdList)
			{
				// Can't use parallel command list because we have to make sure we are not building BVH data
				// on the same RTGeometry on multiple threads at the same time. Ideally move the build
				// requests over to the RaytracingGeometry manager so they can be correctly scheduled
				// with other build requests in the engine (see UE-106982)
				SCOPED_DRAW_EVENT(RHICmdList, Build);

				FRHIBufferRange ScratchBufferRange;
				ScratchBufferRange.Buffer = PassParams->DynamicGeometryScratchBuffer ? PassParams->DynamicGeometryScratchBuffer->GetRHI() : nullptr;
				ScratchBufferRange.Offset = 0;
				RHICmdList.BuildAccelerationStructures(BuildParams, ScratchBufferRange);
			});
	}

	// TODO: Is it safe to use a regular task that waits on FRDGBuilder::GetAsyncExecuteTask() here instead?
	// which would allow the passes above to be tagged with FRDGAsyncTask
	GraphBuilder.AddPostExecuteCallback([this]
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			EndUpdate();
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		});
}

void FRayTracingDynamicGeometryUpdateManager::AddDynamicGeometryUpdatePass(const FViewInfo& View, FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags, ERHIPipeline ResourceAccessPipelines, FRDGBufferRef& OutDynamicGeometryScratchBuffer)
{
	AddDynamicGeometryUpdatePass(GraphBuilder, ComputePassFlags, View.GetSceneUniforms().GetBuffer(GraphBuilder), View.bRayTracingFeedbackEnabled, ResourceAccessPipelines, OutDynamicGeometryScratchBuffer);
}

void FRayTracingDynamicGeometryUpdateManager::DispatchUpdates(FRHICommandList& RHICmdList, TConstArrayView<FMeshComputeDispatchCommand> DispatchCommands, ERHIPipeline SrcResourceAccessPipelines, ERHIPipeline DstResourceAccessPipelines)
{
	if (DispatchCommands.Num() > 0)
	{
		SCOPED_DRAW_EVENT(RHICmdList, RayTracingDynamicGeometryUpdate);

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SortDispatchCommands);

			// This can be optimized by using sorted insert or using map on shaders
			// There are only a handful of unique shaders and a few target buffers so we want to swap state as little as possible
			// to reduce RHI thread overhead
			DispatchCommands.Sort([](const FMeshComputeDispatchCommand& InLHS, const FMeshComputeDispatchCommand& InRHS)
			                      {
									  if (InLHS.MaterialShader.GetComputeShader() != InRHS.MaterialShader.GetComputeShader())
										  return InLHS.MaterialShader.GetComputeShader() < InRHS.MaterialShader.GetComputeShader();

									  return InLHS.TargetBuffer < InRHS.TargetBuffer;
								  });
		}

		FMemMark Mark(FMemStack::Get());

		TArray<FRHITransitionInfo, TMemStackAllocator<>> TransitionsBefore, TransitionsAfter;
		TArray<FRHIUnorderedAccessView*, TMemStackAllocator<>> OverlapUAVs;
		TransitionsBefore.Reserve(DispatchCommands.Num());
		TransitionsAfter.Reserve(DispatchCommands.Num());
		OverlapUAVs.Reserve(DispatchCommands.Num());
		const FRWBuffer* LastBuffer = nullptr;
		TSet<const FRWBuffer*> TransitionedBuffers;
		for (const FMeshComputeDispatchCommand& Cmd : DispatchCommands)
		{
			if (Cmd.TargetBuffer == nullptr)
			{
				continue;
			}
			FRHIUnorderedAccessView* UAV = Cmd.TargetBuffer->UAV.GetReference();

			// The list is sorted by TargetBuffer, so we can remove duplicates by simply looking at the previous value we've processed.
			if (LastBuffer == Cmd.TargetBuffer)
			{
				// This UAV is used by more than one dispatch, so tell the RHI it's OK to overlap the dispatches, because
				// we're updating disjoint regions.
				if (OverlapUAVs.Num() == 0 || OverlapUAVs.Last() != UAV)
				{
					OverlapUAVs.Add(UAV);
				}
				continue;
			}

			LastBuffer = Cmd.TargetBuffer;

			// In case different shaders use different TargetBuffer we want to add transition only once
			bool bAlreadyInSet = false;
			TransitionedBuffers.FindOrAdd(LastBuffer, &bAlreadyInSet);
			if (!bAlreadyInSet)
			{
				// Looks like the resource can get here in either UAVCompute or SRVMask mode, so we'll have to use Unknown until we can have better tracking.
				TransitionsBefore.Add(FRHITransitionInfo(UAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
				TransitionsAfter.Add(FRHITransitionInfo(UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
			}
		}

		{
			FRHIComputeShader* CurrentShader = nullptr;
			FRWBuffer* CurrentBuffer = nullptr;

			// Transition to writeable for each cmd list and enable UAV overlap, because several dispatches can update non-overlapping portions of the same buffer.
			// Mark as no fence because these resources have been fenced already at the beginning of the frame by RDG
			RHICmdList.Transition(TransitionsBefore, ERHITransitionCreateFlags::AllowDecayPipelines);
			RHICmdList.BeginUAVOverlap(OverlapUAVs);

			// Cache the bound uniform buffers because a lot are the same between dispatches
			FShaderBindingState ShaderBindingState;

			for (const FMeshComputeDispatchCommand& Cmd : DispatchCommands)
			{
				const TShaderRef<FRayTracingDynamicGeometryConverterCS>& Shader = Cmd.MaterialShader;
				FRHIComputeShader* ComputeShader = Shader.GetComputeShader();
				if (CurrentShader != ComputeShader)
				{
					SetComputePipelineState(RHICmdList, ComputeShader);
					CurrentBuffer = nullptr;
					CurrentShader = ComputeShader;

					// Reset binding state
					ShaderBindingState = FShaderBindingState();
				}

				FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

				FRWBuffer* TargetBuffer = Cmd.TargetBuffer;

				// Always rebind the target buffer because we have to make sure that the bindless index is always written
				// otherwise it might miss in the cbuffer data
				//if (CurrentBuffer != TargetBuffer)
				{
					CurrentBuffer = TargetBuffer;

					SetUAVParameter(BatchedParameters, Shader->RWVertexPositions, Cmd.TargetBuffer->UAV);
				}

				Cmd.ShaderBindings.SetParameters(BatchedParameters, &ShaderBindingState);
				RHICmdList.SetBatchedShaderParameters(CurrentShader, BatchedParameters);

				const FIntVector NumWrappedThreadGroups = FComputeShaderUtils::GetGroupCountWrapped(Cmd.NumThreads, 64);
				RHICmdList.DispatchComputeShader(NumWrappedThreadGroups.X, NumWrappedThreadGroups.Y, NumWrappedThreadGroups.Z);
			}

			// Make sure buffers are readable again and disable UAV overlap.
			RHICmdList.EndUAVOverlap(OverlapUAVs);

			// Transition to SRV state and mark readable on requested pipelines
			RHICmdList.Transition(TransitionsAfter, SrcResourceAccessPipelines, DstResourceAccessPipelines);
		}
	}
}

void FRayTracingDynamicGeometryUpdateManager::DispatchUpdates(FRHICommandList& RHICmdList, FRHIBuffer* ScratchBuffer, ERHIPipeline SrcResourceAccessPipelines, ERHIPipeline DstResourceAccessPipelines)
{
	for (TPair<FRHIUniformBuffer*, TArray<FMeshComputeDispatchCommand>>& ViewDispatchCommands : DispatchCommandsPerView)
	{
		DispatchUpdates(RHICmdList, ViewDispatchCommands.Value, SrcResourceAccessPipelines, DstResourceAccessPipelines);
	}
			
	if (BuildParams.Num() > 0)
	{
		// Can't use parallel command list because we have to make sure we are not building BVH data
		// on the same RTGeometry on multiple threads at the same time. Ideally move the build
		// requests over to the RaytracingGeometry manager so they can be correctly scheduled
		// with other build requests in the engine (see UE-106982)
		SCOPED_DRAW_EVENT(RHICmdList, Build);

		FRHIBufferRange ScratchBufferRange;
		ScratchBufferRange.Buffer = ScratchBuffer;
		ScratchBufferRange.Offset = 0;
		RHICmdList.BuildAccelerationStructures(BuildParams, ScratchBufferRange);
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	EndUpdate();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FRayTracingDynamicGeometryUpdateManager::EndUpdate()
{
	ReferencedUniformBuffers.Empty(ReferencedUniformBuffers.Max());

	Clear();
}

uint32 FRayTracingDynamicGeometryUpdateManager::ComputeScratchBufferSize()
{	
	return ScratchBufferSize;
}

#undef USE_RAY_TRACING_DYNAMIC_GEOMETRY_PARALLEL_COMMAND_LISTS

#endif // RHI_RAYTRACING
