// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingScene.h"

#if RHI_RAYTRACING

#include "RayTracingInstanceBufferUtil.h"
#include "RenderCore.h"
#include "RayTracingDefinitions.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RaytracingOptions.h"
#include "PrimitiveSceneProxy.h"
#include "SceneUniformBuffer.h"
#include "SceneRendering.h"
#include "RayTracingInstanceCulling.h"
#include "Rendering/RayTracingGeometryManager.h"

static TAutoConsoleVariable<int32> CVarRayTracingSceneBuildMode(
	TEXT("r.RayTracing.Scene.BuildMode"),
	1,
	TEXT("Controls the mode in which ray tracing scene is built:\n")
	TEXT(" 0: Fast build\n")
	TEXT(" 1: Fast trace (default)\n"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<bool> CVarRayTracingSceneUseTracingFeedback(
	TEXT("r.RayTracing.Scene.UseTracingFeedback"),
	false,
	TEXT("When set to true, will only schedule updates of dynamic geometry instances that were hit in the previous frame."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarRayTracingSceneBatchedBuild(
	TEXT("r.RayTracing.Scene.BatchedBuild"),
	true,
	TEXT("Whether to batch TLAS builds. Should be kept enabled since batched builds reduce barriers on GPU."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarRayTracingSceneCompactInstances(
	TEXT("r.RayTracing.Scene.CompactInstances"),
	false,
	TEXT("Whether to compact the instance buffer so it only contains active instances.\n")
	TEXT("On platforms that don't support indirect TLAS build this requires doing a GPU->CPU readback, ")
	TEXT("which lead so instances missing from TLAS due to the extra latency.\n")
	TEXT("r.RayTracing.Scene.CompactInstances.Min and r.RayTracing.Scene.CompactInstances.Margin can be used to avoid those issues."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarRayTracingSceneCompactInstancesMin(
	TEXT("r.RayTracing.Scene.CompactInstances.Min"),
	0,
	TEXT("Minimum of instances in the instance buffer when using compaction.\n")
	TEXT("Should be set to the expected high water mark to avoid issues on platforms that don't support indirect TLAS build."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingSceneCompactInstancesMargin(
	TEXT("r.RayTracing.Scene.CompactInstances.Margin"),
	5000,
	TEXT("Margin applied on top of lastest number of active instances readback from GPU to avoid issues when number instances increases from frame to frame."),
	ECVF_RenderThreadSafe
);

#if !UE_BUILD_SHIPPING

static bool GRayTracingSerializeSceneNextFrame = false;

static FAutoConsoleCommand RayTracingSerializeSceneCmd(
	TEXT("r.RayTracing.Scene.SerializeOnce"),
	TEXT("Serialize Ray Tracing Scene to disk."),
	FConsoleCommandDelegate::CreateStatic([] { GRayTracingSerializeSceneNextFrame = true; }));

#endif

bool IsRayTracingFeedbackEnabled(const FSceneViewFamily& ViewFamily)
{
	// TODO: For now Feedback is limited to inline passes
	return !HasRayTracedOverlay(ViewFamily) && CVarRayTracingSceneUseTracingFeedback.GetValueOnRenderThread() && GRHISupportsInlineRayTracing;
}

BEGIN_SHADER_PARAMETER_STRUCT(FBuildInstanceBufferPassParams, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, InstanceBuffer)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutputStats)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, InstanceExtraDataBuffer)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
END_SHADER_PARAMETER_STRUCT()

const FRayTracingScene::FInstanceHandle FRayTracingScene::INVALID_INSTANCE_HANDLE = FInstanceHandle();

using FInstanceBufferStats = uint32;

FRayTracingScene::FRayTracingScene()
{
	const uint8 NumLayers = uint8(ERayTracingSceneLayer::NUM);
	Layers.AddDefaulted(NumLayers);

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayer& Layer = Layers[LayerIndex];
		Layer.Name = FName(FString::Printf(TEXT("RayTracingScene_Layer%u"), LayerIndex));
	}
}

FRayTracingScene::~FRayTracingScene()
{
	ReleaseFeedbackReadbackBuffers();
	ReleaseReadbackBuffers();
}

void FRayTracingScene::BuildInitializationData()
{
	const uint8 NumLayers = uint8(ERayTracingSceneLayer::NUM);

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayer& Layer = Layers[LayerIndex];

		Layer.InstanceBufferBuilder.Init(Layer.Instances, PreViewTranslation);
	}

	bInitializationDataBuilt = true;
}

void FRayTracingScene::InitPreViewTranslation(const FViewMatrices& ViewMatrices)
{
	PreViewTranslation = ViewMatrices.GetPreViewTranslation();
}

void FRayTracingScene::Create(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FGPUScene* GPUScene, ERDGPassFlags ComputePassFlags)
{
	// Round up buffer sizes to some multiple to avoid pathological growth reallocations.
	static constexpr uint32 AllocationGranularity = 8 * 1024;
	static constexpr uint64 BufferAllocationGranularity = 16 * 1024 * 1024;

	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingScene::Create);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RayTracingScene_Create);

	const ERayTracingAccelerationStructureFlags BuildFlags = CVarRayTracingSceneBuildMode.GetValueOnRenderThread()
		? ERayTracingAccelerationStructureFlags::FastTrace
		: ERayTracingAccelerationStructureFlags::FastBuild;

	if (!bInitializationDataBuilt)
	{
		BuildInitializationData();
	}

	bUsedThisFrame = true;

	FRHICommandListBase& RHICmdList = GraphBuilder.RHICmdList;

	const uint8 NumLayers = uint8(ERayTracingSceneLayer::NUM);

	FRDGBufferUAVRef InstanceStatsBufferUAV = nullptr;
	{
		// one counter per layer in the stats buffer
		FRDGBufferDesc InstanceStatsBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FInstanceBufferStats), NumLayers);
		InstanceStatsBufferDesc.Usage |= BUF_SourceCopy;

		InstanceStatsBuffer = GraphBuilder.CreateBuffer(InstanceStatsBufferDesc, TEXT("FRayTracingScene::InstanceStatsBuffer"));
		InstanceStatsBufferUAV = GraphBuilder.CreateUAV(InstanceStatsBuffer);

		AddClearUAVPass(GraphBuilder, InstanceStatsBufferUAV, 0, ComputePassFlags);
	}

	const bool bCompactInstanceBuffer = CVarRayTracingSceneCompactInstances.GetValueOnRenderThread();

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayer& Layer = Layers[LayerIndex];

		uint32 NumNativeInstances = Layer.InstanceBufferBuilder.GetMaxNumInstances();

		if (bCompactInstanceBuffer)
		{
			NumNativeInstances = FMath::Max<uint32>(CVarRayTracingSceneCompactInstancesMin.GetValueOnRenderThread(), Layer.NumActiveInstances + CVarRayTracingSceneCompactInstancesMargin.GetValueOnRenderThread());
			NumNativeInstances = FMath::Min<uint32>(NumNativeInstances, Layer.InstanceBufferBuilder.GetMaxNumInstances());
		}

		Layer.MaxNumInstances = NumNativeInstances;

		const uint32 NumNativeInstancesAligned = FMath::DivideAndRoundUp(FMath::Max(NumNativeInstances, 1U), AllocationGranularity) * AllocationGranularity;

		{
			FRayTracingSceneInitializer Initializer;
			Initializer.DebugName = Layer.Name;
			Initializer.MaxNumInstances = NumNativeInstances;
			Initializer.BuildFlags = BuildFlags;
			Layer.RayTracingSceneRHI = RHICreateRayTracingScene(MoveTemp(Initializer));
		}

		FRayTracingAccelerationStructureSize SizeInfo = Layer.RayTracingSceneRHI->GetSizeInfo();
		SizeInfo.ResultSize = FMath::DivideAndRoundUp(FMath::Max(SizeInfo.ResultSize, 1ull), BufferAllocationGranularity) * BufferAllocationGranularity;

		// Allocate GPU buffer if current one is too small or significantly larger than what we need.
		if (!Layer.RayTracingScenePooledBuffer.IsValid()
			|| SizeInfo.ResultSize > Layer.RayTracingScenePooledBuffer->GetSize()
			|| SizeInfo.ResultSize < Layer.RayTracingScenePooledBuffer->GetSize() / 2)
		{
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateBufferDesc(1, uint32(SizeInfo.ResultSize));
			Desc.Usage = EBufferUsageFlags::AccelerationStructure;

			Layer.RayTracingScenePooledBuffer = AllocatePooledBuffer(Desc, TEXT("FRayTracingScene::SceneBuffer"));
		}

		Layer.RayTracingSceneBufferRDG = GraphBuilder.RegisterExternalBuffer(Layer.RayTracingScenePooledBuffer);
		Layer.RayTracingSceneBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Layer.RayTracingSceneBufferRDG, Layer.RayTracingSceneRHI, 0));

		{
			const uint64 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
			FRDGBufferDesc ScratchBufferDesc;
			ScratchBufferDesc.Usage = EBufferUsageFlags::RayTracingScratch | EBufferUsageFlags::StructuredBuffer;
			ScratchBufferDesc.BytesPerElement = uint32(ScratchAlignment);
			ScratchBufferDesc.NumElements = uint32(FMath::DivideAndRoundUp(SizeInfo.BuildScratchSize, ScratchAlignment));

			Layer.BuildScratchBuffer = GraphBuilder.CreateBuffer(ScratchBufferDesc, TEXT("FRayTracingScene::ScratchBuffer"));
		}

		{
			FRDGBufferDesc InstanceBufferDesc;
			InstanceBufferDesc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
			InstanceBufferDesc.BytesPerElement = GRHIRayTracingInstanceDescriptorSize;
			InstanceBufferDesc.NumElements = NumNativeInstancesAligned;

			Layer.InstanceBuffer = GraphBuilder.CreateBuffer(InstanceBufferDesc, TEXT("FRayTracingScene::InstanceBuffer"));

			if (bCompactInstanceBuffer)
			{
				// need to clear since FRayTracingBuildInstanceBufferCS will only write active instances
				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Layer.InstanceBuffer), 0, ComputePassFlags);
			}
		}

		// Feedback
		if (bTracingFeedbackEnabled)
		{
			{
				FRDGBufferDesc InstanceHitCountBufferDesc;
				InstanceHitCountBufferDesc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
				InstanceHitCountBufferDesc.BytesPerElement = sizeof(uint32);
				InstanceHitCountBufferDesc.NumElements = NumNativeInstancesAligned;

				Layer.InstanceHitCountBuffer = GraphBuilder.CreateBuffer(InstanceHitCountBufferDesc, TEXT("FRayTracingScene::InstanceHitCount"));
				Layer.InstanceHitCountBufferUAV = GraphBuilder.CreateUAV(Layer.InstanceHitCountBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
				AddClearUAVPass(GraphBuilder, Layer.InstanceHitCountBufferUAV, 0, ComputePassFlags);
			}

			{
				FRDGBufferDesc AccelerationStructureIndexBufferDesc;
				AccelerationStructureIndexBufferDesc.Usage = EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
				AccelerationStructureIndexBufferDesc.BytesPerElement = sizeof(uint32);
				AccelerationStructureIndexBufferDesc.NumElements = NumNativeInstancesAligned;

				Layer.AccelerationStructureIndexBuffer = GraphBuilder.CreateBuffer(AccelerationStructureIndexBufferDesc, TEXT("FRayTracingScene::AccelerationStructureIndexBuffer"));

				TConstArrayView<uint32> InstanceGeometryIndices = Layer.InstanceBufferBuilder.GetInstanceGeometryIndices();
				GraphBuilder.QueueBufferUpload(Layer.AccelerationStructureIndexBuffer, InstanceGeometryIndices.GetData(), InstanceGeometryIndices.GetTypeSize() * InstanceGeometryIndices.Num());
			}

			{
				FRDGBufferDesc GeometryHandleBufferDesc;
				GeometryHandleBufferDesc.Usage = EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
				GeometryHandleBufferDesc.BytesPerElement = sizeof(int32);
				GeometryHandleBufferDesc.NumElements = FMath::Max(Layer.GeometryHandles.Num(), 1);

				Layer.GeometryHandleBuffer = GraphBuilder.CreateBuffer(GeometryHandleBufferDesc, TEXT("FRayTracingScene::GeometryHandleBuffer"));
				GraphBuilder.QueueBufferUpload(Layer.GeometryHandleBuffer, Layer.GeometryHandles.GetData(), Layer.GeometryHandles.GetTypeSize() * Layer.GeometryHandles.Num());
			}
		}

		FRDGBufferUAVRef InstanceExtraDataBufferUAV = nullptr;
		if (bNeedsInstanceExtraDataBuffer || bTracingFeedbackEnabled || bInstanceDebugDataEnabled)
		{
			FRDGBufferDesc InstanceExtraDataBufferDesc;
			InstanceExtraDataBufferDesc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
			InstanceExtraDataBufferDesc.BytesPerElement = sizeof(FRayTracingInstanceExtraData);
			InstanceExtraDataBufferDesc.NumElements = FMath::Max(NumNativeInstances, 1u);

			Layer.InstanceExtraDataBuffer = GraphBuilder.CreateBuffer(InstanceExtraDataBufferDesc, TEXT("FRayTracingScene::InstanceExtraDataBuffer"));
			InstanceExtraDataBufferUAV = GraphBuilder.CreateUAV(Layer.InstanceExtraDataBuffer);

			AddClearUAVPass(GraphBuilder, InstanceExtraDataBufferUAV, 0xFFFFFFFF, ComputePassFlags);
		}

		if (Layer.InstancesDebugData.Num() > 0)
		{
			check(Layer.InstancesDebugData.Num() == Layer.Instances.Num());

			Layer.InstanceDebugBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("FRayTracingScene::InstanceDebugData"), Layer.InstancesDebugData);
		}

		if (NumNativeInstances > 0)
		{
			// Fill instance upload buffer on separate thread since results are only needed in RHI thread
			GraphBuilder.AddCommandListSetupTask([&InstanceBufferBuilder = Layer.InstanceBufferBuilder](FRHICommandList& RHICmdList)
				{
					FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

					InstanceBufferBuilder.FillRayTracingInstanceUploadBuffer(RHICmdList);
				});

			GraphBuilder.AddCommandListSetupTask([&InstanceBufferBuilder = Layer.InstanceBufferBuilder](FRHICommandList& RHICmdList)
				{
					FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

					InstanceBufferBuilder.FillAccelerationStructureAddressesBuffer(RHICmdList);
				});

#if STATS
			const bool bStatsEnabled = true;
#else
			const bool bStatsEnabled = false;
#endif

			FBuildInstanceBufferPassParams* PassParams = GraphBuilder.AllocParameters<FBuildInstanceBufferPassParams>();
			PassParams->InstanceBuffer = GraphBuilder.CreateUAV(Layer.InstanceBuffer);
			PassParams->InstanceExtraDataBuffer = InstanceExtraDataBufferUAV;
			PassParams->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
			PassParams->OutputStats = bCompactInstanceBuffer || bStatsEnabled ? InstanceStatsBufferUAV : nullptr;

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RayTracingBuildInstanceBuffer"),
				PassParams,
				ComputePassFlags,
				[PassParams,
				&InstanceBufferBuilder = Layer.InstanceBufferBuilder,
				LayerIndex,
				GPUScene,
				CullingParameters = &View.RayTracingCullingParameters,
				NumNativeInstances,
				bCompactInstanceBuffer
				](FRHICommandList& RHICmdList)
				{
					InstanceBufferBuilder.BuildRayTracingInstanceBuffer(
						RHICmdList,
						GPUScene,
						CullingParameters,
						PassParams->InstanceBuffer->GetRHI(),
						NumNativeInstances,
						bCompactInstanceBuffer,
						PassParams->OutputStats ? PassParams->OutputStats->GetRHI() : nullptr,
						LayerIndex,
						PassParams->InstanceExtraDataBuffer ? PassParams->InstanceExtraDataBuffer->GetRHI() : nullptr);
				});
		}
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FRayTracingSceneBuildPassParams, )
	RDG_BUFFER_ACCESS_ARRAY(TLASBuildBuffers)
	RDG_BUFFER_ACCESS(DynamicGeometryScratchBuffer, ERHIAccess::UAVCompute)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FRayTracingSceneSerializePassParams, )
	RDG_BUFFER_ACCESS(TLASBuffer, ERHIAccess::BVHRead)
