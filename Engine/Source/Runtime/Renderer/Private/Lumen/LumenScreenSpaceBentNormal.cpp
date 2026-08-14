// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenScreenProbeGather.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "LumenShortRangeAO.h"

static TAutoConsoleVariable<int32> CVarLumenShortRangeAODownsampleFactor(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.DownsampleFactor"),
	2,
	TEXT("Downsampling factor for ShortRangeAO."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenShortRangeAOTemporal(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.Temporal"),
	1,
	TEXT("Whether to run temporal accumulation on Short Range AO"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenShortRangeAOBentNormal(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.BentNormal"),
	1,
	TEXT("Whether to use bent normal or just scalar AO. Scalar AO is slightly faster, but bent normal improves specular occlusion."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenShortRangeAOTemporalNeighborhoodClampScale(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.Temporal.NeighborhoodClampScale"),
	1.0f,
	TEXT("Scales how permissive is neighborhood clamp. Higher values increase ghosting, but reduce noise and instability. Values <= 0 will disable neighborhood clamp."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenShortRangeAOSlopeCompareToleranceScale = .5f;
FAutoConsoleVariableRef CVarLumenShortRangeAOSlopeCompareToleranceScale(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.ScreenSpace.SlopeCompareToleranceScale"),
	GLumenShortRangeAOSlopeCompareToleranceScale,
	TEXT("Scales the slope threshold that screen space traces use to determine whether there was a hit."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenShortRangeAOFoliageOcclusionStrength = .7f;
FAutoConsoleVariableRef CVarLumenShortRangeAOFoliageOcclusionStrength(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.ScreenSpace.FoliageOcclusionStrength"),
	GLumenShortRangeAOFoliageOcclusionStrength,
	TEXT("Maximum strength of ScreenSpaceBentNormal occlusion on foliage and subsurface pixels.  Useful for reducing max occlusion to simulate subsurface scattering."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenMaxShortRangeAOMultibounceAlbedo = .5f;
FAutoConsoleVariableRef CVarLumenMaxShortRangeAOMultibounceAlbedo(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.MaxMultibounceAlbedo"),
	GLumenMaxShortRangeAOMultibounceAlbedo,
	TEXT("Maximum albedo used for the AO multi-bounce approximation.  Useful for forcing near-white albedo to have some occlusion."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenShortRangeAOHairStrandsVoxelTrace = 1;
FAutoConsoleVariableRef GVarLumenShortRangeAOHairStrandsVoxelTrace(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HairVoxelTrace"),
	GLumenShortRangeAOHairStrandsVoxelTrace,
	TEXT("Whether to trace against hair voxel structure for hair casting shadow onto opaques."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenShortRangeAOHairStrandsScreenTrace = 0;
FAutoConsoleVariableRef GVarShortRangeAOHairStrandsScreenTrace(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HairScreenTrace"),
	GLumenShortRangeAOHairStrandsScreenTrace,
	TEXT("Whether to trace against hair depth for hair casting shadow onto opaques."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenShortRangeAOApplyDuringIntegration(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.ApplyDuringIntegration"),
	0,
	TEXT("Whether Screen Space Bent Normal should be applied during BRDF integration, which has higher quality but is before the temporal filter so causes streaking on moving objects."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace LumenShortRangeAO
{
	bool ShouldApplyDuringIntegration()
	{
		return CVarLumenShortRangeAOApplyDuringIntegration.GetValueOnAnyThread() != 0;
	}

	bool UseBentNormal()
	{
		return ShouldApplyDuringIntegration() || CVarLumenShortRangeAOBentNormal.GetValueOnRenderThread() != 0;
	}

	EPixelFormat GetTextureFormat()
	{
		return LumenShortRangeAO::UseBentNormal() ? PF_R32_UINT : PF_R8;
	}

	uint32 GetDownsampleFactor()
	{
		if (ShouldApplyDuringIntegration() || !UseTemporal())
		{
			return 1;
		}

		return FMath::Clamp(CVarLumenShortRangeAODownsampleFactor.GetValueOnRenderThread(), 1, 2);
	}

	bool UseTemporal()
	{
		return CVarLumenShortRangeAOTemporal.GetValueOnRenderThread() != 0;
	}

	float GetTemporalNeighborhoodClampScale()
	{
		return CVarLumenShortRangeAOTemporalNeighborhoodClampScale.GetValueOnRenderThread();
	}
}

class FScreenSpaceShortRangeAOCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenSpaceShortRangeAOCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenSpaceShortRangeAOCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, RWShortRangeAO)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightingChannelsTexture)
		SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)
		SHADER_PARAMETER(uint32, ScreenProbeGatherStateFrameIndex)
		SHADER_PARAMETER(FIntPoint, ShortRangeAOViewMin)
		SHADER_PARAMETER(FIntPoint, ShortRangeAOViewSize)
		SHADER_PARAMETER_STRUCT_INCLUDE(FHZBParameters, HZBParameters)
		SHADER_PARAMETER(float, SlopeCompareToleranceScale)
		SHADER_PARAMETER(float, MaxScreenTraceFraction)
		SHADER_PARAMETER(float, ScreenTraceNoFallbackThicknessScale)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, HairStrandsVoxel)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FNumPixelRays : SHADER_PERMUTATION_SPARSE_INT("NUM_PIXEL_RAYS", 4, 8, 16);
	class FOverflow : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE"); 
	class FHairStrandsScreen : SHADER_PERMUTATION_BOOL("USE_HAIRSTRANDS_SCREEN");
	class FHairStrandsVoxel : SHADER_PERMUTATION_BOOL("USE_HAIRSTRANDS_VOXEL");
	class FOutputBentNormal : SHADER_PERMUTATION_BOOL("OUTPUT_BENT_NORMAL");
	class FDownsampleFactor : SHADER_PERMUTATION_RANGE_INT("DOWNSAMPLE_FACTOR", 1, 2);
	class FUseDistanceFieldRepresentationBit : SHADER_PERMUTATION_BOOL("USE_DISTANCE_FIELD_REPRESENTATION_BIT");
	using FPermutationDomain = TShaderPermutationDomain<FNumPixelRays, FOverflow, FHairStrandsScreen, FHairStrandsVoxel, FOutputBentNormal, FDownsampleFactor, FUseDistanceFieldRepresentationBit>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (!Substrate::IsSubstrateEnabled())
		{
			PermutationVector.Set<FOverflow>(false);
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

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize() 
	{
		// Sanity check
		static_assert(8 == SUBSTRATE_TILE_SIZE);
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenSpaceShortRangeAOCS, "/Engine/Private/Lumen/LumenScreenSpaceBentNormal.usf", "ScreenSpaceShortRangeAOCS", SF_Compute);

FLumenScreenSpaceBentNormalParameters ComputeScreenSpaceShortRangeAO(
	FRDGBuilder& GraphBuilder, 
	const FScene* Scene,
	const FViewInfo& View, 
	const FSceneTextures& SceneTextures,
	FRDGTextureRef LightingChannelsTexture,
	const FBlueNoise& BlueNoise,
	float MaxScreenTraceFraction,
	float ScreenTraceNoFallbackThicknessScale,
	ERDGPassFlags ComputePassFlags)
{
	const FSceneTextureParameters& SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, SceneTextures);

	// When Substrate is enabled, increase the resolution for multi-layer tile overflowing (tile containing multi-BSDF data)
	const int32 DownsampleFactor = LumenShortRangeAO::GetDownsampleFactor();
	FIntPoint ShortRangeAOBufferSize = Substrate::GetSubstrateTextureResolution(View, FIntPoint::DivideAndRoundUp(View.GetSceneTexturesConfig().Extent, DownsampleFactor));
	FIntPoint ShortRangeAOViewMin = FIntPoint::DivideAndRoundUp(View.ViewRect.Min, DownsampleFactor);
	FIntPoint ShortRangeAOViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), DownsampleFactor);
	const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);

	FLumenScreenSpaceBentNormalParameters OutParameters;
	OutParameters.ShortRangeAOViewMin = ShortRangeAOViewMin;
	OutParameters.ShortRangeAOViewSize = ShortRangeAOViewSize;

	FRDGTextureRef ShortRangeAO = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2DArray(ShortRangeAOBufferSize, LumenShortRangeAO::GetTextureFormat(), FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount),
		TEXT("Lumen.ScreenProbeGather.ShortRangeAO"));

	if (DownsampleFactor != 1)
	{
		OutParameters.DownsampledSceneDepth = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(ShortRangeAOBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("Lumen.ScreenProbeGather.DownsampledSceneDepth"));

		OutParameters.DownsampledSceneWorldNormal = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(ShortRangeAOBufferSize, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("Lumen.ScreenProbeGather.DownsampledSceneWorldNormal"));
	}

	int32 NumPixelRays = 4;

	if (View.FinalPostProcessSettings.LumenFinalGatherQuality >= 6.0f)
	{
		NumPixelRays = 16;
	}
	else if (View.FinalPostProcessSettings.LumenFinalGatherQuality >= 2.0f)
	{
		NumPixelRays = 8;
	}

	if (Lumen::UseHardwareRayTracedShortRangeAO(*View.Family))
	{
		RenderHardwareRayTracingShortRangeAO(
			GraphBuilder,
			Scene,
			SceneTextures,
			SceneTextureParameters,
			OutParameters,
			BlueNoise,
			MaxScreenTraceFraction,
			View,
			ShortRangeAO,
			NumPixelRays);
	}
	else
	{
		const bool bNeedTraceHairVoxel = HairStrands::HasViewHairStrandsVoxelData(View) && GLumenShortRangeAOHairStrandsVoxelTrace > 0;
		const bool bNeedTraceHairScreen = HairStrands::HasViewHairStrandsData(View) && GLumenShortRangeAOHairStrandsScreenTrace > 0;
		const bool bUseHardwareRayTracing = Lumen::UseHardwareRayTracedScreenProbeGather(*View.Family);
		
		auto ScreenSpaceShortRangeAO = [&](bool bOverflow)
		{
			FScreenSpaceShortRangeAOCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenSpaceShortRangeAOCS::FParameters>();
			PassParameters->RWShortRangeAO = GraphBuilder.CreateUAV(ShortRangeAO);
			PassParameters->RWDownsampledSceneDepth = OutParameters.DownsampledSceneDepth ? GraphBuilder.CreateUAV(OutParameters.DownsampledSceneDepth) : nullptr;
			PassParameters->RWDownsampledSceneWorldNormal = OutParameters.DownsampledSceneWorldNormal ? GraphBuilder.CreateUAV(OutParameters.DownsampledSceneWorldNormal) : nullptr;
			PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
			PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
			PassParameters->SceneTextures = SceneTextureParameters;

			if (!PassParameters->SceneTextures.GBufferVelocityTexture)
			{
				PassParameters->SceneTextures.GBufferVelocityTexture = GSystemTextures.GetBlackDummy(GraphBuilder);
			}

			PassParameters->MaxScreenTraceFraction = MaxScreenTraceFraction;
			PassParameters->ScreenTraceNoFallbackThicknessScale = ScreenTraceNoFallbackThicknessScale;
			PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
			PassParameters->LightingChannelsTexture = LightingChannelsTexture;
			PassParameters->BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);
			PassParameters->ScreenProbeGatherStateFrameIndex = LumenScreenProbeGather::GetStateFrameIndex(View.ViewState);
			PassParameters->ShortRangeAOViewMin = ShortRangeAOViewMin;
			PassParameters->ShortRangeAOViewSize = ShortRangeAOViewSize;
			PassParameters->HZBParameters = GetHZBParameters(GraphBuilder, View, EHZBType::FurthestHZB);
			PassParameters->SlopeCompareToleranceScale = GLumenShortRangeAOSlopeCompareToleranceScale;

			if (bNeedTraceHairScreen)
			{
				PassParameters->HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
			}

			if (bNeedTraceHairVoxel)
			{
				PassParameters->HairStrandsVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
			}

			FScreenSpaceShortRangeAOCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FScreenSpaceShortRangeAOCS::FNumPixelRays >(NumPixelRays);
			PermutationVector.Set<FScreenSpaceShortRangeAOCS::FOverflow>(bOverflow);
			PermutationVector.Set<FScreenSpaceShortRangeAOCS::FHairStrandsScreen>(bNeedTraceHairScreen);
			PermutationVector.Set<FScreenSpaceShortRangeAOCS::FHairStrandsVoxel>(bNeedTraceHairVoxel);
			PermutationVector.Set<FScreenSpaceShortRangeAOCS::FOutputBentNormal>(LumenShortRangeAO::UseBentNormal() ? 1 : 0);
			PermutationVector.Set<FScreenSpaceShortRangeAOCS::FDownsampleFactor>(DownsampleFactor);
			PermutationVector.Set<FScreenSpaceShortRangeAOCS::FUseDistanceFieldRepresentationBit>(Lumen::IsUsingDistanceFieldRepresentationBit(View));
			PermutationVector = FScreenSpaceShortRangeAOCS::RemapPermutation(PermutationVector);
			auto ComputeShader = View.ShaderMap->GetShader<FScreenSpaceShortRangeAOCS>(PermutationVector);

			if (bOverflow)
			{
				PassParameters->TileIndirectBuffer = View.SubstrateViewData.ClosureTileDispatchIndirectBuffer;
				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ShortRangeAO_ScreenSpace(Rays=%u, DownsampleFactor:%d, BentNormal:%d, Overflow)", NumPixelRays, DownsampleFactor, LumenShortRangeAO::UseBentNormal()),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					View.SubstrateViewData.ClosureTileDispatchIndirectBuffer,
					Substrate::GetClosureTileIndirectArgsOffset(DownsampleFactor));
			}
			else
			{
				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ShortRangeAO_ScreenSpace(Rays=%u, DownsampleFactor:%d, BentNormal:%d)", NumPixelRays, DownsampleFactor, LumenShortRangeAO::UseBentNormal()),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					FComputeShaderUtils::GetGroupCount(ShortRangeAOViewSize, FScreenSpaceShortRangeAOCS::GetGroupSize()));
			}
		};

		ScreenSpaceShortRangeAO(false);
		if (Lumen::SupportsMultipleClosureEvaluation(View))
		{
			ScreenSpaceShortRangeAO(true);
		}
	}

	OutParameters.ShortRangeAOTexture = ShortRangeAO;
	OutParameters.ShortRangeAOMode = LumenShortRangeAO::UseBentNormal() ? 2 : 1;
	return OutParameters;
}