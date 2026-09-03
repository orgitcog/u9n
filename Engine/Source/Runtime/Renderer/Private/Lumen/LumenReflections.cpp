// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenReflections.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "SingleLayerWaterRendering.h"
#include "LumenTracingUtils.h"
#include "LumenFrontLayerTranslucency.h"
#include "RayTracedTranslucency.h"
#include "FirstPersonSceneExtension.h"
#include "LumenScreenProbeGather.h"

extern FLumenGatherCvarState GLumenGatherCvars;

static TAutoConsoleVariable<int> CVarLumenAllowReflections(
	TEXT("r.Lumen.Reflections.Allow"),
	1,
	TEXT("Whether to allow Lumen Reflections.  Lumen Reflections is enabled in the project settings, this cvar can only disable it."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> GVarLumenReflectionsDownsampleFactor(
	TEXT("r.Lumen.Reflections.DownsampleFactor"),
	1,
	TEXT("Downsample factor from the main viewport to trace rays. This is the main performance control for the tracing part of the reflections."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsDownsampleCheckerboard(
	TEXT("r.Lumen.Reflections.DownsampleCheckerboard"),
	0,
	TEXT("Whether to use checkerboard downsampling when DownsampleFactor is greater than one."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReflectionTraceMeshSDFs = 1;
FAutoConsoleVariableRef GVarLumenReflectionTraceMeshSDFs(
	TEXT("r.Lumen.Reflections.TraceMeshSDFs"),
	GLumenReflectionTraceMeshSDFs,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsRadianceCache(
	TEXT("r.Lumen.Reflections.RadianceCache"),
	0,
	TEXT("Whether to reuse Lumen's ScreenProbeGather Radiance Cache, when it is available.  When enabled, reflection rays from rough surfaces are shortened and distant lighting comes from interpolating from the Radiance Cache, speeding up traces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsRadianceCacheStochasticInterapolation(
	TEXT("r.Lumen.Reflections.RadianceCache.StochasticInterpolation"),
	1,
	TEXT("Whether to use stochastic probe interpolation for reflection ray radiance cache lookups."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenReflectionsRadianceCacheMinRoughness(
	TEXT("r.Lumen.Reflections.RadianceCache.MinRoughness"),
	0.2f,
	TEXT("Min roughness where radiance cache should be used at all."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenReflectionsRadianceCacheMaxRoughness(
	TEXT("r.Lumen.Reflections.RadianceCache.MaxRoughness"),
	0.35f,
	TEXT("Roughness value where reflections rays are shortened to minimum (radiance cache probe footprint radius)."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenReflectionsRadianceCacheMinTraceDistance(
	TEXT("r.Lumen.Reflections.RadianceCache.MinTraceDistance"),
	1000.0f,
	TEXT("Min reflection trace distance before the Radiance Cache probe lookup. This will be used at r.Lumen.Reflections.RadianceCache.MaxRoughness treshold."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenReflectionsRadianceCacheMaxTraceDistance(
	TEXT("r.Lumen.Reflections.RadianceCache.MaxTraceDistance"),
	5000.0f,
	TEXT("Max reflection trace distance before the Radiance Cache probe lookup. This will be used at r.Lumen.Reflections.RadianceCache.MinRoughness treshold."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenReflectionsRadianceCacheRoughnessFadeLength(
	TEXT("r.Lumen.Reflections.RadianceCache.RoughnessFadeLength"),
	0.05f,
	TEXT("Roughness range for fading between radiance cache roughness tresholds."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReflectionRadianceCacheReprojectionRadiusScale = 10.0f;
FAutoConsoleVariableRef CVarLumenReflectionRadianceCacheReprojectionRadiusScale(
	TEXT("r.Lumen.Reflections.RadianceCache.ReprojectionRadiusScale"),
	GLumenReflectionRadianceCacheReprojectionRadiusScale,
	TEXT("Scales the radius of the sphere around each Radiance Cache probe that is intersected for parallax correction when interpolating from the Radiance Cache."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarLumenReflectionMaxRoughnessToTrace(
	TEXT("r.Lumen.Reflections.MaxRoughnessToTrace"),
	-1.0f,
	TEXT("Max roughness value for which Lumen still traces dedicated reflection rays. Overrides Post Process Volume settings when set to anything >= 0."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarLumenReflectionMaxRoughnessToTraceClamp(
	TEXT("r.Lumen.Reflections.MaxRoughnessToTraceClamp"),
	1.0f,
	TEXT("Scalability clamp for max roughness value for which Lumen still traces dedicated reflection rays. Project and Post Process Volumes settings are clamped to this value. Useful for scalability."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarLumenReflectionsMaxRoughnessToTraceForFoliage(
	TEXT("r.Lumen.Reflections.MaxRoughnessToTraceForFoliage"),
	0.2f,
	TEXT("Max roughness value for which Lumen still traces dedicated reflection rays from foliage pixels. Where foliage pixel is a pixel with two sided or subsurface shading model."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReflectionRoughnessFadeLength = .1f;
FAutoConsoleVariableRef GVarLumenReflectionRoughnessFadeLength(
	TEXT("r.Lumen.Reflections.RoughnessFadeLength"),
	GLumenReflectionRoughnessFadeLength,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReflectionGGXSamplingBias = .1f;
FAutoConsoleVariableRef GVarLumenReflectionGGXSamplingBias(
	TEXT("r.Lumen.Reflections.GGXSamplingBias"),
	GLumenReflectionGGXSamplingBias,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReflectionTemporalFilter = 1;
FAutoConsoleVariableRef CVarLumenReflectionTemporalFilter(
	TEXT("r.Lumen.Reflections.Temporal"),
	GLumenReflectionTemporalFilter,
	TEXT("Whether to use a temporal filter"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarLumenReflectionTemporalMaxFramesAccumulated(
	TEXT("r.Lumen.Reflections.Temporal.MaxFramesAccumulated"),
	12.0f,
	TEXT("Lower values cause the temporal filter to propagate lighting changes faster, but also increase flickering from noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarLumenReflectionsTemporalNeighborhoodClampScale(
	TEXT("r.Lumen.Reflections.Temporal.NeighborhoodClampScale"),
	1.0f,
	TEXT("Scales how permissive is neighborhood clamp. Higher values reduce noise, but also increase ghosting."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarLumenReflectionTemporalMaxRayDirections(
	TEXT("r.Lumen.Reflections.Temporal.MaxRayDirections"),
	1024,
	TEXT("Number of possible random directions per pixel."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

float GLumenReflectionHistoryDistanceThreshold = .03f;
FAutoConsoleVariableRef CVarLumenReflectionHistoryDistanceThreshold(
	TEXT("r.Lumen.Reflections.Temporal.DistanceThreshold"),
	GLumenReflectionHistoryDistanceThreshold,
	TEXT("World space distance threshold needed to discard last frame's lighting results.  Lower values reduce ghosting from characters when near a wall but increase flickering artifacts."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<int32> CVarLumenReflectionMaxRayIntensity(
	TEXT("r.Lumen.Reflections.MaxRayIntensity"),
	40.0f,
	TEXT("Clamps the maximum ray lighting intensity (with PreExposure) to reduce fireflies."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReflectionSmoothBias = 0.0f;
FAutoConsoleVariableRef GVarLumenReflectionSmoothBias(
	TEXT("r.Lumen.Reflections.SmoothBias"),
	GLumenReflectionSmoothBias,
	TEXT("Values larger than 0 apply a global material roughness bias for Lumen Reflections, where 1 is fully mirror."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReflectionScreenSpaceReconstruction = 1;
FAutoConsoleVariableRef CVarLumenReflectionScreenSpaceReconstruction(
	TEXT("r.Lumen.Reflections.ScreenSpaceReconstruction"),
	GLumenReflectionScreenSpaceReconstruction,
	TEXT("Whether to use the screen space BRDF reweighting reconstruction"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenReflectionScreenSpaceReconstructionNumSamples = 5;
FAutoConsoleVariableRef CVarLumenReflectionScreenSpaceReconstructionNumSamples(
	TEXT("r.Lumen.Reflections.ScreenSpaceReconstruction.NumSamples"),
	GLumenReflectionScreenSpaceReconstructionNumSamples,
	TEXT("Number of samples to use for the screen space BRDF reweighting reconstruction"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarLumenReflectionScreenSpaceReconstructionKernelRadius(
	TEXT("r.Lumen.Reflections.ScreenSpaceReconstruction.KernelRadius"),
	8.0f,
	TEXT("Screen space reflection filter kernel radius in pixels"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

float GLumenReflectionScreenSpaceReconstructionRoughnessScale = 1.0f;
FAutoConsoleVariableRef CVarLumenReflectionScreenSpaceReconstructionRoughnessScale(
	TEXT("r.Lumen.Reflections.ScreenSpaceReconstruction.RoughnessScale"),
	GLumenReflectionScreenSpaceReconstructionRoughnessScale,
	TEXT("Values higher than 1 allow neighbor traces to be blurred together more aggressively, but is not physically correct."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarLumenReflectionsDenoiserTonemapRange(
	TEXT("r.Lumen.Reflections.DenoiserTonemapRange"),
	10.0f,
	TEXT("Max lighting intensity (with PreExposure) for tonemapping during denoising.\n")
	TEXT("Lower values supress more fireflies and noise, but also remove more bright interesting features in reflections.\n")
	TEXT("Compared to r.Lumen.Reflections.MaxRayIntensity it preserves energy in areas without noise.\n")
	TEXT("0 will disable any tonemapping."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarLumenReflectionScreenSpaceReconstructionMinWeight(
	TEXT("r.Lumen.Reflections.ScreenSpaceReconstruction.MinWeight"),
	0.0f,
	TEXT("Min neighorhood weight adding some filtering even if we don't find good rays. It helps with noise on thin features when using downsampled tracing, but removes some contact shadows."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

int32 GLumenReflectionBilateralFilter = 1;
FAutoConsoleVariableRef CVarLumenReflectionBilateralFilter(
	TEXT("r.Lumen.Reflections.BilateralFilter"),
	GLumenReflectionBilateralFilter,
	TEXT("Whether to do a bilateral filter as a last step in denoising Lumen Reflections."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarLumenReflectionBilateralFilterKernelRadius(
	TEXT("r.Lumen.Reflections.BilateralFilter.KernelRadius"),
	8.0f,
	TEXT("Screen space reflection spatial filter kernel radius in pixels"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

int32 GLumenReflectionBilateralFilterNumSamples = 4;
FAutoConsoleVariableRef CVarLumenReflectionBilateralFilterNumSamples(
	TEXT("r.Lumen.Reflections.BilateralFilter.NumSamples"),
	GLumenReflectionBilateralFilterNumSamples,
	TEXT("Number of bilateral filter samples."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenReflectionBilateralFilterDepthWeightScale = 10000.0f;
FAutoConsoleVariableRef CVarLumenReflectionBilateralFilterDepthWeightScale(
	TEXT("r.Lumen.Reflections.BilateralFilter.DepthWeightScale"),
	GLumenReflectionBilateralFilterDepthWeightScale,
	TEXT("Scales the depth weight of the bilateral filter"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenReflectionsVisualizeTracingCoherency = 0;
FAutoConsoleVariableRef GVarLumenReflectionsVisualizeTracingCoherency(
	TEXT("r.Lumen.Reflections.VisualizeTracingCoherency"),
	GLumenReflectionsVisualizeTracingCoherency,
	TEXT("Set to 1 to capture traces from a random wavefront and draw them on the screen. Set to 1 again to re-capture.  Shaders must enable support first, see DEBUG_SUPPORT_VISUALIZE_TRACE_COHERENCY"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsAsyncCompute(
	TEXT("r.Lumen.Reflections.AsyncCompute"),
	0,
	TEXT("Whether to run Lumen reflection passes on the compute pipe if possible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsSurfaceCacheFeedback(
	TEXT("r.Lumen.Reflections.SurfaceCacheFeedback"),
	1,
	TEXT("Whether to allow writing into virtual surface cache feedback buffer from reflection rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsHiResSurface(
	TEXT("r.Lumen.Reflections.HiResSurface"),
	1,
	TEXT("Whether reflections should sample highest available surface data or use lowest res always resident pages."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReflectionsSpecularScale = 1.f;
FAutoConsoleVariableRef GVarLumenReflectionsSpecularScale(
	TEXT("r.Lumen.Reflections.SpecularScale"),
	GLumenReflectionsSpecularScale,
	TEXT("Non-physically correct Lumen specular reflection scale. Recommended to keep at 1."),
	ECVF_RenderThreadSafe);

float GLumenReflectionsContrast = 1.f;
FAutoConsoleVariableRef GVarLumenReflectionsContrast(
	TEXT("r.Lumen.Reflections.Contrast"),
	GLumenReflectionsContrast,
	TEXT("Non-physically correct Lumen reflection contrast. Recommended to keep at 1."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int> GVarLumenReflectionsFixedStateFrameIndex(
	TEXT("r.Lumen.Reflections.FixedStateFrameIndex"),
	-1,
	TEXT("Whether to override View.StateFrameIndex for debugging Lumen Reflections."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> GVarLumenReflectionsDebug(
	TEXT("r.Lumen.Reflections.Debug"),
	0,
	TEXT("Whether to enable debug mode, which prints various extra debug information from shaders."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracedTranslucencyDebug(
	TEXT("r.RayTracedTranslucency.Debug"),
	0,
	TEXT("Whether to enabled debug mode, which prints various extra debug information from shaders."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarRayTracedTranslucencyMaxRayIntensity(
	TEXT("r.RayTracedTranslucency.MaxRayIntensity"),
	1000.0f,
	TEXT("Clamps the maximum ray lighting intensity (with PreExposure) to reduce fireflies for raytraced translucency surfaces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GetLumenReflectionSpecularScale()
{
	return FMath::Max(GLumenReflectionsSpecularScale, 0.f);
}

float GetLumenReflectionContrast()
{
	return FMath::Clamp(GLumenReflectionsContrast, 0.001f, 1.0f);
}

namespace LumenReflections
{
	int32 GetMaxFramesAccumulated()
	{
		return FMath::Max(CVarLumenReflectionTemporalMaxFramesAccumulated.GetValueOnRenderThread(), 1);
	}

	void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SUBSTRATE_STOCHASTIC_LIGHTING_ALLOWED"), Substrate::IsStochasticLightingEnabled(Parameters.Platform) ? 1 : 0);
	}

	float GetDenoiserOneOverTonemapRange()
	{
		if (CVarLumenReflectionsDenoiserTonemapRange.GetValueOnRenderThread() > 0.0f)
		{
			return 1.0f / CVarLumenReflectionsDenoiserTonemapRange.GetValueOnRenderThread();
		}

		return 0.0f;
	}
};

bool LumenReflections::UseRadianceCache()
{
	return CVarLumenReflectionsRadianceCache.GetValueOnRenderThread() != 0 && LumenScreenProbeGather::UseRadianceCache();
}

bool LumenReflections::UseRadianceCacheSkyVisibility()
{
	return UseRadianceCache() && LumenScreenProbeGather::UseRadianceCacheSkyVisibility();
}

bool LumenReflections::UseRadianceCacheStochasticInterpolation()
{
	return CVarLumenReflectionsRadianceCacheStochasticInterapolation.GetValueOnRenderThread() != 0;
}

bool LumenReflections::UseSurfaceCacheFeedback()
{
	return CVarLumenReflectionsSurfaceCacheFeedback.GetValueOnRenderThread() != 0;
}

bool LumenReflections::UseAsyncCompute(const FViewFamilyInfo& ViewFamily, EDiffuseIndirectMethod DiffuseIndirectMethod)
{
	// Disable async if hit-lighting is used and RHI doesn't support async DispatchRays
	if (!GRHIGlobals.RayTracing.SupportsAsyncRayTraceDispatch && Lumen::UseHardwareRayTracing(ViewFamily))
	{
		check(ViewFamily.Views.Num() > 0 && ViewFamily.Views[0]->bIsViewInfo);
		const FViewInfo& View = *(const FViewInfo*)ViewFamily.Views[0];

		if (LumenReflections::UseHitLighting(View, DiffuseIndirectMethod))
		{
			return false;
		}
	}

	return Lumen::UseAsyncCompute(ViewFamily) 
		&& CVarLumenReflectionsAsyncCompute.GetValueOnRenderThread() != 0
		&& CVarLumenAllowReflections.GetValueOnRenderThread() != 0;
}

void LumenReflections::SetupCompositeParameters(const FViewInfo& View, LumenReflections::FCompositeParameters& OutParameters)
{
	OutParameters.MaxRoughnessToTrace = FMath::Min(View.FinalPostProcessSettings.LumenMaxRoughnessToTraceReflections, CVarLumenReflectionMaxRoughnessToTraceClamp.GetValueOnRenderThread());
	OutParameters.InvRoughnessFadeLength = 1.0f / FMath::Clamp(GLumenReflectionRoughnessFadeLength, 0.001f, 1.0f);
	OutParameters.MaxRoughnessToTraceForFoliage = CVarLumenReflectionsMaxRoughnessToTraceForFoliage.GetValueOnRenderThread();

	if (CVarLumenReflectionMaxRoughnessToTrace.GetValueOnRenderThread() >= 0.0f)
	{
		OutParameters.MaxRoughnessToTrace = CVarLumenReflectionMaxRoughnessToTrace.GetValueOnRenderThread();
	}
}

TRefCountPtr<FRDGPooledBuffer> GVisualizeReflectionTracesData;

FRDGBufferRef SetupVisualizeReflectionTraces(FRDGBuilder& GraphBuilder, FLumenReflectionsVisualizeTracesParameters& VisualizeTracesParameters)
{
	FRDGBufferRef VisualizeTracesData = nullptr;

	if (GVisualizeReflectionTracesData.IsValid())
	{
		VisualizeTracesData = GraphBuilder.RegisterExternalBuffer(GVisualizeReflectionTracesData);
	}

	const int32 VisualizeBufferNumElements = 32 * 3;

	if (!VisualizeTracesData || VisualizeTracesData->Desc.NumElements != VisualizeBufferNumElements)
	{
		VisualizeTracesData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), VisualizeBufferNumElements), TEXT("VisualizeTracesData"));
		AddClearUAVFloatPass(GraphBuilder, GraphBuilder.CreateUAV(VisualizeTracesData, PF_A32B32G32R32F), 0.0f);
	}

	VisualizeTracesParameters.VisualizeTraceCoherency = 0;
	VisualizeTracesParameters.RWVisualizeTracesData = GraphBuilder.CreateUAV(VisualizeTracesData, PF_A32B32G32R32F);

	if (GLumenReflectionsVisualizeTracingCoherency == 1)
	{
		GLumenReflectionsVisualizeTracingCoherency = 2;
		VisualizeTracesParameters.VisualizeTraceCoherency = 1;
	}

	return VisualizeTracesData;
}

void GetReflectionsVisualizeTracesBuffer(TRefCountPtr<FRDGPooledBuffer>& VisualizeTracesData)
{
	if (GVisualizeReflectionTracesData.IsValid() && GLumenReflectionsVisualizeTracingCoherency != 0)
	{
		VisualizeTracesData = GVisualizeReflectionTracesData;
	}
}

// Must match usf RESOLVE_TILE_SIZE
const int32 GReflectionResolveTileSize = 8;

class FReflectionTileClassificationMarkCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReflectionTileClassificationMarkCS)
	SHADER_USE_PARAMETER_STRUCT(FReflectionTileClassificationMarkCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float>, RWDownsampledDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledClosureIndex)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWReflectionClearTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWReflectionResolveTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWReflectionTracingTileIndirectArgs)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<uint>, RWResolveTileUsed)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenFrontLayerTranslucencyGBufferParameters, FrontLayerTranslucencyGBufferParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FFrontLayerTranslucency : SHADER_PERMUTATION_BOOL("FRONT_LAYER_TRANSLUCENCY");
	class FOverflowTile : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	using FPermutationDomain = TShaderPermutationDomain<FFrontLayerTranslucency, FOverflowTile>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOverflowTile>() && !Substrate::IsSubstrateEnabled())
		{
			return false;
		}
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		LumenReflections::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FReflectionTileClassificationMarkCS, "/Engine/Private/Lumen/LumenReflections.usf", "ReflectionTileClassificationMarkCS", SF_Compute);


class FReflectionTileClassificationBuildListsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReflectionTileClassificationBuildListsCS)
	SHADER_USE_PARAMETER_STRUCT(FReflectionTileClassificationBuildListsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWReflectionClearTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWReflectionClearTileData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWReflectionTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWReflectionTileData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, ResolveTileUsed)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
		SHADER_PARAMETER(FIntPoint, TileViewportDimensions)
		SHADER_PARAMETER(FIntPoint, ResolveTileViewportDimensions)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOverflow>() && !Substrate::IsSubstrateEnabled())
		{
			return false;
		}
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	class FSupportDownsample : SHADER_PERMUTATION_BOOL("SUPPORT_DOWNSAMPLE_FACTOR");
	class FOverflow : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	using FPermutationDomain = TShaderPermutationDomain<FSupportDownsample,FOverflow>;

	static int32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		LumenReflections::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FReflectionTileClassificationBuildListsCS, "/Engine/Private/Lumen/LumenReflections.usf", "ReflectionTileClassificationBuildListsCS", SF_Compute);

class FReflectionClearNeighborTileCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReflectionClearNeighborTileCS)
	SHADER_USE_PARAMETER_STRUCT(FReflectionClearNeighborTileCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, RWSpecularAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWSpecularIndirect)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, ResolveTileUsed)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER(FIntPoint, TileViewportDimensions)
		SHADER_PARAMETER(FIntPoint, ResolveTileViewportDimensions)
		SHADER_PARAMETER(uint32, KernelRadiusInTiles)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (!Substrate::IsSubstrateEnabled())
		{
			return false;
		}
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	using FPermutationDomain = TShaderPermutationDomain<>;
	static int32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		LumenReflections::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FReflectionClearNeighborTileCS, "/Engine/Private/Lumen/LumenReflections.usf", "ReflectionClearNeighborTileCS", SF_Compute);

class FReflectionGenerateRaysCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReflectionGenerateRaysCS)
	SHADER_USE_PARAMETER_STRUCT(FReflectionGenerateRaysCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, RWRayBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float>, RWDownsampledDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledClosureIndex)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<uint>, RWRayTraceDistance)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, RadianceCacheMaxRoughness)
		SHADER_PARAMETER(float, RadianceCacheMinRoughness)
		SHADER_PARAMETER(float, RadianceCacheMaxTraceDistance)
		SHADER_PARAMETER(float, RadianceCacheMinTraceDistance)
		SHADER_PARAMETER(float, RadianceCacheRoughnessFadeLength)
		SHADER_PARAMETER(float, GGXSamplingBias)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenFrontLayerTranslucencyGBufferParameters, FrontLayerTranslucencyGBufferParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTileParameters, ReflectionTileParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ResolveIndirectArgsForRead)
	END_SHADER_PARAMETER_STRUCT()

	class FRadianceCache : SHADER_PERMUTATION_BOOL("RADIANCE_CACHE");
	class FFrontLayerTranslucency : SHADER_PERMUTATION_BOOL("FRONT_LAYER_TRANSLUCENCY");
	using FPermutationDomain = TShaderPermutationDomain<FRadianceCache, FFrontLayerTranslucency>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		LumenReflections::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FReflectionGenerateRaysCS, "/Engine/Private/Lumen/LumenReflections.usf", "ReflectionGenerateRaysCS", SF_Compute);

class FLumenReflectionResolveCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenReflectionResolveCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenReflectionResolveCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWSpecularIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float>, RWSpecularIndirectDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWBackgroundVisibility)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<float3>, TraceBackgroundVisibility)
		SHADER_PARAMETER(uint32, ClosureIndex)
		SHADER_PARAMETER(uint32, NumSpatialReconstructionSamples)
		SHADER_PARAMETER(float, SpatialReconstructionKernelRadius)
		SHADER_PARAMETER(float, SpatialReconstructionRoughnessScale)
		SHADER_PARAMETER(float, SpatialReconstructionMinWeight)
		SHADER_PARAMETER(float, ReflectionsDenoiserOneOverTonemapRange)
		SHADER_PARAMETER(float, InvSubstrateMaxClosureCount)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTileParameters, ReflectionTileParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenFrontLayerTranslucencyGBufferParameters, FrontLayerTranslucencyGBufferParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		LumenReflections::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	class FSpatialReconstruction : SHADER_PERMUTATION_BOOL("USE_SPATIAL_RECONSTRUCTION");
	class FFrontLayerTranslucency : SHADER_PERMUTATION_BOOL("FRONT_LAYER_TRANSLUCENCY");
	class FResolveBackgroundVisibility : SHADER_PERMUTATION_BOOL("RESOLVE_BACKGROUND_VISIBILITY");
	class FDownsampleFactorX : SHADER_PERMUTATION_RANGE_INT("DOWNSAMPLE_FACTOR_X", 1, 2);
	class FDownsampleFactorY : SHADER_PERMUTATION_RANGE_INT("DOWNSAMPLE_FACTOR_Y", 1, 2);
	class FUseAnisotropy : SHADER_PERMUTATION_BOOL("USE_ANISOTROPY");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FSpatialReconstruction, FFrontLayerTranslucency, FResolveBackgroundVisibility, FDownsampleFactorX, FDownsampleFactorY, FUseAnisotropy, FDebugMode>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (PermutationVector.Get<FSpatialReconstruction>() == 0)
		{
			PermutationVector.Set<FUseAnisotropy>(1);
		}

		if (PermutationVector.Get<FDownsampleFactorY>() == 2)
		{
			PermutationVector.Set<FDownsampleFactorX>(2);
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

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FDebugMode>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}
		return FGlobalShader::ShouldPrecachePermutation(Parameters);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenReflectionResolveCS, "/Engine/Private/Lumen/LumenReflectionResolve.usf", "LumenReflectionResolveCS", SF_Compute);

bool ShouldRenderLumenReflections(const FSceneView& View, bool bSkipTracingDataCheck, bool bSkipProjectCheck, bool bIncludeStandalone)
{
	const FScene* Scene = (const FScene*)View.Family->Scene;
	if (Scene)
	{
		return Lumen::IsLumenFeatureAllowedForView(Scene, View, bSkipTracingDataCheck, bSkipProjectCheck) 
			&& View.FinalPostProcessSettings.ReflectionMethod == EReflectionMethod::Lumen
			&& View.Family->EngineShowFlags.LumenReflections 
			&& CVarLumenAllowReflections.GetValueOnAnyThread()
			&& (ShouldRenderLumenDiffuseGI(Scene, View, bSkipTracingDataCheck, bSkipProjectCheck)
				// GRHISupportsRayTracingShaders is required for standalone Lumen Reflections because Lumen::LumenHardwareRayTracing::GetHitLightingMode forces hit lighting
				|| (bIncludeStandalone && Lumen::UseHardwareRayTracedReflections(*View.Family) && GRHISupportsRayTracingShaders))
			&& (bSkipTracingDataCheck || Lumen::UseHardwareRayTracedReflections(*View.Family) || Lumen::IsSoftwareRayTracingSupported());
	}
	
	return false;
}

FLumenReflectionTileParameters ReflectionTileClassification(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FMinimalSceneTextures& SceneTextures,
	const FLumenReflectionTracingParameters& ReflectionTracingParameters,
	const FLumenFrontLayerTranslucencyGBufferParameters& FrontLayerReflectionGBuffer,
	ERDGPassFlags ComputePassFlags)
{
	FLumenReflectionTileParameters ReflectionTileParameters;

	const bool bFrontLayer = FrontLayerReflectionGBuffer.FrontLayerTranslucencySceneDepth != nullptr;
	const FIntPoint EffectiveTextureResolution = bFrontLayer ? SceneTextures.Config.Extent : Substrate::GetSubstrateTextureResolution(View, SceneTextures.Config.Extent);
	const uint32 TracedClosureCount = (bFrontLayer || Substrate::IsStochasticLightingActive(View.GetShaderPlatform())) ? 1u : Substrate::GetSubstrateMaxClosureCount(View);

	const FIntPoint ResolveTileViewportDimensions = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GReflectionResolveTileSize);
	const FIntPoint ResolveTileBufferDimensions = FIntPoint::DivideAndRoundUp(EffectiveTextureResolution, GReflectionResolveTileSize);

	const FIntPoint TracingTileSize = ReflectionTracingParameters.ReflectionDownsampleFactorXY * GReflectionResolveTileSize;
	const FIntPoint TracingTileViewportDimensions = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), TracingTileSize);
	const FIntPoint TracingTileBufferDimensions = FIntPoint::DivideAndRoundUp(EffectiveTextureResolution, TracingTileSize);

	const int32 NumResolveTiles = ResolveTileBufferDimensions.X * ResolveTileBufferDimensions.Y * TracedClosureCount;
	const int32 NumTracingTiles = TracingTileBufferDimensions.X * TracingTileBufferDimensions.Y * TracedClosureCount;

	FRDGBufferRef ReflectionClearTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumResolveTiles), TEXT("Lumen.Reflections.ReflectionClearTileData"));
	FRDGBufferRef ReflectionResolveTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumResolveTiles), TEXT("Lumen.Reflections.ReflectionResolveTileData"));

	FRDGBufferRef ReflectionClearTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.Reflections.ReflectionClearTileIndirectArgs"));
	FRDGBufferRef ReflectionResolveTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.Reflections.ReflectionResolveTileIndirectArgs"));
	FRDGBufferRef ReflectionTracingTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.Reflections.ReflectionTracingTileIndirectArgs"));

	FRDGTextureDesc ResolveTileUsedDesc = FRDGTextureDesc::Create2DArray(ResolveTileBufferDimensions, PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, TracedClosureCount);
	FRDGTextureRef ResolveTileUsed = GraphBuilder.CreateTexture(ResolveTileUsedDesc, TEXT("Lumen.Reflections.ResolveTileUsed"));

	{
		FRDGTextureUAVRef RWDownsampledClosureIndex = !bFrontLayer && Substrate::IsStochasticLightingEnabled(View.GetShaderPlatform()) ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.DownsampledClosureIndex), ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
		FRDGTextureUAVRef RWDownsampledDepth = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.DownsampledDepth), ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef RWReflectionClearTileIndirectArgs = GraphBuilder.CreateUAV(ReflectionClearTileIndirectArgs, PF_R32_UINT, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef RWReflectionResolveTileIndirectArgs = GraphBuilder.CreateUAV(ReflectionResolveTileIndirectArgs, PF_R32_UINT, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef RWReflectionTracingTileIndirectArgs = GraphBuilder.CreateUAV(ReflectionTracingTileIndirectArgs, PF_R32_UINT, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef RWResolveTileUsed = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ResolveTileUsed), ERDGUnorderedAccessViewFlags::SkipBarrier);

		auto ReflectionTileClassificationMark = [&](bool bOverflow)
		{
			FReflectionTileClassificationMarkCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionTileClassificationMarkCS::FParameters>();
			PassParameters->RWDownsampledDepth = RWDownsampledDepth;
			PassParameters->RWDownsampledClosureIndex = RWDownsampledClosureIndex;
			PassParameters->RWReflectionClearTileIndirectArgs = RWReflectionClearTileIndirectArgs;
			PassParameters->RWReflectionResolveTileIndirectArgs = RWReflectionResolveTileIndirectArgs;
			PassParameters->RWReflectionTracingTileIndirectArgs = RWReflectionTracingTileIndirectArgs;
			PassParameters->RWResolveTileUsed = RWResolveTileUsed;
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
			PassParameters->FrontLayerTranslucencyGBufferParameters = FrontLayerReflectionGBuffer;
			PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
			PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;

			FReflectionTileClassificationMarkCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FReflectionTileClassificationMarkCS::FOverflowTile>(bOverflow);
			PermutationVector.Set<FReflectionTileClassificationMarkCS::FFrontLayerTranslucency>(bFrontLayer);
			auto ComputeShader = View.ShaderMap->GetShader<FReflectionTileClassificationMarkCS>(PermutationVector);

			checkf(ResolveTileViewportDimensions.X > 0 && ResolveTileViewportDimensions.Y > 0, TEXT("FReflectionTileClassificationMarkCS needs non-zero dispatch to clear next pass's indirect args"));

			{
				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("TileClassificationMark(%dx%d)", View.ViewRect.Size().X, View.ViewRect.Size().Y),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					FIntVector(ResolveTileViewportDimensions.X, ResolveTileViewportDimensions.Y, TracedClosureCount));
			}
		};

		ReflectionTileClassificationMark(false);
	}

	// Classification for reflection tiles
	auto ReflectionTileClassificationBuildLists = [&](bool bOverflow)
	{
		FReflectionTileClassificationBuildListsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionTileClassificationBuildListsCS::FParameters>();
		PassParameters->RWReflectionClearTileIndirectArgs = GraphBuilder.CreateUAV(ReflectionClearTileIndirectArgs, PF_R32_UINT);
		PassParameters->RWReflectionClearTileData = GraphBuilder.CreateUAV(ReflectionClearTileData, PF_R32_UINT);
		PassParameters->RWReflectionTileIndirectArgs = GraphBuilder.CreateUAV(ReflectionResolveTileIndirectArgs, PF_R32_UINT);
		PassParameters->RWReflectionTileData = GraphBuilder.CreateUAV(ReflectionResolveTileData, PF_R32_UINT);
		PassParameters->ResolveTileUsed = ResolveTileUsed;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->TileViewportDimensions = ResolveTileViewportDimensions;
		PassParameters->ResolveTileViewportDimensions = ResolveTileViewportDimensions;
		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;

		FReflectionTileClassificationBuildListsCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FReflectionTileClassificationBuildListsCS::FSupportDownsample>(false);
		PermutationVector.Set<FReflectionTileClassificationBuildListsCS::FOverflow>(bOverflow);
		auto ComputeShader = View.ShaderMap->GetShader<FReflectionTileClassificationBuildListsCS>(PermutationVector);

		if (bOverflow)
		{
			PassParameters->TileIndirectBuffer = View.SubstrateViewData.ClosureTilePerThreadDispatchIndirectBuffer;
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TileClassificationBuildLists(Overflow)"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				View.SubstrateViewData.ClosureTilePerThreadDispatchIndirectBuffer, 0u);
		}
		else
		{
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TileClassificationBuildLists"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(ResolveTileViewportDimensions, FReflectionTileClassificationBuildListsCS::GetGroupSize()));
		}
	};

	ReflectionTileClassificationBuildLists(false);
	if (Lumen::SupportsMultipleClosureEvaluation(View)
		&& !(bFrontLayer || Substrate::IsStochasticLightingActive(View.GetShaderPlatform())))
	{
		ReflectionTileClassificationBuildLists(true);
	}

	// Classification for reflection 'tracing' tiles
	FRDGBufferRef ReflectionTracingTileData;
	if (ReflectionTracingParameters.ReflectionDownsampleFactorXY == 1)
	{
		ReflectionTracingTileIndirectArgs = ReflectionResolveTileIndirectArgs;
		ReflectionTracingTileData = ReflectionResolveTileData;
	}
	else
	{
		ReflectionTracingTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumTracingTiles), TEXT("Lumen.Reflections.ReflectionTracingTileData"));

		FReflectionTileClassificationBuildListsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionTileClassificationBuildListsCS::FParameters>();
		PassParameters->RWReflectionTileIndirectArgs = GraphBuilder.CreateUAV(ReflectionTracingTileIndirectArgs, PF_R32_UINT);
		PassParameters->RWReflectionTileData = GraphBuilder.CreateUAV(ReflectionTracingTileData, PF_R32_UINT);
		PassParameters->ResolveTileUsed = ResolveTileUsed;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->TileViewportDimensions = TracingTileViewportDimensions;
		PassParameters->ResolveTileViewportDimensions = ResolveTileViewportDimensions;
		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;

		FReflectionTileClassificationBuildListsCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FReflectionTileClassificationBuildListsCS::FSupportDownsample >(true);
		auto ComputeShader = View.ShaderMap->GetShader<FReflectionTileClassificationBuildListsCS>(PermutationVector);

		// When using downsampled tracing, dispatch for all layers rather using linear sparse set of tiles (i.e., ClosureTilePerThreadDispatchIndirectBuffer) 
		// for easing logic within the TileClassificationBuildList shader
		FIntVector DispatchCount = FComputeShaderUtils::GetGroupCount(TracingTileViewportDimensions, FReflectionTileClassificationBuildListsCS::GetGroupSize());
		DispatchCount.Z = TracedClosureCount;
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TileClassificationBuildTracingLists"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			DispatchCount);
	}

	ReflectionTileParameters.ResolveTileUsed = ResolveTileUsed;
	ReflectionTileParameters.ClearIndirectArgs = ReflectionClearTileIndirectArgs;
	ReflectionTileParameters.ResolveIndirectArgs = ReflectionResolveTileIndirectArgs;
	ReflectionTileParameters.TracingIndirectArgs = ReflectionTracingTileIndirectArgs;
	ReflectionTileParameters.ReflectionClearTileData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ReflectionClearTileData, PF_R32_UINT));
	ReflectionTileParameters.ReflectionResolveTileData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ReflectionResolveTileData, PF_R32_UINT));
	ReflectionTileParameters.ReflectionTracingTileData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ReflectionTracingTileData, PF_R32_UINT));
	return ReflectionTileParameters;
}

BEGIN_SHADER_PARAMETER_STRUCT(FLumenReflectionDenoiserParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLumenFrontLayerTranslucencyGBufferParameters, FrontLayerTranslucencyGBufferParameters)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
	SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTileParameters, ReflectionTileParameters)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
	SHADER_PARAMETER(float, InvSubstrateMaxClosureCount)
END_SHADER_PARAMETER_STRUCT()

class FLumenReflectionDenoiserTemporalCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenReflectionDenoiserTemporalCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenReflectionDenoiserTemporalCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionDenoiserParameters, DenoiserParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<float4>, ResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, ResolvedReflectionsDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<float4>, SpecularHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<UNORM float>, NumFramesAccumulatedHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneDepthHistory)
		SHADER_PARAMETER(uint32, ClosureIndex)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryGatherUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryBufferSizeAndInvSize)
		SHADER_PARAMETER(float, PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, RWSpecularAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<UNORM float>, RWNumFramesAccumulated)
		SHADER_PARAMETER(float, TemporalMaxFramesAccumulated)
		SHADER_PARAMETER(float, TemporalNeighborhoodClampScale)
		SHADER_PARAMETER(float, HistoryDistanceThreshold)
		SHADER_PARAMETER(float, ReflectionsDenoiserOneOverTonemapRange)
	END_SHADER_PARAMETER_STRUCT()

	class FValidHistory : SHADER_PERMUTATION_BOOL("PERMUTATION_VALID_HISTORY");
	class FRayTracedTranslucencyLighting : SHADER_PERMUTATION_BOOL("RAY_TRACED_TRANSLUCENCY_LIGHTING");
	class FDebug : SHADER_PERMUTATION_BOOL("PERMUTATION_DEBUG");
	using FPermutationDomain = TShaderPermutationDomain<FValidHistory, FRayTracedTranslucencyLighting, FDebug>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		LumenReflections::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FDebug>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}
		return FGlobalShader::ShouldPrecachePermutation(Parameters);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenReflectionDenoiserTemporalCS, "/Engine/Private/Lumen/LumenReflectionDenoiserTemporal.usf", "LumenReflectionDenoiserTemporalCS", SF_Compute);

class FLumenReflectionDenoiserClearCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenReflectionDenoiserClearCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenReflectionDenoiserClearCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTileParameters, ReflectionTileParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWResolvedSpecular)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, RWSpecularAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWFinalRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWBackgroundVisibility)
		SHADER_PARAMETER(uint32, bClearToSceneColor)
		SHADER_PARAMETER(uint32, ClosureIndex)
	END_SHADER_PARAMETER_STRUCT()

	class FClearFinalRadianceAndBackgroundVisibility : SHADER_PERMUTATION_BOOL("CLEAR_FINAL_RADIANCE_AND_BACKGROUND_VISIBILITY");
	using FPermutationDomain = TShaderPermutationDomain<FClearFinalRadianceAndBackgroundVisibility>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		LumenReflections::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenReflectionDenoiserClearCS, "/Engine/Private/Lumen/LumenReflectionDenoiserClear.usf", "LumenReflectionDenoiserClearCS", SF_Compute);

class FLumenReflectionDenoiserSpatialCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenReflectionDenoiserSpatialCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenReflectionDenoiserSpatialCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionDenoiserParameters, DenoiserParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWSpecularIndirectAccumulated)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWTranslucencyLighting)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<float3>, SpecularLightingAndSecondMomentTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, BackgroundVisibilityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<UNORM float>, NumFramesAccumulatedTexture)
		SHADER_PARAMETER(float, SpatialFilterDepthWeightScale)
		SHADER_PARAMETER(float, SpatialFilterKernelRadius)
		SHADER_PARAMETER(uint32, SpatialFilterNumSamples)
		SHADER_PARAMETER(float, TemporalMaxFramesAccumulated)
		SHADER_PARAMETER(uint32, bCompositeSceneColor)
		SHADER_PARAMETER(uint32, ClosureIndex)
		SHADER_PARAMETER(float, ReflectionsDenoiserOneOverTonemapRange)
	END_SHADER_PARAMETER_STRUCT()

	class FFrontLayerTranslucency : SHADER_PERMUTATION_BOOL("FRONT_LAYER_TRANSLUCENCY");
	class FRayTracedTranslucency : SHADER_PERMUTATION_BOOL("RAY_TRACED_TRANSLUCENCY");
	class FSpatialFilter : SHADER_PERMUTATION_BOOL("SPATIAL_FILTER");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FFrontLayerTranslucency, FRayTracedTranslucency, FSpatialFilter, FDebugMode>;

	static int32 GetGroupSize()
	{	
		return 8;
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (PermutationVector.Get<FRayTracedTranslucency>())
		{
			PermutationVector.Set<FFrontLayerTranslucency>(true);
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

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		LumenReflections::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FDebugMode>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}
		return FGlobalShader::ShouldPrecachePermutation(Parameters);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenReflectionDenoiserSpatialCS, "/Engine/Private/Lumen/LumenReflectionDenoiserSpatial.usf", "LumenReflectionDenoiserSpatialCS", SF_Compute);


DECLARE_GPU_STAT(LumenReflections);

FRDGTextureRef FDeferredShadingSceneRenderer::RenderLumenReflections(
	FRDGBuilder& GraphBuilder, 
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	FLumenSceneFrameTemporaries& FrameTemporaries,
	const FLumenMeshSDFGridParameters& MeshSDFGridParameters,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& ScreenProbeRadianceCacheParameters,
	ELumenReflectionPass ReflectionPass,
	const FLumenReflectionsConfig& ReflectionsConfig,
	ERDGPassFlags ComputePassFlags)
{
	const bool bFrontLayer = ReflectionPass == ELumenReflectionPass::FrontLayerTranslucency;
	const bool bSingleLayerWater = ReflectionPass == ELumenReflectionPass::SingleLayerWater;
	const EDiffuseIndirectMethod DiffuseIndirectMethod = GetViewPipelineState(View).DiffuseIndirectMethod;

	check(ShouldRenderLumenReflections(View));
	if (ReflectionPass == ELumenReflectionPass::FrontLayerTranslucency)
	{
		check(ReflectionsConfig.FrontLayerReflectionGBuffer.FrontLayerTranslucencySceneDepth->Desc.Extent == SceneTextures.Config.Extent);
	}

	LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheParameters = ScreenProbeRadianceCacheParameters;
	RadianceCacheParameters.RadianceCacheInputs.ReprojectionRadiusScale = FMath::Clamp<float>(GLumenReflectionRadianceCacheReprojectionRadiusScale, 1.0f, 100000.0f);

	LLM_SCOPE_BYTAG(Lumen);
	RDG_EVENT_SCOPE_STAT(GraphBuilder, LumenReflections, "LumenReflections");
	RDG_GPU_STAT_SCOPE(GraphBuilder, LumenReflections);

	FLumenReflectionTracingParameters ReflectionTracingParameters;
	{
		LumenReflections::SetupCompositeParameters(View, ReflectionTracingParameters.ReflectionsCompositeParameters);
		ReflectionTracingParameters.PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRHI();
		ReflectionTracingParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		uint32 StateFrameIndex = View.ViewState ? View.ViewState->GetFrameIndex() : 0;
		if (GVarLumenReflectionsFixedStateFrameIndex.GetValueOnRenderThread() >= 0)
		{
			StateFrameIndex = GVarLumenReflectionsFixedStateFrameIndex.GetValueOnRenderThread();
		}

		ReflectionTracingParameters.ReflectionsStateFrameIndex = StateFrameIndex;
		ReflectionTracingParameters.ReflectionsStateFrameIndexMod8 = StateFrameIndex % 8;
		ReflectionTracingParameters.ReflectionsRayDirectionFrameIndex = StateFrameIndex % FMath::Max(CVarLumenReflectionTemporalMaxRayDirections.GetValueOnRenderThread(), 1);
	}

	FRDGBufferRef VisualizeTracesData = nullptr;
	
	if (ReflectionPass == ELumenReflectionPass::Opaque)
	{
		VisualizeTracesData = SetupVisualizeReflectionTraces(GraphBuilder, ReflectionTracingParameters.VisualizeTracesParameters);
	}

	// Compute effective reflection downsampling factor. 
	const bool bCheckerboardDownsample = CVarLumenReflectionsDownsampleCheckerboard.GetValueOnRenderThread() != 0;
	const int32 UserDownsampleFactor = View.FinalPostProcessSettings.LumenReflectionQuality <= .25f ? 2 : 1;
	FIntPoint LumenReflectionDownsampleFactorXY = FMath::Clamp(GVarLumenReflectionsDownsampleFactor.GetValueOnRenderThread() * UserDownsampleFactor, 1, 2);
	if (bCheckerboardDownsample)
	{
		LumenReflectionDownsampleFactorXY.Y = 1;
	}
	if (ReflectionsConfig.DownsampleFactorXY.X >= 0 && ReflectionsConfig.DownsampleFactorXY.Y >= 0)
	{
		LumenReflectionDownsampleFactorXY = ReflectionsConfig.DownsampleFactorXY;
		LumenReflectionDownsampleFactorXY.X = FMath::Clamp(LumenReflectionDownsampleFactorXY.X, 1, 2);
		LumenReflectionDownsampleFactorXY.Y = FMath::Clamp(LumenReflectionDownsampleFactorXY.Y, 1, 2);
	}

	ReflectionTracingParameters.ReflectionDownsampleFactorXY = LumenReflectionDownsampleFactorXY;
	const FIntPoint ViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), ReflectionTracingParameters.ReflectionDownsampleFactorXY);
	FIntPoint BufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, ReflectionTracingParameters.ReflectionDownsampleFactorXY);
	if (!bFrontLayer && !bSingleLayerWater)
	{
		BufferSize = Substrate::GetSubstrateTextureResolution(View, BufferSize);
	}

	const uint32 TracedClosureCount = Substrate::IsStochasticLightingActive(View.GetShaderPlatform()) ? 1u : Substrate::GetSubstrateMaxClosureCount(View);
	const uint32 ResolvedClosureCount = Substrate::GetSubstrateMaxClosureCount(View);

	const bool bUseFarField = LumenReflections::UseFarField(*View.Family);
	const float NearFieldMaxTraceDistance = Lumen::GetMaxTraceDistance(View);
	const bool bTemporal = GLumenReflectionTemporalFilter != 0 && ReflectionsConfig.bDenoising;

	ReflectionTracingParameters.ReflectionTracingViewMin = FIntPoint::DivideAndRoundUp(View.ViewRect.Min, ReflectionTracingParameters.ReflectionDownsampleFactorXY);
	ReflectionTracingParameters.ReflectionTracingViewSize = ViewSize;
	ReflectionTracingParameters.ReflectionTracingBufferSize = BufferSize;
	ReflectionTracingParameters.ReflectionTracingBufferInvSize = FVector2f(1.0f) / BufferSize;
	ReflectionTracingParameters.MaxRayIntensity = CVarLumenReflectionMaxRayIntensity.GetValueOnRenderThread();
	ReflectionTracingParameters.ReflectionSmoothBias = GLumenReflectionSmoothBias;
	ReflectionTracingParameters.ReflectionPass = (uint32)ReflectionPass;
	ReflectionTracingParameters.UseJitter = bTemporal ? 1 : 0;
	ReflectionTracingParameters.UseHighResSurface = CVarLumenReflectionsHiResSurface.GetValueOnRenderThread() != 0 ? 1 : 0;
	ReflectionTracingParameters.MaxReflectionBounces = LumenReflections::GetMaxReflectionBounces(View);
	ReflectionTracingParameters.MaxRefractionBounces = LumenReflections::GetMaxRefractionBounces(View);
	ReflectionTracingParameters.NearFieldMaxTraceDistance = NearFieldMaxTraceDistance;
	ReflectionTracingParameters.FarFieldMaxTraceDistance = bUseFarField ? Lumen::GetFarFieldMaxTraceDistance() : NearFieldMaxTraceDistance;
	ReflectionTracingParameters.NearFieldMaxTraceDistanceDitherScale = Lumen::GetNearFieldMaxTraceDistanceDitherScale(bUseFarField);
	ReflectionTracingParameters.NearFieldSceneRadius = Lumen::GetNearFieldSceneRadius(View, bUseFarField);

	FRDGTextureDesc RayBufferDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, TracedClosureCount));
	ReflectionTracingParameters.RayBuffer = GraphBuilder.CreateTexture(RayBufferDesc, TEXT("Lumen.Reflections.ReflectionRayBuffer"));

	FRDGTextureDesc DownsampledDepthDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, TracedClosureCount));
	ReflectionTracingParameters.DownsampledDepth = GraphBuilder.CreateTexture(DownsampledDepthDesc, TEXT("Lumen.Reflections.ReflectionDownsampledDepth"));

	FRDGTextureDesc DownsampledClosureIndexDesc(FRDGTextureDesc::Create2D(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_R8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ReflectionTracingParameters.DownsampledClosureIndex = Substrate::IsStochasticLightingEnabled(View.GetShaderPlatform()) ? GraphBuilder.CreateTexture(DownsampledClosureIndexDesc, TEXT("Lumen.Reflections.DownsampledClosureIndex")) : nullptr;

	FRDGTextureDesc RayTraceDistanceDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_R16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, TracedClosureCount));
	ReflectionTracingParameters.RayTraceDistance = GraphBuilder.CreateTexture(RayTraceDistanceDesc, TEXT("Lumen.Reflections.RayTraceDistance"));

	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	ReflectionTracingParameters.BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

	FLumenReflectionTileParameters ReflectionTileParameters;

	// Use the external tile list if there is one from Single Layer Water
	if (ReflectionsConfig.TiledReflection
		&& ReflectionsConfig.TiledReflection->DispatchIndirectParametersBuffer
		&& ReflectionsConfig.TiledReflection->TileSize == GReflectionResolveTileSize)
	{
		ReflectionTileParameters.ReflectionClearTileData = ReflectionsConfig.TiledReflection->ClearTileListDataBufferSRV;
		ReflectionTileParameters.ReflectionResolveTileData = ReflectionsConfig.TiledReflection->TileListDataBufferSRV;
		ReflectionTileParameters.ReflectionTracingTileData = ReflectionsConfig.TiledReflection->DownsampledTileListDataBufferSRV;
		ReflectionTileParameters.ClearIndirectArgs = ReflectionsConfig.TiledReflection->DispatchClearIndirectParametersBuffer;
		ReflectionTileParameters.ResolveIndirectArgs = ReflectionsConfig.TiledReflection->DispatchIndirectParametersBuffer;
		ReflectionTileParameters.TracingIndirectArgs = ReflectionsConfig.TiledReflection->DispatchDownsampledIndirectParametersBuffer;
		ReflectionTileParameters.ResolveTileUsed = nullptr;
	}
	else
	{
		ReflectionTileParameters = ReflectionTileClassification(GraphBuilder, View, SceneTextures, ReflectionTracingParameters, ReflectionsConfig.FrontLayerReflectionGBuffer, ComputePassFlags);
	}

	const bool bUseRadianceCache = RadianceCacheParameters.RadianceProbeIndirectionTexture != nullptr && LumenReflections::UseRadianceCache() && ReflectionPass == ELumenReflectionPass::Opaque;

	{
		FReflectionGenerateRaysCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionGenerateRaysCS::FParameters>();
		PassParameters->RWRayBuffer = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.RayBuffer));
		PassParameters->RWDownsampledDepth = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.DownsampledDepth));
		PassParameters->RWDownsampledClosureIndex = Substrate::IsStochasticLightingEnabled(View.GetShaderPlatform()) ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.DownsampledClosureIndex)) : nullptr;
		PassParameters->RWRayTraceDistance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.RayTraceDistance));
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->MaxTraceDistance = Lumen::GetMaxTraceDistance(View);

		PassParameters->RadianceCacheMinRoughness = FMath::Clamp(CVarLumenReflectionsRadianceCacheMinRoughness.GetValueOnRenderThread(), 0.0f, 1.0f);
		PassParameters->RadianceCacheMaxRoughness = FMath::Clamp(CVarLumenReflectionsRadianceCacheMaxRoughness.GetValueOnRenderThread(), PassParameters->RadianceCacheMinRoughness, 1.0f);
		PassParameters->RadianceCacheMaxTraceDistance = FMath::Clamp(CVarLumenReflectionsRadianceCacheMaxTraceDistance.GetValueOnRenderThread(), 0.0f, PassParameters->MaxTraceDistance);
		PassParameters->RadianceCacheMinTraceDistance = FMath::Clamp(CVarLumenReflectionsRadianceCacheMinTraceDistance.GetValueOnRenderThread(), 0.0f, PassParameters->RadianceCacheMaxTraceDistance);
		PassParameters->RadianceCacheRoughnessFadeLength = FMath::Clamp(CVarLumenReflectionsRadianceCacheRoughnessFadeLength.GetValueOnRenderThread(), 0.0f, 1.0f);

		PassParameters->GGXSamplingBias = GLumenReflectionGGXSamplingBias;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->ResolveIndirectArgsForRead = GraphBuilder.CreateSRV(ReflectionTileParameters.TracingIndirectArgs, PF_R32_UINT);
		PassParameters->FrontLayerTranslucencyGBufferParameters = ReflectionsConfig.FrontLayerReflectionGBuffer;
		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;
		PassParameters->ReflectionTileParameters = ReflectionTileParameters;
		PassParameters->RadianceCacheParameters = RadianceCacheParameters;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

		const bool bDebug = GVarLumenReflectionsDebug.GetValueOnRenderThread() != 0;
		if (bDebug)
		{
			ShaderPrint::SetEnabled(true);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
		}

		FReflectionGenerateRaysCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FReflectionGenerateRaysCS::FRadianceCache>(bUseRadianceCache);
		PermutationVector.Set<FReflectionGenerateRaysCS::FFrontLayerTranslucency>(bFrontLayer);
		auto ComputeShader = View.ShaderMap->GetShader<FReflectionGenerateRaysCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GenerateRays MaxRoughnessToTrace:%.2f%s",
				ReflectionTracingParameters.ReflectionsCompositeParameters.MaxRoughnessToTrace,
				bUseRadianceCache ? TEXT(" RadianceCache") : TEXT("")),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ReflectionTileParameters.TracingIndirectArgs,
			0);
	}

	FRDGTextureDesc TraceRadianceDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, TracedClosureCount));
	ReflectionTracingParameters.TraceRadiance = GraphBuilder.CreateTexture(TraceRadianceDesc, TEXT("Lumen.Reflections.TraceRadiance"));
	ReflectionTracingParameters.RWTraceRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.TraceRadiance));

	FRDGTextureDesc TraceHitDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, TracedClosureCount));
	ReflectionTracingParameters.TraceHit = GraphBuilder.CreateTexture(TraceHitDesc, TEXT("Lumen.Reflections.TraceHit"));
	ReflectionTracingParameters.RWTraceHit = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.TraceHit));

	// Hit lighting requires a few optional buffers
	if (LumenReflections::UseHitLighting(View, DiffuseIndirectMethod))
	{
		FRDGTextureDesc TraceMaterialIdDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_R16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, TracedClosureCount));
		ReflectionTracingParameters.TraceMaterialId = GraphBuilder.CreateTexture(TraceMaterialIdDesc, TEXT("Lumen.Reflections.TraceMaterialId"));
		ReflectionTracingParameters.RWTraceMaterialId = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.TraceMaterialId));

		FRDGTextureDesc TraceBookmarkDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_R32G32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, TracedClosureCount));
		ReflectionTracingParameters.TraceBookmark = GraphBuilder.CreateTexture(TraceBookmarkDesc, TEXT("Lumen.Reflections.TraceBookmark"));
		ReflectionTracingParameters.RWTraceBookmark = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.TraceBookmark));
	}

	const bool bTraceMeshObjects = GLumenReflectionTraceMeshSDFs != 0 
		&& Lumen::UseMeshSDFTracing(ViewFamily.EngineShowFlags)
		// HZB is only built to include opaque but is used to cull Mesh SDFs
		&& ReflectionPass == ELumenReflectionPass::Opaque;

	// Query for the bounds of the first person geometry visible in this view. The extension may not be enabled in certain cases (e.g. "Allow Static Lighting" is enabled).
	// Passing zero-sized bounds is valid and implies that there are no first person relevant primitives in the view.
	FBoxSphereBounds FirstPersonWorldSpaceRepresentationBounds = FBoxSphereBounds(ForceInit);
	if (const FFirstPersonSceneExtensionRenderer* FPRenderer = GetSceneExtensionsRenderers().GetRendererPtr<FFirstPersonSceneExtensionRenderer>())
	{
		FirstPersonWorldSpaceRepresentationBounds = FPRenderer->GetFirstPersonViewBounds(View).WorldSpaceRepresentationBounds;
	}

	TraceReflections(
		GraphBuilder, 
		Scene,
		View, 
		FrameTemporaries,
		bTraceMeshObjects,
		SceneTextures,
		ReflectionTracingParameters,
		ReflectionTileParameters,
		MeshSDFGridParameters,
		bUseRadianceCache,
		DiffuseIndirectMethod,
		RadianceCacheParameters,
		FirstPersonWorldSpaceRepresentationBounds,
		ComputePassFlags);
	
	if (VisualizeTracesData)
	{
		GVisualizeReflectionTracesData = GraphBuilder.ConvertToExternalBuffer(VisualizeTracesData);
	}

	const FIntPoint EffectiveTextureResolution = (bFrontLayer || bSingleLayerWater) ? SceneTextures.Config.Extent : Substrate::GetSubstrateTextureResolution(View, SceneTextures.Config.Extent);
	const FIntPoint EffectiveViewExtent = FrameTemporaries.ViewExtent;

	FRDGTextureRef ResolvedSpecularIndirect = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2DArray(EffectiveTextureResolution, PF_FloatRGB, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_UAV, ResolvedClosureCount),
			bFrontLayer ? TEXT("Lumen.Reflections.FrontLayer.ResolvedSpecularIndirect") : TEXT("Lumen.Reflections.ResolvedSpecularIndirect"));

	FRDGTextureRef ResolvedSpecularIndirectDepth = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2DArray(EffectiveTextureResolution, PF_R16F, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_UAV, ResolvedClosureCount),
			bFrontLayer ? TEXT("Lumen.Reflections.FrontLayer.ResolvedSpecularIndirectDepth") : TEXT("Lumen.Reflections.ResolvedSpecularIndirectDepth"));

	const int32 NumReconstructionSamples = FMath::Clamp(FMath::RoundToInt(View.FinalPostProcessSettings.LumenReflectionQuality * GLumenReflectionScreenSpaceReconstructionNumSamples), GLumenReflectionScreenSpaceReconstructionNumSamples, 64);
	const bool bUseSpatialReconstruction = GLumenReflectionScreenSpaceReconstruction != 0 && ReflectionsConfig.bScreenSpaceReconstruction;

	FRDGTextureRef SpecularAndSecondMoment = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2DArray(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ResolvedClosureCount),
		bFrontLayer ? TEXT("Lumen.Reflections.FrontLayer.SpecularAndSecondMoment") : TEXT("Lumen.Reflections.SpecularAndSecondMoment"));

	FRDGTextureUAVRef ResolvedSpecularUAV = GraphBuilder.CreateUAV(ResolvedSpecularIndirect, ERDGUnorderedAccessViewFlags::SkipBarrier);

	// Clear tiles which won't be processed
	auto ReflectionDenoiserClear = [&](uint32 ClosureIndex)
	{
		FLumenReflectionDenoiserClearCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenReflectionDenoiserClearCS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = nullptr;
		PassParameters->ReflectionTileParameters = ReflectionTileParameters;
		PassParameters->RWResolvedSpecular = ResolvedSpecularUAV;
		PassParameters->RWSpecularAndSecondMoment = GraphBuilder.CreateUAV(SpecularAndSecondMoment);
		PassParameters->RWFinalRadiance = nullptr;
		PassParameters->RWBackgroundVisibility = nullptr;
		PassParameters->bClearToSceneColor = 0;
		PassParameters->ClosureIndex = ClosureIndex;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

		FLumenReflectionDenoiserClearCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenReflectionDenoiserClearCS::FClearFinalRadianceAndBackgroundVisibility>(false);

		auto ComputeShader = View.ShaderMap->GetShader<FLumenReflectionDenoiserClearCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClearEmptyTiles"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ReflectionTileParameters.ClearIndirectArgs,
			0);
	};

	if (bTemporal)
	{
		const uint32 ClearClosureCount = Substrate::IsStochasticLightingActive(View.GetShaderPlatform()) ? ResolvedClosureCount : 1;
		for (uint32 ClosureIndex =0;ClosureIndex<ClearClosureCount; ++ClosureIndex)
		{
			ReflectionDenoiserClear(ClosureIndex);
		}
	}

	// Clear neighboring tile
	const float SpatialReconstructionKernelRadius = CVarLumenReflectionScreenSpaceReconstructionKernelRadius.GetValueOnRenderThread();
	const bool bClearNeighborTiles = Substrate::IsSubstrateEnabled() && ResolvedClosureCount > 1 && ReflectionTileParameters.ResolveTileUsed;
	if (bClearNeighborTiles)
	{
		const uint32 KernelRadiusInPixels = FMath::CeilToInt(LumenReflectionDownsampleFactorXY.X * SpatialReconstructionKernelRadius);
		const uint32 KernelRadiusInTiles = FMath::DivideAndRoundUp(KernelRadiusInPixels, uint32(GReflectionResolveTileSize));

		FReflectionClearNeighborTileCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionClearNeighborTileCS::FParameters>();
		//PassParameters->RWResolvedSpecularIndirect = //GraphBuilder.CreateUAV(ResolvedSpecularIndirect);  TO BE DONE
		PassParameters->RWSpecularAndSecondMoment = GraphBuilder.CreateUAV(SpecularAndSecondMoment);
		PassParameters->RWSpecularIndirect = ResolvedSpecularUAV;
		PassParameters->ResolveTileUsed = ReflectionTileParameters.ResolveTileUsed;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->TileViewportDimensions = ReflectionTileParameters.ResolveTileUsed->Desc.Extent;
		PassParameters->ResolveTileViewportDimensions = ReflectionTileParameters.ResolveTileUsed->Desc.Extent;
		PassParameters->KernelRadiusInTiles = KernelRadiusInTiles;

		FReflectionClearNeighborTileCS::FPermutationDomain PermutationVector;
		auto ComputeShader = View.ShaderMap->GetShader<FReflectionClearNeighborTileCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClearNeighborTile"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(PassParameters->ResolveTileViewportDimensions.X, PassParameters->ResolveTileViewportDimensions.Y, ResolvedClosureCount-1));
	}

	// #lumen_todo: use tile classification instead
	const bool bUseAnisotropy = HasAnyDraw(View.ParallelMeshDrawCommandPasses[EMeshPass::AnisotropyPass]) || Substrate::IsSubstrateEnabled();

	// Resolve reflections
	auto ResolveReflections = [&](uint32 ClosureIndex)
	{
		FLumenReflectionResolveCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenReflectionResolveCS::FParameters>();
		PassParameters->RWSpecularIndirect = ResolvedSpecularUAV;
		PassParameters->RWSpecularIndirectDepth = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ResolvedSpecularIndirectDepth));
		PassParameters->RWBackgroundVisibility = nullptr;
		PassParameters->TraceBackgroundVisibility = nullptr;
		PassParameters->NumSpatialReconstructionSamples = NumReconstructionSamples;
		PassParameters->SpatialReconstructionKernelRadius = SpatialReconstructionKernelRadius;
		PassParameters->SpatialReconstructionRoughnessScale = GLumenReflectionScreenSpaceReconstructionRoughnessScale;
		PassParameters->SpatialReconstructionMinWeight = FMath::Max(CVarLumenReflectionScreenSpaceReconstructionMinWeight.GetValueOnRenderThread(), 0.0f);
		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->FrontLayerTranslucencyGBufferParameters = ReflectionsConfig.FrontLayerReflectionGBuffer;
		PassParameters->ReflectionTileParameters = ReflectionTileParameters;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->InvSubstrateMaxClosureCount = 1.0f / ResolvedClosureCount;
		PassParameters->ClosureIndex = ClosureIndex;
		PassParameters->ReflectionsDenoiserOneOverTonemapRange = LumenReflections::GetDenoiserOneOverTonemapRange();

		const bool bDebug = GVarLumenReflectionsDebug.GetValueOnRenderThread() != 0;
		if (bDebug)
		{
			ShaderPrint::SetEnabled(true);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
		}

		FLumenReflectionResolveCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenReflectionResolveCS::FSpatialReconstruction>(bUseSpatialReconstruction);
		PermutationVector.Set<FLumenReflectionResolveCS::FFrontLayerTranslucency>(bFrontLayer);
		PermutationVector.Set<FLumenReflectionResolveCS::FResolveBackgroundVisibility>(false);
		PermutationVector.Set<FLumenReflectionResolveCS::FDownsampleFactorX>(LumenReflectionDownsampleFactorXY.X);
		PermutationVector.Set<FLumenReflectionResolveCS::FDownsampleFactorY>(LumenReflectionDownsampleFactorXY.Y);
		PermutationVector.Set<FLumenReflectionResolveCS::FUseAnisotropy>(bUseAnisotropy);
		PermutationVector.Set<FLumenReflectionResolveCS::FDebugMode>(bDebug);
		PermutationVector = FLumenReflectionResolveCS::RemapPermutation(PermutationVector);
		auto ComputeShader = View.ShaderMap->GetShader<FLumenReflectionResolveCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ReflectionsResolve DonwsampleFactor:%dx%d SpatialReconstruction:%d Aniso:%d",
				LumenReflectionDownsampleFactorXY.X,
				LumenReflectionDownsampleFactorXY.Y,
				bUseSpatialReconstruction ? 1 : 0,
				bUseAnisotropy),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ReflectionTileParameters.ResolveIndirectArgs,
			0);
	};

	{
		const uint32 ResolveReflectionClosureCount = Substrate::IsStochasticLightingActive(View.GetShaderPlatform()) ? ResolvedClosureCount : 1;
		for (uint32 ClosureIndex =0;ClosureIndex<ResolveReflectionClosureCount; ++ClosureIndex)
		{
			ResolveReflections(ClosureIndex);
		}
	}

	FRDGTextureRef SpecularIndirect = ResolvedSpecularIndirect;

	if (bTemporal)
	{
		FLumenReflectionDenoiserParameters DenoiserParameters;
		DenoiserParameters.ViewUniformBuffer = View.ViewUniformBuffer;
		DenoiserParameters.SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
		DenoiserParameters.SceneTexturesStruct = SceneTextures.UniformBuffer;
		DenoiserParameters.FrontLayerTranslucencyGBufferParameters = ReflectionsConfig.FrontLayerReflectionGBuffer;
		DenoiserParameters.Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		DenoiserParameters.ReflectionTileParameters = ReflectionTileParameters;
		DenoiserParameters.ReflectionTracingParameters = ReflectionTracingParameters;
		DenoiserParameters.InvSubstrateMaxClosureCount = 1.0f / ResolvedClosureCount;

		const bool bDebug = GVarLumenReflectionsDebug.GetValueOnRenderThread() != 0;
		if (bDebug)
		{
			ShaderPrint::SetEnabled(true);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, DenoiserParameters.ShaderPrintUniformBuffer);
		}

		const bool bSpatial = GLumenReflectionBilateralFilter != 0 && ReflectionsConfig.bDenoising;
		FVector4f HistoryScreenPositionScaleBias = FVector4f(1.0f, 1.0f, 0.0f, 0.0f);
		FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FVector4f HistoryGatherUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FVector4f HistoryBufferSizeAndInvSize = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FRDGTextureRef SpecularAndSecondMomentHistory = nullptr;
		FRDGTextureRef NumFramesAccumulatedHistory = nullptr;

		FReflectionTemporalState* ReflectionState = nullptr;
		if (View.ViewState)
		{
			if (ReflectionPass == ELumenReflectionPass::SingleLayerWater)
			{
				ReflectionState = &View.ViewState->Lumen.WaterReflectionState;
			}
			else if (ReflectionPass == ELumenReflectionPass::FrontLayerTranslucency)
			{
				ReflectionState = &View.ViewState->Lumen.TranslucentReflectionState;
			}
			else
			{
				ReflectionState = &View.ViewState->Lumen.ReflectionState;
			}
		}

		if (ReflectionState 
			&& !View.bCameraCut
			&& !View.bPrevTransformsReset
			&& bTemporal)
		{
			HistoryScreenPositionScaleBias = ReflectionState->HistoryScreenPositionScaleBias;
			HistoryUVMinMax = ReflectionState->HistoryUVMinMax;
			HistoryGatherUVMinMax = ReflectionState->HistoryGatherUVMinMax;
			HistoryBufferSizeAndInvSize = ReflectionState->HistoryBufferSizeAndInvSize;

			if (ReflectionState->SpecularAndSecondMomentHistory
				&& ReflectionState->NumFramesAccumulatedHistory)
			{
				SpecularAndSecondMomentHistory = GraphBuilder.RegisterExternalTexture(ReflectionState->SpecularAndSecondMomentHistory);
				NumFramesAccumulatedHistory = GraphBuilder.RegisterExternalTexture(ReflectionState->NumFramesAccumulatedHistory);
			}
		}

		FRDGTextureRef SceneDepthHistory = View.ViewState->StochasticLighting.SceneDepthHistory ? GraphBuilder.RegisterExternalTexture(View.ViewState->StochasticLighting.SceneDepthHistory) : nullptr;

		FRDGTextureRef NumFramesAccumulated = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2DArray(View.GetSceneTexturesConfig().Extent, PF_G8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ResolvedClosureCount),
			bFrontLayer ? TEXT("Lumen.Reflections.FrontLayer.NumFramesAccumulated") : TEXT("Lumen.Reflections.NumFramesAccumulated"));

		// Temporal accumulation
		auto TemporalAccumulation = [&](uint32 ClosureIndex)
		{
			const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
			FRDGTextureRef VelocityTexture = GetIfProduced(SceneTextures.Velocity, SystemTextures.Black);

			FLumenReflectionDenoiserTemporalCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenReflectionDenoiserTemporalCS::FParameters>();
			PassParameters->DenoiserParameters = DenoiserParameters;
			PassParameters->ResolvedSpecularLighting = ResolvedSpecularIndirect;
			PassParameters->ResolvedReflectionsDepth = ResolvedSpecularIndirectDepth;
			PassParameters->SpecularHistoryTexture = SpecularAndSecondMomentHistory;
			PassParameters->NumFramesAccumulatedHistoryTexture = NumFramesAccumulatedHistory;
			PassParameters->VelocityTexture = VelocityTexture;
			PassParameters->SceneDepthHistory = SceneDepthHistory;
			PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
			PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
			PassParameters->HistoryUVMinMax = HistoryUVMinMax;
			PassParameters->HistoryGatherUVMinMax = HistoryGatherUVMinMax;
			PassParameters->HistoryBufferSizeAndInvSize = HistoryBufferSizeAndInvSize;
			PassParameters->RWSpecularAndSecondMoment = GraphBuilder.CreateUAV(SpecularAndSecondMoment);
			PassParameters->RWNumFramesAccumulated = GraphBuilder.CreateUAV(NumFramesAccumulated);

			PassParameters->TemporalMaxFramesAccumulated = LumenReflections::GetMaxFramesAccumulated();
			PassParameters->TemporalNeighborhoodClampScale = CVarLumenReflectionsTemporalNeighborhoodClampScale.GetValueOnRenderThread();
			PassParameters->HistoryDistanceThreshold = GLumenReflectionHistoryDistanceThreshold;
			PassParameters->ClosureIndex = ClosureIndex;
			PassParameters->ReflectionsDenoiserOneOverTonemapRange = LumenReflections::GetDenoiserOneOverTonemapRange();

			FLumenReflectionDenoiserTemporalCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenReflectionDenoiserTemporalCS::FValidHistory>(SceneDepthHistory != nullptr && SpecularAndSecondMomentHistory != nullptr && bTemporal);
			PermutationVector.Set<FLumenReflectionDenoiserTemporalCS::FRayTracedTranslucencyLighting>(false);
			PermutationVector.Set<FLumenReflectionDenoiserTemporalCS::FDebug>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FLumenReflectionDenoiserTemporalCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TemporalAccumulation"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				ReflectionTileParameters.ResolveIndirectArgs,
				0);
		};

		{
			const uint32 TemporalAccumulationClosureCount = Substrate::IsStochasticLightingActive(View.GetShaderPlatform()) ? ResolvedClosureCount : 1;
			for (uint32 ClosureIndex =0;ClosureIndex<TemporalAccumulationClosureCount; ++ClosureIndex)
			{
				TemporalAccumulation(ClosureIndex);
			}
		}

		// Final reflection output
		SpecularIndirect = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2DArray(EffectiveTextureResolution, PF_FloatRGB, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable, ResolvedClosureCount),
			bFrontLayer ? TEXT("Lumen.Reflections.FrontLayer.SpecularIndirect") : TEXT("Lumen.Reflections.SpecularIndirect"));

		// Spatial filter
		auto SpatialFilter = [&](uint32 ClosureIndex)
		{
			FLumenReflectionDenoiserSpatialCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenReflectionDenoiserSpatialCS::FParameters>();
			PassParameters->DenoiserParameters = DenoiserParameters;
			PassParameters->RWSpecularIndirectAccumulated = GraphBuilder.CreateUAV(SpecularIndirect);
			PassParameters->RWTranslucencyLighting = nullptr;
			PassParameters->SpecularLightingAndSecondMomentTexture = SpecularAndSecondMoment;
			PassParameters->BackgroundVisibilityTexture = nullptr;
			PassParameters->NumFramesAccumulatedTexture = NumFramesAccumulated;
			PassParameters->SpatialFilterDepthWeightScale = GLumenReflectionBilateralFilterDepthWeightScale;
			PassParameters->SpatialFilterKernelRadius = CVarLumenReflectionBilateralFilterKernelRadius.GetValueOnRenderThread();
			PassParameters->SpatialFilterNumSamples = FMath::Clamp(GLumenReflectionBilateralFilterNumSamples, 0, 1024);
			PassParameters->TemporalMaxFramesAccumulated = LumenReflections::GetMaxFramesAccumulated();
			PassParameters->bCompositeSceneColor = 0;
			PassParameters->ClosureIndex = ClosureIndex;
			PassParameters->ReflectionsDenoiserOneOverTonemapRange = LumenReflections::GetDenoiserOneOverTonemapRange();

			FLumenReflectionDenoiserSpatialCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenReflectionDenoiserSpatialCS::FFrontLayerTranslucency>(bFrontLayer);
			PermutationVector.Set<FLumenReflectionDenoiserSpatialCS::FRayTracedTranslucency>(false);
			PermutationVector.Set<FLumenReflectionDenoiserSpatialCS::FSpatialFilter>(bSpatial);
			PermutationVector.Set<FLumenReflectionDenoiserSpatialCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FLumenReflectionDenoiserSpatialCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Spatial"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				ReflectionTileParameters.ResolveIndirectArgs,
				0);
		};

		{
			const uint32 SpatialFilterClosureCount = Substrate::IsStochasticLightingActive(View.GetShaderPlatform()) ? ResolvedClosureCount : 1;
			for (uint32 ClosureIndex = 0; ClosureIndex < SpatialFilterClosureCount; ++ClosureIndex)
			{
				SpatialFilter(ClosureIndex);
			}
		}

		if (ReflectionState && !View.bStatePrevViewInfoIsReadOnly)
		{
			ReflectionState->HistoryFrameIndex = View.ViewState->PendingPrevFrameNumber;
			ReflectionState->HistoryViewRect = View.ViewRect;
			ReflectionState->HistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(View.GetSceneTexturesConfig().Extent, View.ViewRect);

			const FVector2D InvBufferSize(1.0f / SceneTextures.Config.Extent.X, 1.0f / SceneTextures.Config.Extent.Y);

			ReflectionState->HistoryUVMinMax = FVector4f(
				View.ViewRect.Min.X * InvBufferSize.X,
				View.ViewRect.Min.Y * InvBufferSize.Y,
				View.ViewRect.Max.X * InvBufferSize.X,
				View.ViewRect.Max.Y * InvBufferSize.Y);

			// Clamp gather4 to a valid bilinear footprint in order to avoid sampling outside of valid bounds
			ReflectionState->HistoryGatherUVMinMax = FVector4f(
				(View.ViewRect.Min.X + 0.51f) * InvBufferSize.X,
				(View.ViewRect.Min.Y + 0.51f) * InvBufferSize.Y,
				(View.ViewRect.Max.X - 0.51f) * InvBufferSize.X,
				(View.ViewRect.Max.Y - 0.51f) * InvBufferSize.Y);

			ReflectionState->HistoryBufferSizeAndInvSize = FVector4f(
				SceneTextures.Config.Extent.X,
				SceneTextures.Config.Extent.Y,
				1.0f / SceneTextures.Config.Extent.X,
				1.0f / SceneTextures.Config.Extent.Y);

			if (SpecularAndSecondMoment && NumFramesAccumulated && bTemporal)
			{
				GraphBuilder.QueueTextureExtraction(SpecularAndSecondMoment, &ReflectionState->SpecularAndSecondMomentHistory);
				GraphBuilder.QueueTextureExtraction(NumFramesAccumulated, &ReflectionState->NumFramesAccumulatedHistory);
			}
			else
			{
				ReflectionState->SpecularAndSecondMomentHistory = nullptr;
				ReflectionState->NumFramesAccumulatedHistory = nullptr;
			}
		}
	}

	return SpecularIndirect;
}