END_SHADER_PARAMETER_STRUCT()

void FRayTracingScene::Build(FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags, FRDGBufferRef DynamicGeometryScratchBuffer)
{
	const uint8 NumLayers = uint8(ERayTracingSceneLayer::NUM);

	FRayTracingSceneBuildPassParams* PassParams = GraphBuilder.AllocParameters<FRayTracingSceneBuildPassParams>();
	PassParams->DynamicGeometryScratchBuffer = DynamicGeometryScratchBuffer; // TODO: Is this necessary?

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayer& Layer = Layers[LayerIndex];

		PassParams->TLASBuildBuffers.Emplace(Layer.BuildScratchBuffer, ERHIAccess::UAVCompute);
		PassParams->TLASBuildBuffers.Emplace(Layer.InstanceBuffer, ERHIAccess::SRVCompute);
		PassParams->TLASBuildBuffers.Emplace(Layer.RayTracingSceneBufferRDG, ERHIAccess::BVHWrite);
	}

	GraphBuilder.AddPass(RDG_EVENT_NAME("RayTracingBuildScene"), PassParams, ComputePassFlags,
		[PassParams, this](FRHICommandList& RHICmdList)
		{
			const bool bUseBatchedBuild = CVarRayTracingSceneBatchedBuild.GetValueOnRenderThread();

			TArray<FRayTracingSceneBuildParams> BatchedBuildParams;
			BatchedBuildParams.Reserve(NumLayers);

			for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
			{
				FLayer& Layer = Layers[LayerIndex];

				FRayTracingSceneBuildParams BuildParams;
				BuildParams.Scene = Layer.RayTracingSceneRHI;
				BuildParams.ScratchBuffer = Layer.BuildScratchBuffer->GetRHI();
				BuildParams.ScratchBufferOffset = 0;
				BuildParams.InstanceBuffer = Layer.InstanceBuffer->GetRHI();
				BuildParams.InstanceBufferOffset = 0;
				BuildParams.NumInstances = Layer.MaxNumInstances;
				BuildParams.ReferencedGeometries = Layer.InstanceBufferBuilder.GetReferencedGeometries();

				RHICmdList.BindAccelerationStructureMemory(Layer.RayTracingSceneRHI, Layer.RayTracingSceneBufferRDG->GetRHI(), 0);

				if (bUseBatchedBuild)
				{
					BatchedBuildParams.Add(BuildParams);
				}
				else
				{
					RHICmdList.BuildAccelerationStructure(BuildParams);
				}
			}

			if (bUseBatchedBuild)
			{
				RHICmdList.BuildAccelerationStructures(BatchedBuildParams);
			}
		});

