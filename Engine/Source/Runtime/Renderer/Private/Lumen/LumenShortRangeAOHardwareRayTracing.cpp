// Copyright Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "SceneTextureParameters.h"
#include "IndirectLightRendering.h"
#include "LumenRadianceCache.h"
#include "LumenScreenProbeGather.h"
#include "LumenShortRangeAO.h"

#if RHI_RAYTRACING

#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "RayTracing/RayTracingMaterialHitShaders.h"
#include "Nanite/NaniteRayTracing.h"

#endif // RHI_RAYTRACING

static TAutoConsoleVariable<int32> CVarLumenShortRangeAOHardwareRayTracing(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing"),
	0,
	TEXT("0. Screen space tracing for the full resolution Bent Normal (directional occlusion).")
	TEXT("1. Enable hardware ray tracing of the full resolution Bent Normal (directional occlusion). (Default)\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenShortRangeAOHardwareRayTracingNormalBias(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing.NormalBias"),
	.1f,
	TEXT("Bias for HWRT Bent Normal to avoid self intersection"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace Lumen
{
	// If Substrate is enabled with multiple closure evaluation, it requires indirect ray dispatch
	bool UseHardwareRayTracedShortRangeAO(const FSceneViewFamily& ViewFamily)
	{
#if RHI_RAYTRACING
		return IsRayTracingEnabled()
			&& Lumen::UseHardwareRayTracing(ViewFamily)
			&& (CVarLumenShortRangeAOHardwareRayTracing.GetValueOnAnyThread() != 0)
			&& (!Lumen::SupportsMultipleClosureEvaluation(ViewFamily.GetShaderPlatform()) || GRHISupportsRayTracingDispatchIndirect);
#else
		return false;
#endif
	}
}

#if RHI_RAYTRACING

class FLumenShortRangeAOHardwareRayTracing : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenShortRangeAOHardwareRayTracing)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FLumenShortRangeAOHardwareRayTracing, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, RWScreenBentNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float>, RWShortRangeAO)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)
		SHADER_PARAMETER(uint32, ScreenProbeGatherStateFrameIndex)
		SHADER_PARAMETER(FIntPoint, ShortRangeAOViewMin)
		SHADER_PARAMETER(FIntPoint, ShortRangeAOViewSize)
		SHADER_PARAMETER(uint32, NumRays)
		SHADER_PARAMETER(float, NormalBias)
		SHADER_PARAMETER(float, MaxScreenTraceFraction)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRayTracingUniformParameters, NaniteRayTracing)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, HairStrandsVoxel)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FHairStrandsVoxel : SHADER_PERMUTATION_BOOL("USE_HAIRSTRANDS_VOXEL");
	class FOutputBentNormal : SHADER_PERMUTATION_BOOL("OUTPUT_BENT_NORMAL");
	class FDownsampleFactor : SHADER_PERMUTATION_RANGE_INT("DOWNSAMPLE_FACTOR", 1, 2);
	class FOverflowTile : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	using FPermutationDomain = TShaderPermutationDomain<FHairStrandsVoxel, FOutputBentNormal, FDownsampleFactor, FOverflowTile>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (!Substrate::IsSubstrateEnabled() && PermutationVector.Get<FOverflowTile>())
		{
			PermutationVector.Set<FOverflowTile>(false);
		}
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (RemapPermutation(PermutationVector) != PermutationVector)
		{
			return false;
		}

		return ShouldCompileRayTracingShadersForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DYNAMIC_CLOSEST_HIT_SHADER"), 0);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DYNAMIC_ANY_HIT_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DYNAMIC_MISS_SHADER"), 0);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_COHERENT_RAYS"), 1);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::RayTracingMaterial;
	}

	static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Parameters) 
	{
		return RayTracing::GetShaderBindingLayout(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenShortRangeAOHardwareRayTracing, "/Engine/Private/Lumen/LumenShortRangeAOHardwareRayTracing.usf", "LumenShortRangeAOHardwareRayTracing", SF_RayGen);

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingShortRangeAO(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseHardwareRayTracedShortRangeAO(*View.Family))
	{
		const int32 OverflowTileCount = Lumen::SupportsMultipleClosureEvaluation(View.GetShaderPlatform()) ? 2 : 1;
		for (int32 OverflowTile = 0; OverflowTile < OverflowTileCount; OverflowTile++)
		for (int32 HairOcclusion = 0; HairOcclusion < 2; HairOcclusion++)
		{
			FLumenShortRangeAOHardwareRayTracing::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenShortRangeAOHardwareRayTracing::FOutputBentNormal>(LumenShortRangeAO::UseBentNormal());
			PermutationVector.Set<FLumenShortRangeAOHardwareRayTracing::FHairStrandsVoxel>(HairOcclusion == 0);
			PermutationVector.Set<FLumenShortRangeAOHardwareRayTracing::FDownsampleFactor>(LumenShortRangeAO::GetDownsampleFactor());
			PermutationVector.Set<FLumenShortRangeAOHardwareRayTracing::FOverflowTile>(OverflowTile > 0);
			TShaderRef<FLumenShortRangeAOHardwareRayTracing> RayGenerationShader = View.ShaderMap->GetShader<FLumenShortRangeAOHardwareRayTracing>(PermutationVector);
			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

#endif // RHI_RAYTRACING

void RenderHardwareRayTracingShortRangeAO(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextures& SceneTextures,
	const FSceneTextureParameters& SceneTextureParameters,
	const FLumenScreenSpaceBentNormalParameters& BentNormalParameters,
	const FBlueNoise& BlueNoise,
	float MaxScreenTraceFraction,
	const FViewInfo& View,
	FRDGTextureRef ShortRangeAO,
	uint32 NumPixelRays)
#if RHI_RAYTRACING
{
	extern int32 GLumenShortRangeAOHairStrandsVoxelTrace;
	const int32 DownsampleFactor = LumenShortRangeAO::GetDownsampleFactor();
	const bool bNeedTraceHairVoxel = HairStrands::HasViewHairStrandsVoxelData(View) && GLumenShortRangeAOHairStrandsVoxelTrace > 0;
	const FRayTracingScene& RayTracingScene = Scene->RayTracingScene;
	const FRayTracingShaderBindingTable& RayTracingSBT = Scene->RayTracingSBT;

	auto ShortAORTPass = [&](bool bOverflow)
	{
		FLumenShortRangeAOHardwareRayTracing::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenShortRangeAOHardwareRayTracing::FParameters>();
		PassParameters->RWShortRangeAO = GraphBuilder.CreateUAV(ShortRangeAO);
		PassParameters->RWDownsampledSceneDepth = BentNormalParameters.DownsampledSceneDepth ? GraphBuilder.CreateUAV(BentNormalParameters.DownsampledSceneDepth) : nullptr;
		PassParameters->RWDownsampledSceneWorldNormal = BentNormalParameters.DownsampledSceneWorldNormal ? GraphBuilder.CreateUAV(BentNormalParameters.DownsampledSceneWorldNormal) : nullptr;
		PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->SceneTextures = SceneTextureParameters;
		PassParameters->Scene = GetSceneUniformBufferRef(GraphBuilder, View);
		PassParameters->NaniteRayTracing = Nanite::GRayTracingManager.GetUniformBuffer();
		PassParameters->BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);
		PassParameters->ScreenProbeGatherStateFrameIndex = LumenScreenProbeGather::GetStateFrameIndex(View.ViewState);
		PassParameters->ShortRangeAOViewMin = BentNormalParameters.ShortRangeAOViewMin;
		PassParameters->ShortRangeAOViewSize = BentNormalParameters.ShortRangeAOViewSize;
		PassParameters->MaxScreenTraceFraction = MaxScreenTraceFraction;
		PassParameters->NumRays = NumPixelRays;
		PassParameters->NormalBias = CVarLumenShortRangeAOHardwareRayTracingNormalBias.GetValueOnRenderThread();
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

		if (bNeedTraceHairVoxel)
		{
			PassParameters->HairStrandsVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
		}

		if (bOverflow)
		{
			PassParameters->TileIndirectBuffer = View.SubstrateViewData.ClosureTileRaytracingIndirectBuffer;
		}

		FLumenShortRangeAOHardwareRayTracing::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenShortRangeAOHardwareRayTracing::FHairStrandsVoxel>(bNeedTraceHairVoxel);
		PermutationVector.Set<FLumenShortRangeAOHardwareRayTracing::FOutputBentNormal>(LumenShortRangeAO::UseBentNormal());
		PermutationVector.Set<FLumenShortRangeAOHardwareRayTracing::FDownsampleFactor>(DownsampleFactor);
		PermutationVector.Set<FLumenShortRangeAOHardwareRayTracing::FOverflowTile>(bOverflow);
		PermutationVector = FLumenShortRangeAOHardwareRayTracing::RemapPermutation(PermutationVector);
		TShaderMapRef<FLumenShortRangeAOHardwareRayTracing> RayGenerationShader(View.ShaderMap, PermutationVector);

		ClearUnusedGraphResources(RayGenerationShader, PassParameters);

		FIntPoint Resolution(View.ViewRect.Width(), View.ViewRect.Height());
		if (DownsampleFactor > 1)
		{
			Resolution.X = FMath::DivideAndRoundUp(Resolution.X, DownsampleFactor);
			Resolution.Y = FMath::DivideAndRoundUp(Resolution.Y, DownsampleFactor);
		}

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ShortRangeAO_HWRT(Rays=%u, DownsampledFactor:%d, BentNormal:%d)", NumPixelRays, DownsampleFactor, LumenShortRangeAO::UseBentNormal()),
			PassParameters,
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			[&View, RayGenerationShader, PassParameters, Resolution, DownsampleFactor, bOverflow, &RayTracingSBT](FRHICommandList& RHICmdList)
			{
				FRHIBatchedShaderParameters& GlobalResources = RHICmdList.GetScratchShaderParameters();
				SetShaderParameters(GlobalResources, RayGenerationShader, *PassParameters);

				FRHIUniformBuffer* SceneUniformBuffer = PassParameters->Scene->GetRHI();
				FRHIUniformBuffer* NaniteRayTracingUniformBuffer = PassParameters->NaniteRayTracing->GetRHI();
				TOptional<FScopedUniformBufferStaticBindings> StaticUniformBufferScope = RayTracing::BindStaticUniformBufferBindings(View, SceneUniformBuffer, NaniteRayTracingUniformBuffer, RHICmdList);

				bool bBentNormalEnableMaterials = false;

				if (bBentNormalEnableMaterials)
				{
					if (bOverflow)
					{
						RHICmdList.RayTraceDispatchIndirect(View.MaterialRayTracingData.PipelineState, RayGenerationShader.GetRayTracingShader(), View.MaterialRayTracingData.ShaderBindingTable, GlobalResources, PassParameters->TileIndirectBuffer->GetIndirectRHICallBuffer(), Substrate::GetClosureTileIndirectArgsOffset(DownsampleFactor));
					}
					else
					{
						RHICmdList.RayTraceDispatch(View.MaterialRayTracingData.PipelineState, RayGenerationShader.GetRayTracingShader(), View.MaterialRayTracingData.ShaderBindingTable, GlobalResources, Resolution.X, Resolution.Y);
					}
				}
				else
				{
					FRayTracingPipelineStateInitializer Initializer;

					const FShaderBindingLayout* ShaderBindingLayout = RayTracing::GetShaderBindingLayout(View.GetShaderPlatform());
					if (ShaderBindingLayout)
					{
						Initializer.ShaderBindingLayout = &ShaderBindingLayout->RHILayout;
					}

					Initializer.MaxPayloadSizeInBytes = GetRayTracingPayloadTypeMaxSize(ERayTracingPayloadType::RayTracingMaterial);

					FRHIRayTracingShader* RayGenShaderTable[] = { RayGenerationShader.GetRayTracingShader() };
					Initializer.SetRayGenShaderTable(RayGenShaderTable);

					FRHIRayTracingShader* HitGroupTable[] = { GetRayTracingDefaultOpaqueShader(View.ShaderMap) };
					Initializer.SetHitGroupTable(HitGroupTable);

					FRHIRayTracingShader* MissGroupTable[] = { GetRayTracingDefaultMissShader(View.ShaderMap) };
					Initializer.SetMissShaderTable(MissGroupTable);

					FRayTracingPipelineState* Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer);

					FShaderBindingTableRHIRef SBT = RayTracingSBT.AllocateTransientRHI(RHICmdList, ERayTracingShaderBindingMode::RTPSO, ERayTracingHitGroupIndexingMode::Disallow, Initializer.GetMaxLocalBindingDataSize());
					
					RHICmdList.SetDefaultRayTracingHitGroup(SBT, Pipeline, 0);
					RHICmdList.SetRayTracingMissShader(SBT, 0, Pipeline, 0 /* ShaderIndexInPipeline */, 0, nullptr, 0);
					RHICmdList.CommitShaderBindingTable(SBT);

					if (bOverflow)
					{
						RHICmdList.RayTraceDispatchIndirect(Pipeline, RayGenerationShader.GetRayTracingShader(), SBT, GlobalResources, PassParameters->TileIndirectBuffer->GetIndirectRHICallBuffer(), Substrate::GetClosureTileIndirectArgsOffset(DownsampleFactor));
					}
					else
					{	
						RHICmdList.RayTraceDispatch(Pipeline, RayGenerationShader.GetRayTracingShader(), SBT, GlobalResources, Resolution.X, Resolution.Y);
					}
				}
			});
	};

	ShortAORTPass(false /*bOverflow*/);
	if (Lumen::SupportsMultipleClosureEvaluation(View) && GRHISupportsRayTracingDispatchIndirect)
	{
		ShortAORTPass(true /*bOverflow*/);
	}
}
#else // RHI_RAYTRACING
{
	unimplemented();
}
#endif // RHI_RAYTRACING