DECLARE_GPU_STAT(RayTracedTranslucency);

void FDeferredShadingSceneRenderer::RenderRayTracedTranslucencyView(
	FRDGBuilder& GraphBuilder,
	FViewInfo& View,
	FSceneTextures& SceneTextures,
	FLumenSceneFrameTemporaries& FrameTemporaries,
	const FFrontLayerTranslucencyData& FrontLayerTranslucencyData,
	FRDGTextureRef& InOutFinalRadiance,
	FRDGTextureRef& InOutBackgroundVisibility)
{
	if (!View.bTranslucentSurfaceLighting || !RayTracedTranslucency::IsEnabled(View))
	{
		return;
	}

	check(FrontLayerTranslucencyData.IsValid() && FrontLayerTranslucencyData.SceneDepth->Desc.Extent == SceneTextures.Config.Extent);

	const bool bUseRayTracedRefractions = RayTracedTranslucency::UseRayTracedRefraction(Views);
	const ERDGPassFlags ComputePassFlags = ERDGPassFlags::Compute;
	const bool bDenoise = true;
	const bool bCompositeBackToSceneColor = bUseRayTracedRefractions
		|| !ViewFamily.AllowStandardTranslucencySeparated()
		|| !ShouldRenderDistortion();

	FLumenReflectionTracingParameters ReflectionTracingParameters;
	{
		LumenReflections::SetupCompositeParameters(View, ReflectionTracingParameters.ReflectionsCompositeParameters);
		ReflectionTracingParameters.PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRHI();
		ReflectionTracingParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		uint32 StateFrameIndex = View.ViewState ? View.ViewState->GetFrameIndex() : 0;
		if (GVarLumenReflectionsFixedStateFrameIndex.GetValueOnRenderThread() >= 0)
		{
			StateFrameIndex = GVarLumenReflectionsFixedStateFrameIndex.GetValueOnRenderThread();
		}

		ReflectionTracingParameters.ReflectionsStateFrameIndex = StateFrameIndex;
		ReflectionTracingParameters.ReflectionsStateFrameIndexMod8 = StateFrameIndex % 8;
		ReflectionTracingParameters.ReflectionsRayDirectionFrameIndex = StateFrameIndex % FMath::Max(CVarLumenReflectionTemporalMaxRayDirections.GetValueOnRenderThread(), 1);
	}

	FRDGBufferRef VisualizeTracesData = nullptr;

	// TODO: Visualization
	//VisualizeTracesData = SetupVisualizeReflectionTraces(GraphBuilder, ReflectionTracingParameters.VisualizeTracesParameters);

	// TODO: Look into possibility of downsampling
	// Compute effective downsampling factor. 
	const int32 UserDownsampleFactor = View.FinalPostProcessSettings.LumenReflectionQuality <= .25f ? 2 : 1;
	const uint32 DownsampleFactor = bDenoise ? RayTracedTranslucency::GetDownsampleFactor(Views) : 1u;
	ReflectionTracingParameters.ReflectionDownsampleFactorXY = DownsampleFactor;
	const FIntPoint ViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), ReflectionTracingParameters.ReflectionDownsampleFactorXY);
	FIntPoint BufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, ReflectionTracingParameters.ReflectionDownsampleFactorXY);
	const uint32 ClosureCount = 1;

	const bool bUseFarField = LumenReflections::UseFarField(*View.Family);
	const float NearFieldMaxTraceDistance = Lumen::GetMaxTraceDistance(View);

	ReflectionTracingParameters.ReflectionTracingViewMin = FIntPoint::DivideAndRoundUp(View.ViewRect.Min, ReflectionTracingParameters.ReflectionDownsampleFactorXY);
	ReflectionTracingParameters.ReflectionTracingViewSize = ViewSize;
	ReflectionTracingParameters.ReflectionTracingBufferSize = BufferSize;
	ReflectionTracingParameters.ReflectionTracingBufferInvSize = FVector2f(1.0f) / BufferSize;
	ReflectionTracingParameters.MaxRayIntensity = CVarRayTracedTranslucencyMaxRayIntensity.GetValueOnRenderThread();
	ReflectionTracingParameters.ReflectionSmoothBias = GLumenReflectionSmoothBias;
	ReflectionTracingParameters.ReflectionPass = (uint32)ELumenReflectionPass::FrontLayerTranslucency;
	ReflectionTracingParameters.UseJitter = bDenoise && GLumenReflectionTemporalFilter ? 1 : 0;
	ReflectionTracingParameters.UseHighResSurface = CVarLumenReflectionsHiResSurface.GetValueOnRenderThread() != 0 ? 1 : 0;
	ReflectionTracingParameters.MaxReflectionBounces = LumenReflections::GetMaxReflectionBounces(View);
	ReflectionTracingParameters.MaxRefractionBounces = LumenReflections::GetMaxRefractionBounces(View);
	ReflectionTracingParameters.NearFieldMaxTraceDistance = NearFieldMaxTraceDistance;
	ReflectionTracingParameters.FarFieldMaxTraceDistance = bUseFarField ? Lumen::GetFarFieldMaxTraceDistance() : NearFieldMaxTraceDistance;
	ReflectionTracingParameters.NearFieldMaxTraceDistanceDitherScale = Lumen::GetNearFieldMaxTraceDistanceDitherScale(bUseFarField);
	ReflectionTracingParameters.NearFieldSceneRadius = Lumen::GetNearFieldSceneRadius(View, bUseFarField);
	ReflectionTracingParameters.DownsampledClosureIndex = nullptr; // Not use with front layer

	FRDGTextureDesc RayBufferDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount));
	ReflectionTracingParameters.RayBuffer = GraphBuilder.CreateTexture(RayBufferDesc, TEXT("Lumen.RTTranslucency.ReflectionRayBuffer"));

	FRDGTextureDesc DownsampledDepthDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount));
	ReflectionTracingParameters.DownsampledDepth = GraphBuilder.CreateTexture(DownsampledDepthDesc, TEXT("Lumen.RTTranslucency.DownsampledDepth"));

	FRDGTextureDesc RayTraceDistanceDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_R16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount));
	ReflectionTracingParameters.RayTraceDistance = GraphBuilder.CreateTexture(RayTraceDistanceDesc, TEXT("Lumen.RTTranslucency.RayTraceDistance"));

	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	ReflectionTracingParameters.BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

	LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheParameters;

	FLumenFrontLayerTranslucencyGBufferParameters FrontLayerReflectionGBuffer;
	FrontLayerReflectionGBuffer.FrontLayerTranslucencyNormal = FrontLayerTranslucencyData.Normal;
	FrontLayerReflectionGBuffer.FrontLayerTranslucencySceneDepth = FrontLayerTranslucencyData.SceneDepth;

	const FLumenReflectionTileParameters ReflectionTileParameters =
		ReflectionTileClassification(GraphBuilder, View, SceneTextures, ReflectionTracingParameters, FrontLayerReflectionGBuffer, ComputePassFlags);

	{
		FReflectionGenerateRaysCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionGenerateRaysCS::FParameters>();
		PassParameters->RWRayBuffer = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.RayBuffer));
		PassParameters->RWDownsampledDepth = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.DownsampledDepth));
		PassParameters->RWDownsampledClosureIndex = ReflectionTracingParameters.DownsampledClosureIndex ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.DownsampledClosureIndex)) : nullptr;
		PassParameters->RWRayTraceDistance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.RayTraceDistance));
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
		PassParameters->GGXSamplingBias = GLumenReflectionGGXSamplingBias;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->ResolveIndirectArgsForRead = GraphBuilder.CreateSRV(ReflectionTileParameters.TracingIndirectArgs, PF_R32_UINT);
		PassParameters->FrontLayerTranslucencyGBufferParameters = FrontLayerReflectionGBuffer;
		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;
		PassParameters->ReflectionTileParameters = ReflectionTileParameters;
		PassParameters->RadianceCacheParameters = RadianceCacheParameters;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

		FReflectionGenerateRaysCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FReflectionGenerateRaysCS::FRadianceCache>(false);
		PermutationVector.Set<FReflectionGenerateRaysCS::FFrontLayerTranslucency>(true);
		auto ComputeShader = View.ShaderMap->GetShader<FReflectionGenerateRaysCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GenerateRays MaxRoughnessToTrace:%.2f", ReflectionTracingParameters.ReflectionsCompositeParameters.MaxRoughnessToTrace),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ReflectionTileParameters.TracingIndirectArgs,
			0);
	}

	FRDGTextureDesc TraceRadianceDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_FloatR11G11B10, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount));
	ReflectionTracingParameters.TraceRadiance = GraphBuilder.CreateTexture(TraceRadianceDesc, TEXT("Lumen.RTTranslucency.TraceRadiance"));
	ReflectionTracingParameters.RWTraceRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.TraceRadiance));

	FRDGTextureDesc TraceBackgroundVisibilityDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_FloatR11G11B10, FClearValueBinding::White, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount));
	FRDGTextureRef TraceBackgroundVisibilityTexture = GraphBuilder.CreateTexture(TraceBackgroundVisibilityDesc, TEXT("Lumen.RTTranslucency.TraceBackgroundVisibility"));
	ReflectionTracingParameters.RWTraceBackgroundVisibility = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TraceBackgroundVisibilityTexture));

	FRDGTextureDesc TraceHitDesc(FRDGTextureDesc::Create2DArray(ReflectionTracingParameters.ReflectionTracingBufferSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount));
	ReflectionTracingParameters.TraceHit = GraphBuilder.CreateTexture(TraceHitDesc, TEXT("Lumen.RTTranslucency.TraceHit"));
	ReflectionTracingParameters.RWTraceHit = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReflectionTracingParameters.TraceHit));

	TraceTranslucency(
		GraphBuilder,
		Scene,
		View,
		FrameTemporaries,
		SceneTextures,
		ReflectionTracingParameters,
		ReflectionTileParameters,
		GetViewPipelineState(View).DiffuseIndirectMethod,
		ComputePassFlags,
		bUseRayTracedRefractions);

	if (VisualizeTracesData)
	{
		GVisualizeReflectionTracesData = GraphBuilder.ConvertToExternalBuffer(VisualizeTracesData);
	}

	const FIntPoint EffectiveTextureResolution = SceneTextures.Config.Extent;
	const FIntPoint EffectiveViewExtent = FrameTemporaries.ViewExtent;

	FRDGTextureRef ResolvedSpecularIndirect = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2DArray(EffectiveTextureResolution, PF_FloatR11G11B10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount),
		TEXT("Lumen.RTTranslucency.ResolvedSpecularIndirect"));

	FRDGTextureRef ResolvedSpecularIndirectDepth = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2DArray(EffectiveTextureResolution, PF_R16F, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount),
		TEXT("Lumen.RTTranslucency.ResolvedSpecularIndirectDepth"));

	const int32 NumReconstructionSamples = FMath::Clamp(FMath::RoundToInt(View.FinalPostProcessSettings.LumenReflectionQuality * GLumenReflectionScreenSpaceReconstructionNumSamples), GLumenReflectionScreenSpaceReconstructionNumSamples, 64);
	const bool bUseSpatialReconstruction = bDenoise && GLumenReflectionScreenSpaceReconstruction != 0;
	const bool bUseBilaterialFilter = bDenoise && GLumenReflectionBilateralFilter != 0;

	FRDGTextureRef SpecularAndSecondMoment = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2DArray(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount),
		TEXT("Lumen.RTTranslucency.SpecularAndSecondMoment"));

	if (!InOutFinalRadiance)
	{
		FRDGTextureDesc FinalRandianceDesc = SceneTextures.Color.Target->Desc;
		FinalRandianceDesc.Flags |= TexCreate_UAV;
		InOutFinalRadiance = GraphBuilder.CreateTexture(FinalRandianceDesc, TEXT("Lumen.RTTranslucency.FinalRadiance"));
	}

	if (!InOutBackgroundVisibility)
	{
		InOutBackgroundVisibility = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(EffectiveTextureResolution, PF_FloatR11G11B10, FClearValueBinding::White, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("Lumen.RTTranslucency.BackgroundVisibility"));
	}

	FRDGTextureUAVRef ResolvedSpecularUAV = GraphBuilder.CreateUAV(ResolvedSpecularIndirect, ERDGUnorderedAccessViewFlags::SkipBarrier);
	FRDGTextureUAVRef FinalRadianceUAV = GraphBuilder.CreateUAV(InOutFinalRadiance, ERDGUnorderedAccessViewFlags::SkipBarrier);
	FRDGTextureUAVRef BackgroundVisibilityUAV = GraphBuilder.CreateUAV(InOutBackgroundVisibility, ERDGUnorderedAccessViewFlags::SkipBarrier);

	// Clear tiles which won't be processed
	if (bDenoise)
	{
		FLumenReflectionDenoiserClearCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenReflectionDenoiserClearCS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->ReflectionTileParameters = ReflectionTileParameters;
		PassParameters->RWResolvedSpecular = ResolvedSpecularUAV;
		PassParameters->RWSpecularAndSecondMoment = GraphBuilder.CreateUAV(SpecularAndSecondMoment);
		PassParameters->RWFinalRadiance = FinalRadianceUAV;
		PassParameters->RWBackgroundVisibility = BackgroundVisibilityUAV;
		PassParameters->bClearToSceneColor = bCompositeBackToSceneColor;
		PassParameters->ClosureIndex = 0;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

		FLumenReflectionDenoiserClearCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenReflectionDenoiserClearCS::FClearFinalRadianceAndBackgroundVisibility>(true);

		auto ComputeShader = View.ShaderMap->GetShader<FLumenReflectionDenoiserClearCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClearEmptyTiles"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ReflectionTileParameters.ClearIndirectArgs,
			0);
	}

	// #lumen_todo: use tile classification instead
	const bool bUseAnisotropy = HasAnyDraw(View.ParallelMeshDrawCommandPasses[EMeshPass::AnisotropyPass]) || Substrate::IsSubstrateEnabled();

	// Resolve translucency
	{
		FLumenReflectionResolveCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenReflectionResolveCS::FParameters>();
		PassParameters->RWSpecularIndirect = ResolvedSpecularUAV;
		PassParameters->RWSpecularIndirectDepth = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ResolvedSpecularIndirectDepth));
		PassParameters->RWBackgroundVisibility = BackgroundVisibilityUAV;
		PassParameters->TraceBackgroundVisibility = TraceBackgroundVisibilityTexture;
		PassParameters->NumSpatialReconstructionSamples = NumReconstructionSamples;
		PassParameters->SpatialReconstructionKernelRadius = CVarLumenReflectionScreenSpaceReconstructionKernelRadius.GetValueOnRenderThread();
		PassParameters->SpatialReconstructionRoughnessScale = GLumenReflectionScreenSpaceReconstructionRoughnessScale;
		PassParameters->SpatialReconstructionMinWeight = FMath::Max(CVarLumenReflectionScreenSpaceReconstructionMinWeight.GetValueOnRenderThread(), 0.0f);
		PassParameters->ReflectionsDenoiserOneOverTonemapRange = LumenReflections::GetDenoiserOneOverTonemapRange();
		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->FrontLayerTranslucencyGBufferParameters = FrontLayerReflectionGBuffer;
		PassParameters->ReflectionTileParameters = ReflectionTileParameters;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->InvSubstrateMaxClosureCount = 1.0f / ClosureCount;
		PassParameters->ClosureIndex = 0;

		const bool bDebug = CVarRayTracedTranslucencyDebug.GetValueOnRenderThread() != 0;
		if (bDebug)
		{
			ShaderPrint::SetEnabled(true);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
		}

		FLumenReflectionResolveCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenReflectionResolveCS::FSpatialReconstruction>(bUseSpatialReconstruction);
		PermutationVector.Set<FLumenReflectionResolveCS::FFrontLayerTranslucency>(true);
		PermutationVector.Set<FLumenReflectionResolveCS::FResolveBackgroundVisibility>(true);
		PermutationVector.Set<FLumenReflectionResolveCS::FDownsampleFactorX>(DownsampleFactor);
		PermutationVector.Set<FLumenReflectionResolveCS::FDownsampleFactorY>(DownsampleFactor);
		PermutationVector.Set<FLumenReflectionResolveCS::FUseAnisotropy>(bUseAnisotropy);
		PermutationVector.Set<FLumenReflectionResolveCS::FDebugMode>(bDebug);
		PermutationVector = FLumenReflectionResolveCS::RemapPermutation(PermutationVector);
		auto ComputeShader = View.ShaderMap->GetShader<FLumenReflectionResolveCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ReflectionsResolve DonwsampleFactor:%d Aniso:%d", DownsampleFactor, bUseAnisotropy),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ReflectionTileParameters.ResolveIndirectArgs,
			0);
	}

	if (bDenoise)
	{
		FLumenReflectionDenoiserParameters DenoiserParameters;
		DenoiserParameters.ViewUniformBuffer = View.ViewUniformBuffer;
		DenoiserParameters.SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
		DenoiserParameters.SceneTexturesStruct = SceneTextures.UniformBuffer;
		DenoiserParameters.FrontLayerTranslucencyGBufferParameters = FrontLayerReflectionGBuffer;
		DenoiserParameters.Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		DenoiserParameters.ReflectionTileParameters = ReflectionTileParameters;
		DenoiserParameters.ReflectionTracingParameters = ReflectionTracingParameters;
		DenoiserParameters.InvSubstrateMaxClosureCount = 1.0f / ClosureCount;

		const bool bDebug = CVarRayTracedTranslucencyDebug.GetValueOnRenderThread() != 0;
		if (bDebug)
		{
			ShaderPrint::SetEnabled(true);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, DenoiserParameters.ShaderPrintUniformBuffer);
		}

		bool bTemporal = GLumenReflectionTemporalFilter != 0;
		bool bSpatial = GLumenReflectionBilateralFilter != 0;
		FVector4f HistoryScreenPositionScaleBias = FVector4f(1.0f, 1.0f, 0.0f, 0.0f);
		FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FVector4f HistoryGatherUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FRDGTextureRef SpecularAndSecondMomentHistory = nullptr;
		FRDGTextureRef NumFramesAccumulatedHistory = nullptr;

		FReflectionTemporalState* ReflectionState = nullptr;
		if (View.ViewState)
		{
			ReflectionState = &View.ViewState->Lumen.TranslucentReflectionState;
		}

		if (ReflectionState
			&& !View.bCameraCut
			&& !View.bPrevTransformsReset
			&& bTemporal)
		{
			HistoryScreenPositionScaleBias = ReflectionState->HistoryScreenPositionScaleBias;
			HistoryUVMinMax = ReflectionState->HistoryUVMinMax;
			HistoryGatherUVMinMax = ReflectionState->HistoryGatherUVMinMax;

			if (ReflectionState->SpecularAndSecondMomentHistory
				&& ReflectionState->NumFramesAccumulatedHistory
				&& ReflectionState->SpecularAndSecondMomentHistory->GetDesc().Extent == View.GetSceneTexturesConfig().Extent)
			{
				SpecularAndSecondMomentHistory = GraphBuilder.RegisterExternalTexture(ReflectionState->SpecularAndSecondMomentHistory);
				NumFramesAccumulatedHistory = GraphBuilder.RegisterExternalTexture(ReflectionState->NumFramesAccumulatedHistory);
			}
		}

		FRDGTextureRef SceneDepthHistory = nullptr;
		if (View.ViewState && View.ViewState->StochasticLighting.SceneDepthHistory)
		{
			SceneDepthHistory = GraphBuilder.RegisterExternalTexture(View.ViewState->StochasticLighting.SceneDepthHistory);
		}

		FRDGTextureRef NumFramesAccumulated = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2DArray(View.GetSceneTexturesConfig().Extent, PF_G8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount),
			TEXT("Lumen.RTTranslucency.NumFramesAccumulated"));

		// Temporal accumulation
		{
			const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
			// Not using translucent velocity currently
			FRDGTextureRef VelocityTexture = SystemTextures.Black;

			FLumenReflectionDenoiserTemporalCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenReflectionDenoiserTemporalCS::FParameters>();
			PassParameters->DenoiserParameters = DenoiserParameters;
			PassParameters->ResolvedSpecularLighting = ResolvedSpecularIndirect;
			PassParameters->ResolvedReflectionsDepth = ResolvedSpecularIndirectDepth;
			PassParameters->SpecularHistoryTexture = SpecularAndSecondMomentHistory;
			PassParameters->NumFramesAccumulatedHistoryTexture = NumFramesAccumulatedHistory;
			PassParameters->VelocityTexture = VelocityTexture;
			PassParameters->SceneDepthHistory = SceneDepthHistory;
			PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
			PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
			PassParameters->HistoryUVMinMax = HistoryUVMinMax;
			PassParameters->HistoryGatherUVMinMax = HistoryGatherUVMinMax;
			PassParameters->RWSpecularAndSecondMoment = GraphBuilder.CreateUAV(SpecularAndSecondMoment);
			PassParameters->RWNumFramesAccumulated = GraphBuilder.CreateUAV(NumFramesAccumulated);
			PassParameters->TemporalMaxFramesAccumulated = LumenReflections::GetMaxFramesAccumulated();
			PassParameters->TemporalNeighborhoodClampScale = CVarLumenReflectionsTemporalNeighborhoodClampScale.GetValueOnRenderThread();
			PassParameters->HistoryDistanceThreshold = GLumenReflectionHistoryDistanceThreshold;
			PassParameters->ClosureIndex = 0;
			PassParameters->ReflectionsDenoiserOneOverTonemapRange = LumenReflections::GetDenoiserOneOverTonemapRange();

			FLumenReflectionDenoiserTemporalCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenReflectionDenoiserTemporalCS::FValidHistory>(SceneDepthHistory != nullptr && SpecularAndSecondMomentHistory != nullptr && bTemporal);
			PermutationVector.Set<FLumenReflectionDenoiserTemporalCS::FRayTracedTranslucencyLighting>(true);
			PermutationVector.Set<FLumenReflectionDenoiserTemporalCS::FDebug>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FLumenReflectionDenoiserTemporalCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TemporalAccumulation"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				ReflectionTileParameters.ResolveIndirectArgs,
				0);
		}

		// Spatial filter
		{
			FLumenReflectionDenoiserSpatialCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenReflectionDenoiserSpatialCS::FParameters>();
			PassParameters->DenoiserParameters = DenoiserParameters;
			PassParameters->RWSpecularIndirectAccumulated = nullptr;
			PassParameters->RWTranslucencyLighting = FinalRadianceUAV;
			PassParameters->SpecularLightingAndSecondMomentTexture = SpecularAndSecondMoment;
			PassParameters->BackgroundVisibilityTexture = InOutBackgroundVisibility;
			PassParameters->NumFramesAccumulatedTexture = NumFramesAccumulated;
			PassParameters->SpatialFilterDepthWeightScale = GLumenReflectionBilateralFilterDepthWeightScale;
			PassParameters->SpatialFilterKernelRadius = CVarLumenReflectionBilateralFilterKernelRadius.GetValueOnRenderThread();
			PassParameters->SpatialFilterNumSamples = FMath::Clamp(GLumenReflectionBilateralFilterNumSamples, 0, 1024);
			PassParameters->TemporalMaxFramesAccumulated = LumenReflections::GetMaxFramesAccumulated();
			PassParameters->bCompositeSceneColor = bCompositeBackToSceneColor ? 1 : 0;
			PassParameters->ClosureIndex = 0;
			PassParameters->ReflectionsDenoiserOneOverTonemapRange = LumenReflections::GetDenoiserOneOverTonemapRange();

			FLumenReflectionDenoiserSpatialCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenReflectionDenoiserSpatialCS::FFrontLayerTranslucency>(true);
			PermutationVector.Set<FLumenReflectionDenoiserSpatialCS::FRayTracedTranslucency>(true);
			PermutationVector.Set<FLumenReflectionDenoiserSpatialCS::FSpatialFilter>(bSpatial);
			PermutationVector.Set<FLumenReflectionDenoiserSpatialCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FLumenReflectionDenoiserSpatialCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Spatial"),
				ComputeShader,
				PassParameters,
				ReflectionTileParameters.ResolveIndirectArgs,
				0);
		}

		if (ReflectionState && !View.bStatePrevViewInfoIsReadOnly)
		{
			ReflectionState->HistoryFrameIndex = View.ViewState->PendingPrevFrameNumber;
			ReflectionState->HistoryViewRect = View.ViewRect;
			ReflectionState->HistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(View.GetSceneTexturesConfig().Extent, View.ViewRect);

			const FVector2D InvBufferSize(1.0f / SceneTextures.Config.Extent.X, 1.0f / SceneTextures.Config.Extent.Y);

			ReflectionState->HistoryUVMinMax = FVector4f(
				View.ViewRect.Min.X * InvBufferSize.X,
				View.ViewRect.Min.Y * InvBufferSize.Y,
				View.ViewRect.Max.X * InvBufferSize.X,
				View.ViewRect.Max.Y * InvBufferSize.Y);

			// Clamp gather4 to a valid bilinear footprint in order to avoid sampling outside of valid bounds
			ReflectionState->HistoryGatherUVMinMax = FVector4f(
				(View.ViewRect.Min.X + 0.51f) * InvBufferSize.X,
				(View.ViewRect.Min.Y + 0.51f) * InvBufferSize.Y,
				(View.ViewRect.Max.X - 0.51f) * InvBufferSize.X,
				(View.ViewRect.Max.Y - 0.51f) * InvBufferSize.Y);

			if (SpecularAndSecondMoment && NumFramesAccumulated && bTemporal)
			{
				GraphBuilder.QueueTextureExtraction(SpecularAndSecondMoment, &ReflectionState->SpecularAndSecondMomentHistory);
				GraphBuilder.QueueTextureExtraction(NumFramesAccumulated, &ReflectionState->NumFramesAccumulatedHistory);
			}
			else
			{
				ReflectionState->SpecularAndSecondMomentHistory = nullptr;
				ReflectionState->NumFramesAccumulatedHistory = nullptr;
			}
		}
	}
}