#if !UE_BUILD_SHIPPING
	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayer& Layer = Layers[LayerIndex];

		if (GRayTracingSerializeSceneNextFrame && GRHIGlobals.RayTracing.SupportsSerializeAccelerationStructure)
		{
			FRayTracingSceneSerializePassParams* SerializePassParams = GraphBuilder.AllocParameters<FRayTracingSceneSerializePassParams>();
			SerializePassParams->TLASBuffer = Layer.RayTracingSceneBufferRDG;

			GraphBuilder.AddPass(RDG_EVENT_NAME("RayTracingSerializeScene"), SerializePassParams, ERDGPassFlags::Readback,
				[SerializePassParams, &Layer](FRHICommandListImmediate& RHICmdList)
				{
					FString Filename = FString::Printf(TEXT("%s_(%s)"), *Layer.Name.ToString(), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
					FString RootPath = FPaths::ScreenShotDir() + TEXT("BVH/"); // Save BVH dumps to ScreenShot directory
					FString OutputFilename = RootPath + Filename + TEXT(".bvh");

					RHICmdList.SerializeAccelerationStructure(Layer.RayTracingSceneRHI, *OutputFilename);
				});
		}
	}

	GRayTracingSerializeSceneNextFrame = false;
#endif // !UE_BUILD_SHIPPING
}

struct FRayTracingProcessFeedbackCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingProcessFeedbackCS);
	SHADER_USE_PARAMETER_STRUCT(FRayTracingProcessFeedbackCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )		
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, GeometryHitCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, RWGeometryHandleBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWGeometryHandleAllocator)
		SHADER_PARAMETER(uint32, NumGeometries)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr uint32 ThreadGroupSize = 64;

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);

		// Force DXC to avoid shader reflection issues.
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsRayTracingEnabledForProject(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingProcessFeedbackCS, "/Engine/Private/Raytracing/RayTracingFeedback.usf", "RayTracingProcessFeedbackCS", SF_Compute);

struct FRayTracingUpdateGeometryHitCountCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingUpdateGeometryHitCountCS);
	SHADER_USE_PARAMETER_STRUCT(FRayTracingUpdateGeometryHitCountCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, GeometryHandleBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, InstanceHitCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, RWGeometryHitCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, RWGeometryHandleBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, AccelerationStructureIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, InstanceExtraDataBuffer)

		SHADER_PARAMETER(uint32, NumInstances)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr uint32 ThreadGroupSize = 64;

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);

		// Force DXC to avoid shader reflection issues.
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsRayTracingEnabledForProject(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingUpdateGeometryHitCountCS, "/Engine/Private/Raytracing/RayTracingFeedback.usf", "RayTracingUpdateGeometryHitCountCS", SF_Compute);