bool FDeferredShadingSceneRenderer::RenderRayTracedTranslucency(
	FRDGBuilder& GraphBuilder,
	FSceneTextures& SceneTextures,
	FLumenSceneFrameTemporaries& FrameTemporaries,
	const FFrontLayerTranslucencyData& FrontLayerTranslucencyData)
{
	LLM_SCOPE_BYTAG(Lumen);
	RDG_GPU_STAT_SCOPE(GraphBuilder, RayTracedTranslucency);

	const bool bUseRayTracedRefraction = RayTracedTranslucency::UseRayTracedRefraction(Views);
	const bool bRenderDistortion = !bUseRayTracedRefraction && ShouldRenderDistortion();
	const bool bSceneColorChanged = !bRenderDistortion || !ViewFamily.AllowStandardTranslucencySeparated();

	FRDGTextureRef FinalRadianceTexture = nullptr;
	FRDGTextureRef BackgroundVisibilityTexture = nullptr;
	FTranslucencyPassResourcesMap TranslucencyResourceMap(Views.Num());

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		RDG_EVENT_SCOPE_STAT(GraphBuilder, RayTracedTranslucency, "RayTracedTranslucency View%d", ViewIndex);

		FViewInfo& View = Views[ViewIndex];

		RenderRayTracedTranslucencyView(
			GraphBuilder,
			View,
			SceneTextures,
			FrameTemporaries,
			FrontLayerTranslucencyData,
			FinalRadianceTexture,
			BackgroundVisibilityTexture);

		if (FinalRadianceTexture && BackgroundVisibilityTexture && !bSceneColorChanged && bRenderDistortion)
		{
			FTranslucencyPassResources& TranslucencyResources = TranslucencyResourceMap.Get(ViewIndex, ETranslucencyPass::TPT_TranslucencyStandard);
			TranslucencyResources.ViewRect = View.ViewRect;
			TranslucencyResources.ColorTexture = FinalRadianceTexture;
			TranslucencyResources.ColorModulateTexture = BackgroundVisibilityTexture;
		}
	}

	if (FinalRadianceTexture)
	{
		if (bSceneColorChanged)
		{
			SceneTextures.Color = FinalRadianceTexture;
			SceneTextures.UniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, FeatureLevel, SceneTextures.SetupMode);
		}

		if (bRenderDistortion)
		{
			RenderDistortion(GraphBuilder, SceneTextures.Color.Target, SceneTextures.Depth.Target, SceneTextures.Velocity, TranslucencyResourceMap);
		}
	}

	return FinalRadianceTexture != nullptr;
}

void Lumen::Shutdown()
{
	GVisualizeReflectionTracesData.SafeRelease();
}