BEGIN_SHADER_PARAMETER_STRUCT(FFeedbackReadbackPassParameters, )
	RDG_BUFFER_ACCESS(HandleBuffer, ERHIAccess::CopySrc)
	RDG_BUFFER_ACCESS(CountBuffer, ERHIAccess::CopySrc)	
END_SHADER_PARAMETER_STRUCT()

void FRayTracingScene::FinishTracingFeedback(FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags)
{
	RDG_EVENT_SCOPE(GraphBuilder, "RayTracingScene::FinishTracingFeedback");
	
	const FLayer& Layer = Layers[0];
	const uint32 NumGeometries = (uint32)Layer.InstanceBufferBuilder.GetReferencedGeometries().Num();
	const uint32 NumInstances = Layer.InstanceBufferBuilder.GetMaxNumInstances();

	if (NumGeometries == 0 || !bTracingFeedbackEnabled)
	{
		return;
	}

	FRDGBufferRef GeometryHandleBuffer;

	FRDGBufferDesc GeometryHandleBufferDesc;
	GeometryHandleBufferDesc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer | EBufferUsageFlags::SourceCopy;
	GeometryHandleBufferDesc.BytesPerElement = sizeof(int32);
	GeometryHandleBufferDesc.NumElements = NumGeometries;

	GeometryHandleBuffer = GraphBuilder.CreateBuffer(GeometryHandleBufferDesc, TEXT("FRayTracingScene::GeometryHandleBuffer"));

	// Update geometry hit count
	FRDGBufferRef GeometryHitCountBuffer;
	{
		FRDGBufferDesc GeometryHitCountBufferDesc;
		GeometryHitCountBufferDesc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
		GeometryHitCountBufferDesc.BytesPerElement = sizeof(uint32);
		GeometryHitCountBufferDesc.NumElements = NumGeometries;

		GeometryHitCountBuffer = GraphBuilder.CreateBuffer(GeometryHitCountBufferDesc, TEXT("FRayTracingScene::GeometryHitCountBuffer"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(GeometryHitCountBuffer), 0, ComputePassFlags);

		FRayTracingUpdateGeometryHitCountCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingUpdateGeometryHitCountCS::FParameters>();

		PassParameters->GeometryHandleBuffer = GraphBuilder.CreateSRV(Layer.GeometryHandleBuffer);
		PassParameters->AccelerationStructureIndexBuffer = GraphBuilder.CreateSRV(Layer.AccelerationStructureIndexBuffer);
		PassParameters->InstanceHitCountBuffer = GraphBuilder.CreateSRV(Layer.InstanceHitCountBuffer);
		PassParameters->RWGeometryHitCountBuffer = GraphBuilder.CreateUAV(GeometryHitCountBuffer);
		PassParameters->RWGeometryHandleBuffer = GraphBuilder.CreateUAV(GeometryHandleBuffer);
		PassParameters->InstanceExtraDataBuffer = GraphBuilder.CreateSRV(Layer.InstanceExtraDataBuffer);
		PassParameters->NumInstances = NumInstances;

		const FIntVector GroupSize = FComputeShaderUtils::GetGroupCountWrapped(NumInstances, FRayTracingUpdateGeometryHitCountCS::ThreadGroupSize);

		TShaderRef<FRayTracingUpdateGeometryHitCountCS> ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FRayTracingUpdateGeometryHitCountCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FRayTracingScene::RayTracingUpdateGeometryHitCount"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			GroupSize);
	}	

	// Fill geometry handle buffer	
	FRDGBufferRef GeometryHandleAllocatorBuffer;
	{
		FRDGBufferDesc GeometryHandleAllocatorBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1);
		GeometryHandleAllocatorBufferDesc.Usage = EBufferUsageFlags(GeometryHandleAllocatorBufferDesc.Usage | BUF_SourceCopy);
		GeometryHandleAllocatorBuffer = GraphBuilder.CreateBuffer(GeometryHandleAllocatorBufferDesc, TEXT("FRayTracingScene::GeometryHandleAllocator"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(GeometryHandleAllocatorBuffer, PF_R32_UINT), 0, ComputePassFlags);

		FRayTracingProcessFeedbackCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingProcessFeedbackCS::FParameters>();
		PassParameters->GeometryHitCountBuffer = GraphBuilder.CreateSRV(GeometryHitCountBuffer);
		PassParameters->RWGeometryHandleBuffer = GraphBuilder.CreateUAV(GeometryHandleBuffer);
		PassParameters->RWGeometryHandleAllocator = GraphBuilder.CreateUAV(GeometryHandleAllocatorBuffer, PF_R32_UINT);
		PassParameters->NumGeometries = NumGeometries;

		const FIntVector GroupSize = FComputeShaderUtils::GetGroupCountWrapped(NumGeometries, FRayTracingProcessFeedbackCS::ThreadGroupSize);

		TShaderRef<FRayTracingProcessFeedbackCS> ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FRayTracingProcessFeedbackCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FRayTracingScene::FinishTracingFeedback"),			
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			GroupSize);
	}	

	// Readback
	//  if necessary create readback buffers
	if (FeedbackReadback.IsEmpty())
	{
		FeedbackReadback.SetNum(MaxReadbackBuffers);

		for (uint32 Index = 0; Index < MaxReadbackBuffers; ++Index)
		{
			FeedbackReadback[Index].GeometryHandleReadbackBuffer = new FRHIGPUBufferReadback(TEXT("FRayTracingScene::FeedbackReadbackBuffer::GeometryHandles"));
			FeedbackReadback[Index].GeometryCountReadbackBuffer = new FRHIGPUBufferReadback(TEXT("FRayTracingScene::FeedbackReadbackBuffer::GeometryCount"));			
		}
	}

	// copy feedback to readback buffer
	{
		FFeedbackReadbackPassParameters* PassParameters = GraphBuilder.AllocParameters<FFeedbackReadbackPassParameters>();
		PassParameters->HandleBuffer = GeometryHandleBuffer;
		PassParameters->CountBuffer = GeometryHandleAllocatorBuffer;

		GraphBuilder.AddPass(RDG_EVENT_NAME("FRayTracingScene::FeedbackReadback"), PassParameters, ERDGPassFlags::Readback,
			[HandleReadbackBuffer = FeedbackReadback[FeedbackReadbackWriteIndex].GeometryHandleReadbackBuffer,
			CountReadbackBuffer = FeedbackReadback[FeedbackReadbackWriteIndex].GeometryCountReadbackBuffer,
			PassParameters]
			(FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				HandleReadbackBuffer->EnqueueCopy(RHICmdList, PassParameters->HandleBuffer->GetRHI(), 0u);
				CountReadbackBuffer->EnqueueCopy(RHICmdList, PassParameters->CountBuffer->GetRHI(), 0u);
			});

		FeedbackReadbackWriteIndex = (FeedbackReadbackWriteIndex + 1u) % MaxReadbackBuffers;
		FeedbackReadbackNumPending = FMath::Min(FeedbackReadbackNumPending + 1u, MaxReadbackBuffers);

		while (FeedbackReadbackNumPending > 0)
		{
			uint32 Index = (FeedbackReadbackWriteIndex + MaxReadbackBuffers - FeedbackReadbackNumPending) % MaxReadbackBuffers;
			FRHIGPUBufferReadback* GeometryHandleReadbackBuffer = FeedbackReadback[Index].GeometryHandleReadbackBuffer;
			FRHIGPUBufferReadback* GeometryCountReadbackBuffer = FeedbackReadback[Index].GeometryCountReadbackBuffer;
			check(GeometryHandleReadbackBuffer->IsReady() == GeometryCountReadbackBuffer->IsReady());
			if (GeometryHandleReadbackBuffer->IsReady() && GeometryCountReadbackBuffer->IsReady())
			{
				FeedbackReadbackNumPending--;
				
				const uint32* GeometryCountPtr = (const uint32*)GeometryCountReadbackBuffer->Lock(sizeof(uint32));
				const uint32 GeometryCount = GeometryCountPtr[0];
				GeometryCountReadbackBuffer->Unlock();

				const int32* GeometryHandlesPtr = (const int32*)GeometryHandleReadbackBuffer->Lock(sizeof(int32) * GeometryCount);

				for (uint32 i = 0; i < GeometryCount; i++)
				{					
					if (ensure(GeometryHandlesPtr[i] != INDEX_NONE))
					{
						GRayTracingGeometryManager->AddVisibleGeometry(GeometryHandlesPtr[i]);
					}				
				}

				GeometryHandleReadbackBuffer->Unlock();
			}
			else
			{
				break;
			}
		}
	}
}

void FRayTracingScene::FinishStats(FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags)
{
	//  if necessary create readback buffers
	if (StatsReadbackBuffers.IsEmpty())
	{
		StatsReadbackBuffers.SetNum(MaxReadbackBuffers);

		for (uint32 Index = 0; Index < MaxReadbackBuffers; ++Index)
		{
			StatsReadbackBuffers[Index] = new FRHIGPUBufferReadback(TEXT("FRayTracingScene::StatsReadbackBuffer"));
		}
	}

	// copy stats to readback buffer
	if (InstanceStatsBuffer != nullptr)
	{
		AddReadbackBufferPass(GraphBuilder, RDG_EVENT_NAME("FRayTracingScene::StatsReadback"), InstanceStatsBuffer,
			[ReadbackBuffer = StatsReadbackBuffers[StatsReadbackBuffersWriteIndex], InstanceStatsBuffer = InstanceStatsBuffer](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				ReadbackBuffer->EnqueueCopy(RHICmdList, InstanceStatsBuffer->GetRHI(), 0u);
			});

		StatsReadbackBuffersWriteIndex = (StatsReadbackBuffersWriteIndex + 1u) % MaxReadbackBuffers;
		StatsReadbackBuffersNumPending = FMath::Min(StatsReadbackBuffersNumPending + 1u, MaxReadbackBuffers);
	}

	uint32 TotalNumNativeInstances = 0;
	uint32 TotalNumActiveInstances = 0;

	const uint8 NumLayers = uint8(ERayTracingSceneLayer::NUM);

	// process ready results
	while (StatsReadbackBuffersNumPending > 0)
	{
		uint32 Index = (StatsReadbackBuffersWriteIndex + MaxReadbackBuffers - StatsReadbackBuffersNumPending) % MaxReadbackBuffers;
		FRHIGPUBufferReadback* ReadbackBuffer = StatsReadbackBuffers[Index];
		if (ReadbackBuffer->IsReady())
		{
			StatsReadbackBuffersNumPending--;

			auto ReadbackBufferPtr = (const FInstanceBufferStats*)ReadbackBuffer->Lock(sizeof(FInstanceBufferStats) * NumLayers);

			for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
			{
				FLayer& Layer = Layers[LayerIndex];

				const uint32 LayerNumNativeInstances = Layer.InstanceBufferBuilder.GetMaxNumInstances();

				Layer.NumActiveInstances = FMath::Min(ReadbackBufferPtr[LayerIndex], LayerNumNativeInstances);

				TotalNumNativeInstances += LayerNumNativeInstances;
				TotalNumActiveInstances += Layer.NumActiveInstances;
			}

			ReadbackBuffer->Unlock();
		}
		else
		{
			break;
		}
	}

	SET_DWORD_STAT(STAT_RayTracingTotalInstances, TotalNumNativeInstances);
	SET_DWORD_STAT(STAT_RayTracingActiveInstances, TotalNumActiveInstances);
}

void FRayTracingScene::PostRender(FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags)
{
	FinishTracingFeedback(GraphBuilder, ComputePassFlags);
	FinishStats(GraphBuilder, ComputePassFlags);
}

bool FRayTracingScene::IsCreated() const
{
	return bUsedThisFrame;
}

FRHIRayTracingScene* FRayTracingScene::GetRHIRayTracingScene(ERayTracingSceneLayer Layer) const
{
	return Layers[uint8(Layer)].RayTracingSceneRHI.GetReference();
}

FRHIRayTracingScene* FRayTracingScene::GetRHIRayTracingSceneChecked(ERayTracingSceneLayer Layer) const
{
	FRHIRayTracingScene* Result = GetRHIRayTracingScene(Layer);
	checkf(Result, TEXT("Ray tracing scene was not created. Perhaps Create() was not called."));
	return Result;
}

FShaderResourceViewRHIRef FRayTracingScene::CreateLayerViewRHI(FRHICommandListBase& RHICmdList, ERayTracingSceneLayer InLayer) const
{
	const FLayer& Layer = Layers[uint8(InLayer)];
	checkf(Layer.RayTracingScenePooledBuffer, TEXT("Ray tracing scene was not created. Perhaps Create() was not called."));
	return RHICmdList.CreateShaderResourceView(FShaderResourceViewInitializer(Layer.RayTracingScenePooledBuffer->GetRHI(), Layer.RayTracingSceneRHI, 0));
}

FRDGBufferSRVRef FRayTracingScene::GetLayerView(ERayTracingSceneLayer Layer) const
{
	checkf(Layers[uint8(Layer)].RayTracingSceneBufferSRV, TEXT("Ray tracing scene SRV was not created. Perhaps Create() was not called."));
	return Layers[uint8(Layer)].RayTracingSceneBufferSRV;
}

FRDGBufferUAVRef FRayTracingScene::GetInstanceHitCountBufferUAV(ERayTracingSceneLayer Layer) const
{
	return bTracingFeedbackEnabled ? Layers[uint8(Layer)].InstanceHitCountBufferUAV : nullptr;
}

uint32 FRayTracingScene::GetNumNativeInstances(ERayTracingSceneLayer InLayer) const
{
	const FLayer& Layer = Layers[uint8(InLayer)];
	checkf(bInitializationDataBuilt, TEXT("Must call BuildInitializationData() or Create() before using GetNumNativeInstances()."));
	return Layer.InstanceBufferBuilder.GetMaxNumInstances();
}

FRayTracingScene::FInstanceHandle FRayTracingScene::AddInstance(FRayTracingGeometryInstance Instance, ERayTracingSceneLayer InLayer, const FPrimitiveSceneProxy* Proxy, bool bDynamic, int32 GeometryHandle)
{
	FLayer& Layer = Layers[uint8(InLayer)];

	FRHIRayTracingGeometry* GeometryRHI = Instance.GeometryRHI;

	const uint32 InstanceIndex = Layer.Instances.Add(MoveTemp(Instance));

	if (bTracingFeedbackEnabled)
	{
		Layer.GeometryHandles.Add(GeometryHandle);
		check(Layer.Instances.Num() == Layer.GeometryHandles.Num());
	}

	if (bInstanceDebugDataEnabled)
	{
		FRayTracingInstanceDebugData& InstanceDebugData = Layer.InstancesDebugData.AddDefaulted_GetRef();
		InstanceDebugData.Flags = bDynamic ? 1 : 0;
		InstanceDebugData.GeometryAddress = uint64(GeometryRHI);

		if (Proxy)
		{
			InstanceDebugData.ProxyHash = Proxy->GetTypeHash();
		}

		check(Layer.Instances.Num() == Layer.InstancesDebugData.Num());
	}

	return { InLayer, InstanceIndex };
}

FRayTracingScene::FInstanceRange FRayTracingScene::AllocateInstanceRangeUninitialized(uint32 NumInstances, ERayTracingSceneLayer InLayer)
{
	FLayer& Layer = Layers[uint8(InLayer)];

	const uint32 OldNum = Layer.Instances.AddUninitialized(NumInstances);

	if (bTracingFeedbackEnabled)
	{
		Layer.GeometryHandles.AddUninitialized(NumInstances);
		check(Layer.Instances.Num() == Layer.GeometryHandles.Num());
	}

	if (bInstanceDebugDataEnabled)
	{
		Layer.InstancesDebugData.AddUninitialized(NumInstances);

		check(Layer.Instances.Num() == Layer.InstancesDebugData.Num());
	}

	return { InLayer, OldNum, NumInstances };
}

void FRayTracingScene::SetInstance(FInstanceRange InstanceRange, uint32 InstanceIndexInRange, FRayTracingGeometryInstance InInstance, const FPrimitiveSceneProxy* Proxy, bool bDynamic, int32 GeometryHandle)
{
	checkf(InstanceIndexInRange < InstanceRange.Num, TEXT("InstanceIndexInRange (%d) is out of bounds for the range (%d)"), InstanceIndexInRange, InstanceRange.Num);

	FLayer& Layer = Layers[uint8(InstanceRange.Layer)];

	const uint32 InstanceIndex = InstanceRange.StartIndex + InstanceIndexInRange;

	FRHIRayTracingGeometry* GeometryRHI = InInstance.GeometryRHI;

	FRayTracingGeometryInstance* Instance = &Layer.Instances[InstanceIndex];
	new (Instance) FRayTracingGeometryInstance(MoveTemp(InInstance));

	if (bTracingFeedbackEnabled)
	{
		Layer.GeometryHandles[InstanceIndex] = GeometryHandle;
	}

	if (bInstanceDebugDataEnabled)
	{
		FRayTracingInstanceDebugData InstanceDebugData;
		InstanceDebugData.Flags = bDynamic ? 1 : 0;
		InstanceDebugData.GeometryAddress = uint64(GeometryRHI);

		if (Proxy)
		{
			InstanceDebugData.ProxyHash = Proxy->GetTypeHash();
		}

		Layer.InstancesDebugData[InstanceIndex] = InstanceDebugData;

		check(Layer.Instances.Num() == Layer.InstancesDebugData.Num());
	}
}

void FRayTracingScene::Reset(bool bInInstanceDebugDataEnabled)
{
	const uint8 NumLayers = uint8(ERayTracingSceneLayer::NUM);

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayer& Layer = Layers[LayerIndex];

		Layer.Instances.Reset();
		Layer.InstancesDebugData.Reset();

		Layer.RayTracingSceneRHI = nullptr;
		Layer.RayTracingSceneBufferRDG = nullptr;
		Layer.RayTracingSceneBufferSRV = nullptr;

		Layer.InstanceBuffer = nullptr;
		Layer.BuildScratchBuffer = nullptr;
		Layer.InstanceExtraDataBuffer = nullptr;
		Layer.InstanceDebugBuffer = nullptr;

		Layer.InstanceHitCountBuffer = nullptr;
		Layer.AccelerationStructureIndexBuffer = nullptr;
		Layer.GeometryHandleBuffer = nullptr;
		Layer.GeometryHandles.Reset();		
	}

	InstanceStatsBuffer = nullptr;

	GeometriesToBuild.Reset();

	Allocator.Flush();

	bUsesLightingChannels = false;

	bInstanceDebugDataEnabled = bInInstanceDebugDataEnabled;
}

void FRayTracingScene::EndFrame()
{
	Reset(false);

	// Release the resources if ray tracing wasn't used
	if (!bUsedThisFrame)
	{
		const uint8 NumLayers = uint8(ERayTracingSceneLayer::NUM);

		for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
		{
			Layers[LayerIndex] = {};
		}

		GeometriesToBuild.Empty();

		ReleaseFeedbackReadbackBuffers();
		ReleaseReadbackBuffers();
	}

	bUsedThisFrame = false;
	bInitializationDataBuilt = false;
}

void FRayTracingScene::ReleaseReadbackBuffers()
{
	for (auto& ReadbackBuffer : StatsReadbackBuffers)
	{
		delete ReadbackBuffer;
	}

	StatsReadbackBuffers.Empty();

	StatsReadbackBuffersWriteIndex = 0;
	StatsReadbackBuffersNumPending = 0;
}

void FRayTracingScene::ReleaseFeedbackReadbackBuffers()
{
	for (auto& ReadbackBuffer : FeedbackReadback)
	{
		delete ReadbackBuffer.GeometryHandleReadbackBuffer;
		delete ReadbackBuffer.GeometryCountReadbackBuffer;
	}
	FeedbackReadback.Empty();

	FeedbackReadbackWriteIndex = 0;
	FeedbackReadbackNumPending = 0;
}

#endif // RHI_RAYTRACING
