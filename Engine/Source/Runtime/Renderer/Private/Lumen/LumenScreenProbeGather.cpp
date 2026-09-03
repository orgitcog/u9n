// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenScreenProbeGather.h"
#include "BasePassRendering.h"
#include "Lumen/LumenRadianceCache.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "ScreenSpaceDenoise.h"
#include "HairStrands/HairStrandsEnvironment.h"
#include "Substrate/Substrate.h"
#include "LumenReflections.h"
#include "LumenShortRangeAO.h"
#include "ShaderPrintParameters.h"

extern FLumenGatherCvarState GLumenGatherCvars;

int32 GLumenScreenProbeGather = 1;
FAutoConsoleVariableRef GVarLumenScreenProbeGather(
	TEXT("r.Lumen.ScreenProbeGather"),
	GLumenScreenProbeGather,
	TEXT("Whether to use the Screen Probe Final Gather"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarLumenScreenProbeGatherWaveOps(
	TEXT("r.Lumen.ScreenProbeGather.WaveOps"),
	1,
	TEXT("Whether to use wave ops for Lumen Screen Probe Gather."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

FAutoConsoleVariableRef CVarLumenScreenProbeGatherTraceMeshSDFs(
	TEXT("r.Lumen.ScreenProbeGather.TraceMeshSDFs"),
	GLumenGatherCvars.TraceMeshSDFs,
	TEXT("Whether to trace against Mesh Signed Distance fields for Lumen's Screen Probe Gather."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenScreenProbeGatherNumAdaptiveProbes(
	TEXT("r.Lumen.ScreenProbeGather.NumAdaptiveProbes"),
	8,
	TEXT("Number of adaptive probes to try to place per default placed uniform screen probe."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeGatherAdaptiveProbeAllocationFraction = .5f;
FAutoConsoleVariableRef GVarAdaptiveProbeAllocationFraction(
	TEXT("r.Lumen.ScreenProbeGather.AdaptiveProbeAllocationFraction"),
	GLumenScreenProbeGatherAdaptiveProbeAllocationFraction,
	TEXT("Fraction of uniform probes to allow for adaptive probe placement."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeGatherReferenceMode = 0;
FAutoConsoleVariableRef GVarLumenScreenProbeGatherReferenceMode(
	TEXT("r.Lumen.ScreenProbeGather.ReferenceMode"),
	GLumenScreenProbeGatherReferenceMode,
	TEXT("When enabled, traces 1024 uniform rays per probe with no filtering, Importance Sampling or Radiance Caching."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeTracingOctahedronResolution = 8;
FAutoConsoleVariableRef GVarLumenScreenProbeTracingOctahedronResolution(
	TEXT("r.Lumen.ScreenProbeGather.TracingOctahedronResolution"),
	GLumenScreenProbeTracingOctahedronResolution,
	TEXT("Resolution of the tracing octahedron.  Determines how many traces are done per probe."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeGatherOctahedronResolutionScale = 1.0f;
FAutoConsoleVariableRef GVarLumenScreenProbeGatherOctahedronResolutionScale(
	TEXT("r.Lumen.ScreenProbeGather.GatherOctahedronResolutionScale"),
	GLumenScreenProbeGatherOctahedronResolutionScale,
	TEXT("Resolution that probe filtering and integration will happen at, as a scale of TracingOctahedronResolution"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeDownsampleFactor = 16;
FAutoConsoleVariableRef GVarLumenScreenProbeDownsampleFactor(
	TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor"),
	GLumenScreenProbeDownsampleFactor,
	TEXT("Pixel size of the screen tile that a screen probe will be placed on."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeFullResolutionJitterWidth = 1;
FAutoConsoleVariableRef GVarLumenScreenProbeFullResolutionJitterWidth(
	TEXT("r.Lumen.ScreenProbeGather.FullResolutionJitterWidth"),
	GLumenScreenProbeFullResolutionJitterWidth,
	TEXT("Size of the full resolution jitter applied to Screen Probe upsampling, as a fraction of a screen tile.  A width of 1 results in jittering by DownsampleFactor number of pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeIntegrationTileClassification = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeIntegrationTileClassification(
	TEXT("r.Lumen.ScreenProbeGather.IntegrationTileClassification"),
	GLumenScreenProbeIntegrationTileClassification,
	TEXT("Whether to use tile classification during diffuse integration.  Tile Classification splits compute dispatches by VGPRs for better occupancy, but can introduce errors if implemented incorrectly."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeSupportTwoSidedFoliageBackfaceDiffuse = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeSupportBackfaceDiffuse(
	TEXT("r.Lumen.ScreenProbeGather.TwoSidedFoliageBackfaceDiffuse"),
	GLumenScreenProbeSupportTwoSidedFoliageBackfaceDiffuse,
	TEXT("Whether to gather lighting along the backface for the Two Sided Foliage shading model, which adds some GPU cost.  The final lighting is then DiffuseColor * FrontfaceLighting + SubsurfaceColor * BackfaceLighting.  When disabled, SubsurfaceColor will simply be added to DiffuseColor instead."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeDiffuseIntegralMethod = 0;
FAutoConsoleVariableRef CVarLumenScreenProbeDiffuseIntegralMethod(
	TEXT("r.Lumen.ScreenProbeGather.DiffuseIntegralMethod"),
	GLumenScreenProbeDiffuseIntegralMethod,
	TEXT("Spherical Harmonic = 0, Importance Sample BRDF = 1, Numerical Integral Reference = 2"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeMaterialAO = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeMaterialAO(
	TEXT("r.Lumen.ScreenProbeGather.MaterialAO"),
	GLumenScreenProbeMaterialAO,
	TEXT("Whether to apply Material Ambient Occlusion or Material Bent Normal to Lumen GI."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarLumenScreenProbeInterpolationDepthWeight(
	TEXT("r.Lumen.ScreenProbeGather.InterpolationDepthWeight"),
	1.0f,
	TEXT("Strength of a distance test when interpolating probes.")
	TEXT("Higher values will make lighting sharper on small elements, but somewhat less stable and will spawn more adaptive probes."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarLumenScreenProbeInterpolationDepthWeightForFoliage(
	TEXT("r.Lumen.ScreenProbeGather.InterpolationDepthWeightForFoliage"),
	0.1f,
	TEXT("Strength of a distance test when interpolating probes on foliage pixels.")
	TEXT("Higher values will make lighting sharper on small elements, but somewhat less stable and will spawn more adaptive probes.")
	TEXT("Usually can be relaxed on foliage in order to spawn less adaptive probes, as light leaking is less visible on foliage."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeTemporalFilter = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalFilter(
	TEXT("r.Lumen.ScreenProbeGather.Temporal"),
	GLumenScreenProbeTemporalFilter,
	TEXT("Whether to use a temporal filter"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenScreenProbeClearHistoryEveryFrame = 0;
FAutoConsoleVariableRef CVarLumenScreenProbeClearHistoryEveryFrame(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.ClearHistoryEveryFrame"),
	GLumenScreenProbeClearHistoryEveryFrame,
	TEXT("Whether to clear the history every frame for debugging"),
	ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<float> CVarLumenScreenProbeHistoryDistanceThreshold(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.DistanceThreshold"),
	0.01f,
	TEXT("Relative distance threshold needed to discard last frame's lighting results.  Lower values reduce ghosting from characters when near a wall but increase flickering artifacts."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<float> CVarLumenScreenProbeHistoryDistanceThresholdForFoliage(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.DistanceThresholdForFoliage"),
	0.03f,
	TEXT("r.Lumen.ScreenProbeGather.Temporal.DistanceThreshold which only affects foliage pixels. Often foliage has lots of discontinuities and edges and it's beneficial to be more agressive with keeping history there."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenScreenProbeFractionOfLightingMovingForFastUpdateMode = .1f;
FAutoConsoleVariableRef CVarLumenScreenProbeFractionOfLightingMovingForFastUpdateMode(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.FractionOfLightingMovingForFastUpdateMode"),
	GLumenScreenProbeFractionOfLightingMovingForFastUpdateMode,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenScreenProbeTemporalMaxFastUpdateModeAmount = .9f;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalMaxFastUpdateModeAmount(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxFastUpdateModeAmount"),
	GLumenScreenProbeTemporalMaxFastUpdateModeAmount,
	TEXT("Maximum amount of fast-responding temporal filter to use when traces hit a moving object.  Values closer to 1 cause more noise, but also faster reaction to scene changes."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);
int32 GLumenScreenProbeTemporalFastUpdateModeUseNeighborhoodClamp = 0;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalFastUpdateModeUseNeighborhoodClamp(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.FastUpdateModeUseNeighborhoodClamp"),
	GLumenScreenProbeTemporalFastUpdateModeUseNeighborhoodClamp,
	TEXT("Whether to clamp history values to the current frame's screen space neighborhood, in areas around moving objects."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarLumenScreenProbeTemporalRejectBasedOnNormal(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.RejectBasedOnNormal"),
	0,
	TEXT("Whether to reject history lighting based on their normal.  Increases cost of the temporal filter but can reduce streaking especially around character feet."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenScreenProbeRelativeSpeedDifferenceToConsiderLightingMoving = .005f;
FAutoConsoleVariableRef CVarLumenScreenProbeRelativeSpeedDifferenceToConsiderLightingMoving(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.RelativeSpeedDifferenceToConsiderLightingMoving"),
	GLumenScreenProbeRelativeSpeedDifferenceToConsiderLightingMoving,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenScreenProbeTemporalMaxFramesAccumulated = 10.0f;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalMaxFramesAccumulated(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated"),
	GLumenScreenProbeTemporalMaxFramesAccumulated,
	TEXT("Lower values cause the temporal filter to propagate lighting changes faster, but also increase flickering from noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<int32> CVarLumenScreenProbeTemporalMaxRayDirections(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxRayDirections"),
	8,
	TEXT("Number of possible random directions per pixel. Should be tweaked based on MaxFramesAccumulated."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

float GLumenScreenProbeTemporalHistoryNormalThreshold = 45.0f;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalHistoryNormalThreshold(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.NormalThreshold"),
	GLumenScreenProbeTemporalHistoryNormalThreshold,
	TEXT("Maximum angle that the history texel's normal can be from the current pixel to accept it's history lighting, in degrees."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenScreenProbeScreenTracesThicknessScaleWhenNoFallback = 2;
FAutoConsoleVariableRef CVarLumenScreenProbeScreenTracesThicknessScaleWhenNoFallback(
	TEXT("r.Lumen.ScreenProbeGather.ScreenTraces.ThicknessScaleWhenNoFallback"),
	GLumenScreenProbeScreenTracesThicknessScaleWhenNoFallback,
	TEXT("Larger scales effectively treat depth buffer surfaces as thicker for screen traces when there is no Distance Field present to resume the occluded ray."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenScreenProbeSpatialFilter = 1;
FAutoConsoleVariableRef GVarLumenScreenProbeFilter(
	TEXT("r.Lumen.ScreenProbeGather.SpatialFilterProbes"),
	GLumenScreenProbeSpatialFilter,
	TEXT("Whether to spatially filter probe traces to reduce noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeTemporalFilterProbes = 0;
FAutoConsoleVariableRef GVarLumenScreenProbeTemporalFilter(
	TEXT("r.Lumen.ScreenProbeGather.TemporalFilterProbes"),
	GLumenScreenProbeTemporalFilterProbes,
	TEXT("Whether to temporally filter probe traces to reduce noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeExtraAmbientOcclusion = 0;
FAutoConsoleVariableRef GVarLumenScreenProbeExtraAmbientOcclusion(
	TEXT("r.Lumen.ScreenProbeGather.ExtraAmbientOcclusion"),
	GLumenScreenProbeExtraAmbientOcclusion,
	TEXT("Indirect Occlusion is already included in Lumen's Global Illumination, but Ambient Occlusion can also be calculated cheaply if desired for non-physically based art direction.\n")
	TEXT("0: Extra AO off\n")
	TEXT("1: Extra AO on"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeExtraAmbientOcclusionMaxDistanceWorldSpace = 500.0f;
FAutoConsoleVariableRef GVarLumenScreenProbeExtraAmbientOcclusionMaxDistanceWorldSpace(
	TEXT("r.Lumen.ScreenProbeGather.ExtraAmbientOcclusion.MaxDistanceWorldSpace"),
	GLumenScreenProbeExtraAmbientOcclusionMaxDistanceWorldSpace,
	TEXT("Maximum distance from the receiver surface that another surface in the world should cause ambient occlusion."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeExtraAmbientOcclusionExponent = .5f;
FAutoConsoleVariableRef GVarLumenScreenProbeExtraAmbientOcclusionExponent(
	TEXT("r.Lumen.ScreenProbeGather.ExtraAmbientOcclusion.Exponent"),
	GLumenScreenProbeExtraAmbientOcclusionExponent,
	TEXT("Exponent applied to the distance fraction of an occluder to calculate its occlusion. Values smaller than one reduce the occlusion of nearby objects, while values larger than one increase the occlusion of nearby objects."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenShortRangeAmbientOcclusion = 1;
FAutoConsoleVariableRef GVarLumenScreenSpaceShortRangeAO(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO"),
	GLumenShortRangeAmbientOcclusion,
	TEXT("Whether to compute a short range, full resolution AO to add high frequency occlusion (contact shadows) which Screen Probes lack due to downsampling."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeFixedJitterIndex = -1;
FAutoConsoleVariableRef CVarLumenScreenProbeUseJitter(
	TEXT("r.Lumen.ScreenProbeGather.FixedJitterIndex"),
	GLumenScreenProbeFixedJitterIndex,
	TEXT("If zero or greater, overrides the temporal jitter index with a fixed index.  Useful for debugging and inspecting sampling patterns."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenScreenProbeFixedStateFrameIndex(
	TEXT("r.Lumen.ScreenProbeGather.FixedStateFrameIndex"),
	-1,
	TEXT("Whether to override View.StateFrameIndex for debugging."),
	ECVF_RenderThreadSafe
);

int32 GLumenRadianceCache = 1;
FAutoConsoleVariableRef CVarRadianceCache(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache"),
	GLumenRadianceCache,
	TEXT("Whether to enable the Persistent world space Radiance Cache"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenScreenProbeIrradianceFormat = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeIrradianceFormat(
	TEXT("r.Lumen.ScreenProbeGather.IrradianceFormat"),
	GLumenScreenProbeIrradianceFormat,
	TEXT("Prefilter irradiance format\n")
	TEXT("0 - Full 3rd order SH. Higher quality but slower\n")
	TEXT("1 - Octahedral probe. Faster, but reverts to SH3 when ShortRangeAO.ApplyDuringIntegration is enabled"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeStochasticInterpolation = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeStochasticInterpolation(
	TEXT("r.Lumen.ScreenProbeGather.StochasticInterpolation"),
	GLumenScreenProbeStochasticInterpolation,
	TEXT("Where to interpolate screen probes stochastically (1 sample) or bilinearly (4 samples)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenScreenProbeIntegrateDownsampleFactor(
	TEXT("r.Lumen.ScreenProbeGather.IntegrateDownsampleFactor"),
	1,
	TEXT("Downsampling factor for Screen Probe Integration. 2 makes this pass faster, but can blur some of the fine indirect lighting details on normal maps."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> GVarLumenScreenProbeMaxRoughnessToEvaluateRoughSpecular(
	TEXT("r.Lumen.ScreenProbeGather.MaxRoughnessToEvaluateRoughSpecular"),
	0.8f,
	TEXT("Maximum roughness value to evaluate rough specular in Screen Probe Gather. Lower values reduce GPU cost of integration, but also lose rough specular."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> GVarLumenScreenProbeMaxRoughnessToEvaluateRoughSpecularForFoliage(
	TEXT("r.Lumen.ScreenProbeGather.MaxRoughnessToEvaluateRoughSpecularForFoliage"),
	0.8f,
	TEXT("Maximum roughness value to evaluate rough specular in Screen Probe Gather for foliage pixels, where foliage pixel is a pixel with two sided or subsurface shading model. Lower values reduce GPU cost of integration, but also lose rough specular."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeRoughSpecularSamplingMode = 0;
FAutoConsoleVariableRef GVarLumenScreenProbeRoughSpecularSamplingMode(
	TEXT("r.Lumen.ScreenProbeGather.RoughSpecularSamplingMode"),
	GLumenScreenProbeRoughSpecularSamplingMode,
	TEXT("Mode 0: use the diffuse SH sample as specular. Mode 1: sample the SH along the main GGX specular reflection vector."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeTileDebugMode = 0;
FAutoConsoleVariableRef GVarLumenScreenProbeTileDebugMode(
	TEXT("r.Lumen.ScreenProbeGather.TileDebugMode"),
	GLumenScreenProbeTileDebugMode,
	TEXT("Display Lumen screen probe tile classification."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> GVarLumenScreenProbeGatherDebug(
	TEXT("r.Lumen.ScreenProbeGather.Debug"),
	0,
	TEXT("Whether to enable debug mode, which prints various extra debug information from shaders."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> GVarLumenScreenProbeGatherDebugProbePlacement(
	TEXT("r.Lumen.ScreenProbeGather.Debug.ProbePlacement"),
	0,
	TEXT("Whether visualize screen probe placement."),
	ECVF_RenderThreadSafe);

bool SupportsHairScreenTraces();

namespace LumenScreenProbeGather 
{
	// Keep in sync with LumenScreenProbeGather.usf
	const FIntPoint AdaptiveSamplesPerPassXY(2, 2);

	uint32 GetStateFrameIndex(const FSceneViewState* ViewState)
	{
		uint32 StateFrameIndex = ViewState ? ViewState->GetFrameIndex() : 0;

		if (CVarLumenScreenProbeFixedStateFrameIndex.GetValueOnRenderThread() >= 0)
		{
			StateFrameIndex = CVarLumenScreenProbeFixedStateFrameIndex.GetValueOnRenderThread();
		}

		return StateFrameIndex;
	}

	uint32 GetIntegrateDownsampleFactor(const FViewInfo& View)
	{
		// For now downsampling is only supported on a specific rendering path
		if (LumenShortRangeAO::ShouldApplyDuringIntegration()
			|| GLumenScreenProbeIntegrationTileClassification == 0
			// For now, we don't support ScreenProbeGather integrate downsample factor !=1.
			// Substrate overflow tiles are randomly scatter (i.e.,  not grouped in 2x2 8px-Tile), which causes a lot of complication. 
			// * Classification: this can be handled by loading 4 8x8-subtile to mark correctly the type of integration. 
			// * BuildTileList: the BuildTileList output a single 8x8 tile, which makes difficult to use the 4 8x8-subtile approch. The 8x8 tile is mapped onto a 16x16 pixel region during the Integrate step.
			// * Integrate: 8x8 pixels out of the 16x16 area are selected with a jitter for the actual shading.
			|| (Substrate::IsSubstrateEnabled() && !Substrate::IsSubstrateBlendableGBufferEnabled(View.GetShaderPlatform()))) 
		{
			return 1;
		}

		return FMath::Clamp(CVarLumenScreenProbeIntegrateDownsampleFactor.GetValueOnAnyThread(), 1, 2);
	}

	int32 GetTracingOctahedronResolution(const FViewInfo& View)
	{
		const float SqrtQuality = FMath::Sqrt(FMath::Max(View.FinalPostProcessSettings.LumenFinalGatherQuality, 0.0f));
		const int32 TracingOctahedronResolution = FMath::Clamp(FMath::RoundToInt(SqrtQuality * GLumenScreenProbeTracingOctahedronResolution), 4, 16);
		ensureMsgf(IsProbeTracingResolutionSupportedForImportanceSampling(TracingOctahedronResolution), TEXT("Tracing resolution %u requested that is not supported by importance sampling"), TracingOctahedronResolution);
		return GLumenScreenProbeGatherReferenceMode ? 32 : TracingOctahedronResolution;
	}

	int32 GetGatherOctahedronResolution(int32 TracingOctahedronResolution)
	{
		if (GLumenScreenProbeGatherReferenceMode)
		{
			return 8;
		}

		if (GLumenScreenProbeGatherOctahedronResolutionScale >= 1.0f)
		{
			const int32 Multiplier = FMath::RoundToInt(GLumenScreenProbeGatherOctahedronResolutionScale);
			return TracingOctahedronResolution * Multiplier;
		}
		else
		{
			const int32 Divisor = FMath::RoundToInt(1.0f / FMath::Max(GLumenScreenProbeGatherOctahedronResolutionScale, .1f));
			return TracingOctahedronResolution / Divisor;
		}
	}
	
	int32 GetScreenDownsampleFactor(const FViewInfo& View, const FSceneTextures& SceneTextures)
	{
		if (GLumenScreenProbeGatherReferenceMode)
		{
			return 16;
		}

		const int32 UnclampedDownsampleFactor = GLumenScreenProbeDownsampleFactor / (View.FinalPostProcessSettings.LumenFinalGatherQuality >= 6.0f ? 2 : 1);

		FIntPoint MaxScreenProbeAtlasSize = SceneTextures.Config.Extent;
		MaxScreenProbeAtlasSize.Y += FMath::TruncToInt(MaxScreenProbeAtlasSize.Y * GLumenScreenProbeGatherAdaptiveProbeAllocationFraction);

		// Includes probe border for filtering
		FIntPoint MaxScreenProbeResolution = LumenScreenProbeGather::GetTracingOctahedronResolution(View) + 2 * (1 << (GLumenScreenProbeGatherNumMips - 1));

		// Clamp screen probe downsample factor so the trace atlas doesn't overflow the maximum texture resolution, which can happen with high screen percentage + high res screenshot
		const FIntPoint MinDownsampleFactorVector = FIntPoint::DivideAndRoundUp(MaxScreenProbeAtlasSize * MaxScreenProbeResolution, FIntPoint(GetMax2DTextureDimension(), GetMax2DTextureDimension()));
		const int32 MinDownsampleFactor = FMath::Max(4, (int32)FMath::RoundUpToPowerOfTwo(FMath::Max(MinDownsampleFactorVector.X, MinDownsampleFactorVector.Y)));

		if (MinDownsampleFactor > 4 && UnclampedDownsampleFactor < MinDownsampleFactor)
		{
			static bool bLogged = false;

			if (!bLogged)
			{
				UE_LOG(LogRenderer, Warning, TEXT("Increased Lumen ScreenProbeGather DownsampleFactor to %u (%u requested) to avoid overflowing max 2d texture size, quality loss."), MinDownsampleFactor, UnclampedDownsampleFactor);
				bLogged = true;
			}
		}

		return FMath::Clamp(UnclampedDownsampleFactor, MinDownsampleFactor, 64);
	}

	bool UseShortRangeAmbientOcclusion(const FEngineShowFlags& ShowFlags)
	{
		return GLumenScreenProbeGatherReferenceMode ? false : (GLumenShortRangeAmbientOcclusion != 0 && ShowFlags.LumenShortRangeAmbientOcclusion);
	}

	bool UseProbeSpatialFilter()
	{
		return GLumenScreenProbeGatherReferenceMode ? false : GLumenScreenProbeSpatialFilter != 0;
	}

	bool UseProbeTemporalFilter()
	{
		return GLumenScreenProbeGatherReferenceMode ? false : GLumenScreenProbeTemporalFilterProbes != 0;
	}

	int32 GetDiffuseIntegralMethod()
	{
		return GLumenScreenProbeGatherReferenceMode ? 2 : GLumenScreenProbeDiffuseIntegralMethod;
	}

	EScreenProbeIrradianceFormat GetScreenProbeIrradianceFormat(const FEngineShowFlags& ShowFlags)
	{
		const bool bApplyShortRangeAO = UseShortRangeAmbientOcclusion(ShowFlags) && LumenShortRangeAO::ShouldApplyDuringIntegration();
		if (bApplyShortRangeAO)
		{
			// At the moment only SH3 support bent normal path
			return EScreenProbeIrradianceFormat::SH3;
		}

		return (EScreenProbeIrradianceFormat)FMath::Clamp(GLumenScreenProbeIrradianceFormat, 0, 1);
	}

	bool UseScreenProbeExtraAO()
	{
		return GLumenScreenProbeExtraAmbientOcclusion != 0;
	}

	float GetScreenProbeFullResolutionJitterWidth(const FViewInfo& View)
	{
		return GLumenScreenProbeFullResolutionJitterWidth * (View.FinalPostProcessSettings.LumenFinalGatherQuality >= 4.0f ? .5f : 1.0f);
	}

	bool UseWaveOps(EShaderPlatform ShaderPlatform)
	{
		return Lumen::UseWaveOps(ShaderPlatform) && CVarLumenScreenProbeGatherWaveOps.GetValueOnRenderThread() != 0;
	}

	FIntPoint GetNumSamplesPerUniformProbe2D(uint32 NumSamplesPerUniformProbe)
	{
		if (NumSamplesPerUniformProbe >= 16)
		{
			return FIntPoint(4, 4);
		}
		else if (NumSamplesPerUniformProbe >= 8)
		{
			return FIntPoint(4, 2);
		}
		else
		{
			return FIntPoint(2, 2);
		}
	}
}

bool LumenScreenProbeGather::UseRejectBasedOnNormal()
{
	return GLumenScreenProbeGather != 0
		&& CVarLumenScreenProbeTemporalRejectBasedOnNormal.GetValueOnRenderThread() != 0;
}

static TAutoConsoleVariable<int32> CVarScreenProbeGatherRadianceCacheSkyVisibility(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.SkyVisibility"),
	0,
	TEXT("Whether to separate sky from radiance cache using separate sky visibility channel, or bake sky into the probe itself.\n")
	TEXT("Separate visibility can be later used to reconstruct high-quality sky reflections when using `r.Lumen.Reflections.RadianceCache 1`"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GRadianceCacheNumClipmaps = 4;
FAutoConsoleVariableRef CVarRadianceCacheNumClipmaps(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.NumClipmaps"),
	GRadianceCacheNumClipmaps,
	TEXT("Number of radiance cache clipmaps."),
	ECVF_RenderThreadSafe
);

float GLumenRadianceCacheClipmapWorldExtent = 2500.0f;
FAutoConsoleVariableRef CVarLumenRadianceCacheClipmapWorldExtent(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ClipmapWorldExtent"),
	GLumenRadianceCacheClipmapWorldExtent,
	TEXT("World space extent of the first clipmap"),
	ECVF_RenderThreadSafe
);

float GLumenRadianceCacheClipmapDistributionBase = 2.0f;
FAutoConsoleVariableRef CVarLumenRadianceCacheClipmapDistributionBase(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ClipmapDistributionBase"),
	GLumenRadianceCacheClipmapDistributionBase,
	TEXT("Base of the Pow() that controls the size of each successive clipmap relative to the first."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRadianceCacheNumProbesToTraceBudget(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.NumProbesToTraceBudget"),
	100,
	TEXT("Number of radiance cache probes that can be updated per frame."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GRadianceCacheGridResolution = 48;
FAutoConsoleVariableRef CVarRadianceCacheResolution(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.GridResolution"),
	GRadianceCacheGridResolution,
	TEXT("Resolution of the probe placement grid within each clipmap"),
	ECVF_RenderThreadSafe
);

int32 GRadianceCacheProbeResolution = 32;
FAutoConsoleVariableRef CVarRadianceCacheProbeResolution(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ProbeResolution"),
	GRadianceCacheProbeResolution,
	TEXT("Resolution of the probe's 2d radiance layout.  The number of rays traced for the probe will be ProbeResolution ^ 2"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GRadianceCacheNumMipmaps = 1;
FAutoConsoleVariableRef CVarRadianceCacheNumMipmaps(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.NumMipmaps"),
	GRadianceCacheNumMipmaps,
	TEXT("Number of radiance cache mipmaps."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRadianceCacheProbeAtlasResolutionInProbes(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ProbeAtlasResolutionInProbes"),
	128,
	TEXT("Number of probes along one dimension of the probe atlas cache texture. This controls the memory usage of the cache. Overflow currently results in incorrect rendering. Aligned to the next power of two."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GRadianceCacheReprojectionRadiusScale = 1.5f;
FAutoConsoleVariableRef CVarRadianceCacheProbeReprojectionRadiusScale(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ReprojectionRadiusScale"),
	GRadianceCacheReprojectionRadiusScale,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GRadianceCacheStats = 0;
FAutoConsoleVariableRef CVarRadianceCacheStats(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.Stats"),
	GRadianceCacheStats,
	TEXT("GPU print out Radiance Cache update stats."),
	ECVF_RenderThreadSafe
);

bool LumenScreenProbeGather::UseRadianceCache()
{
	return GLumenScreenProbeGatherReferenceMode ? false : GLumenRadianceCache != 0;
}

bool LumenScreenProbeGather::UseRadianceCacheSkyVisibility()
{
	return UseRadianceCache() && CVarScreenProbeGatherRadianceCacheSkyVisibility.GetValueOnRenderThread() != 0;
}

namespace LumenScreenProbeGatherRadianceCache
{
	int32 GetNumClipmaps()
	{
		return FMath::Clamp(GRadianceCacheNumClipmaps, 1, LumenRadianceCache::MaxClipmaps);
	}

	int32 GetClipmapGridResolution()
	{
		const int32 GridResolution = GRadianceCacheGridResolution / (GLumenFastCameraMode ? 2 : 1);
		return FMath::Clamp(GridResolution, 1, 256);
	}

	int32 GetProbeResolution()
	{
		return GRadianceCacheProbeResolution / (GLumenFastCameraMode ? 2 : 1);
	}

	int32 GetFinalProbeResolution()
	{
		return GetProbeResolution() + 2 * (1 << (GRadianceCacheNumMipmaps - 1));
	}

	FIntVector GetProbeIndirectionTextureSize()
	{
		return FIntVector(GetClipmapGridResolution() * GRadianceCacheNumClipmaps, GetClipmapGridResolution(), GetClipmapGridResolution());
	}

	int32 GetProbeAtlasResolutionInProbes()
	{
		return FMath::RoundUpToPowerOfTwo(FMath::Clamp(CVarRadianceCacheProbeAtlasResolutionInProbes.GetValueOnRenderThread(), 1, 1024));
	}

	FIntPoint GetProbeAtlasTextureSize()
	{
		return FIntPoint(GetProbeAtlasResolutionInProbes() * GetProbeResolution());
	}

	FIntPoint GetFinalRadianceAtlasTextureSize()
	{
		return FIntPoint(GetProbeAtlasResolutionInProbes() * GetFinalProbeResolution(), GetProbeAtlasResolutionInProbes() * GetFinalProbeResolution());
	}

	int32 GetMaxNumProbes()
	{
		return GetProbeAtlasResolutionInProbes() * GetProbeAtlasResolutionInProbes();
	}

	LumenRadianceCache::FRadianceCacheInputs SetupRadianceCacheInputs(const FViewInfo& View)
	{
		LumenRadianceCache::FRadianceCacheInputs Parameters = LumenRadianceCache::GetDefaultRadianceCacheInputs();
		Parameters.ReprojectionRadiusScale = GRadianceCacheReprojectionRadiusScale;
		Parameters.ClipmapWorldExtent = GLumenRadianceCacheClipmapWorldExtent;
		Parameters.ClipmapDistributionBase = GLumenRadianceCacheClipmapDistributionBase;
		Parameters.RadianceProbeClipmapResolution = GetClipmapGridResolution();
		Parameters.ProbeAtlasResolutionInProbes = FIntPoint(GetProbeAtlasResolutionInProbes(), GetProbeAtlasResolutionInProbes());
		Parameters.NumRadianceProbeClipmaps = GetNumClipmaps();
		Parameters.RadianceProbeResolution = FMath::Max(GetProbeResolution(), LumenRadianceCache::MinRadianceProbeResolution);
		Parameters.FinalProbeResolution = GetFinalProbeResolution();
		Parameters.FinalRadianceAtlasMaxMip = GRadianceCacheNumMipmaps - 1;
		const float LightingUpdateSpeed = FMath::Clamp(View.FinalPostProcessSettings.LumenFinalGatherLightingUpdateSpeed, .5f, 4.0f);
		const float EditingBudgetScale = View.Family->bCurrentlyBeingEdited ? 10.0f : 1.0f;
		Parameters.NumProbesToTraceBudget = FMath::RoundToInt(CVarRadianceCacheNumProbesToTraceBudget.GetValueOnRenderThread() * LightingUpdateSpeed * EditingBudgetScale);
		Parameters.RadianceCacheStats = GRadianceCacheStats;
		Parameters.UseSkyVisibility = LumenScreenProbeGather::UseRadianceCacheSkyVisibility() ? 1 : 0;
		return Parameters;
	}
};

// Used for all Lumen Screen Probe Gather shaders
BEGIN_SHADER_PARAMETER_STRUCT(FScreenProbeGatherCommonParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
END_SHADER_PARAMETER_STRUCT()

class FScreenProbeDownsampleDepthUniformCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeDownsampleDepthUniformCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeDownsampleDepthUniformCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWScreenProbeSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeWorldNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeWorldSpeed)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWScreenProbeTranslatedWorldPosition)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
	END_SHADER_PARAMETER_STRUCT()

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
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeDownsampleDepthUniformCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeDownsampleDepthUniformCS", SF_Compute);

class FScreenProbeAdaptivePlacementMarkCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeAdaptivePlacementMarkCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeAdaptivePlacementMarkCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWAdaptiveProbePlacementMask)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeGatherCommonParameters, ScreenProbeGatherCommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
	END_SHADER_PARAMETER_STRUCT()

	class FNumSamplesPerUniformProbe : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_UNIFORM_PROBE", 4, 8, 16);
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerUniformProbe>;

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
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 NumSamplesPerUniformProbe = PermutationVector.Get<FNumSamplesPerUniformProbe>();
		const FIntPoint NumSamplesPerUniformProbe2D = LumenScreenProbeGather::GetNumSamplesPerUniformProbe2D(NumSamplesPerUniformProbe);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_UNIFORM_PROBE_X"), NumSamplesPerUniformProbe2D.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_UNIFORM_PROBE_Y"), NumSamplesPerUniformProbe2D.Y);

		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeAdaptivePlacementMarkCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeAdaptivePlacementMarkCS", SF_Compute);

class FScreenProbeAdaptivePlacementSpawnCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeAdaptivePlacementSpawnCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeAdaptivePlacementSpawnCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeWorldNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeWorldSpeed)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWScreenProbeTranslatedWorldPosition)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumAdaptiveScreenProbes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWAdaptiveScreenProbeData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWScreenTileAdaptiveProbeHeader)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWScreenTileAdaptiveProbeIndices)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, AdaptiveProbePlacementMask)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeGatherCommonParameters, ScreenProbeGatherCommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
	END_SHADER_PARAMETER_STRUCT()

	class FNumSamplesPerUniformProbe : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_UNIFORM_PROBE", 4, 8, 16);
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerUniformProbe>;

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
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 NumSamplesPerUniformProbe = PermutationVector.Get<FNumSamplesPerUniformProbe>();
		const FIntPoint NumSamplesPerUniformProbe2D = LumenScreenProbeGather::GetNumSamplesPerUniformProbe2D(NumSamplesPerUniformProbe);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_UNIFORM_PROBE_X"), NumSamplesPerUniformProbe2D.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_UNIFORM_PROBE_Y"), NumSamplesPerUniformProbe2D.Y);

		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeAdaptivePlacementSpawnCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeAdaptivePlacementSpawnCS", SF_Compute);

class FSetupAdaptiveProbeIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSetupAdaptiveProbeIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FSetupAdaptiveProbeIndirectArgsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWScreenProbeIndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSetupAdaptiveProbeIndirectArgsCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "SetupAdaptiveProbeIndirectArgsCS", SF_Compute);


class FMarkRadianceProbesUsedByScreenProbesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMarkRadianceProbesUsedByScreenProbesCS)
	SHADER_USE_PARAMETER_STRUCT(FMarkRadianceProbesUsedByScreenProbesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheMarkParameters, RadianceCacheMarkParameters)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FMarkRadianceProbesUsedByScreenProbesCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "MarkRadianceProbesUsedByScreenProbesCS", SF_Compute);

class FMarkRadianceProbesUsedByHairStrandsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMarkRadianceProbesUsedByHairStrandsCS)
	SHADER_USE_PARAMETER_STRUCT(FMarkRadianceProbesUsedByHairStrandsCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, HairStrandsResolution)
		SHADER_PARAMETER(FVector2f, HairStrandsInvResolution)
		SHADER_PARAMETER(uint32, HairStrandsMip)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheMarkParameters, RadianceCacheMarkParameters)
		RDG_BUFFER_ACCESS(IndirectBufferArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

public:

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
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FMarkRadianceProbesUsedByHairStrandsCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "MarkRadianceProbesUsedByHairStrandsCS", SF_Compute);

// Must match usf INTEGRATE_TILE_SIZE
const int32 GScreenProbeIntegrateTileSize = 8;

class FScreenProbeTileClassificationMarkCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeTileClassificationMarkCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeTileClassificationMarkCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<UNORM float>, RWLightIsMoving)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWBackfaceDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWRoughSpecularIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIntegrateIndirectArgs)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<uint>, RWTileClassificationModes)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenReflections::FCompositeParameters, ReflectionsCompositeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeIntegrateParameters, ScreenProbeIntegrateParameters)
		SHADER_PARAMETER(uint32, DefaultDiffuseIntegrationMethod)
		SHADER_PARAMETER(float, MaxRoughnessToEvaluateRoughSpecular)
		SHADER_PARAMETER(float, MaxRoughnessToEvaluateRoughSpecularForFoliage)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FOverflowTile : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	class FSupportBackfaceDiffuse : SHADER_PERMUTATION_BOOL("SUPPORT_BACKFACE_DIFFUSE");
	class FOutputDownsampledDepth : SHADER_PERMUTATION_BOOL("OUTPUT_DOWNSAMPLED_DEPTH");
	class FIntegrateDownsampleFactor : SHADER_PERMUTATION_RANGE_INT("INTEGRATE_DOWNSAMPLE_FACTOR", 1, 2);
	using FPermutationDomain = TShaderPermutationDomain<FOverflowTile, FSupportBackfaceDiffuse, FOutputDownsampledDepth, FIntegrateDownsampleFactor>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (PermutationVector.Get<FIntegrateDownsampleFactor>() == 1)
		{
			PermutationVector.Set<FOutputDownsampledDepth>(false);
		}

		if (!Substrate::IsSubstrateEnabled())
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

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeTileClassificationMarkCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeTileClassificationMarkCS", SF_Compute);


class FScreenProbeTileClassificationBuildListsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeTileClassificationBuildListsCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeTileClassificationBuildListsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIntegrateIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWIntegrateTileData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, TileClassificationModes)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER(FIntPoint, ViewportTileDimensions)
		SHADER_PARAMETER(FIntPoint, ViewportTileDimensionsWithOverflow)
		SHADER_PARAMETER(uint32, MaxClosurePerPixel)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOverflowTile>() && !Substrate::IsSubstrateEnabled())
		{
			return false;
		}
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
	class FOverflowTile : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	using FPermutationDomain = TShaderPermutationDomain<FOverflowTile>;

	static int32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeTileClassificationBuildListsCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeTileClassificationBuildListsCS", SF_Compute);


class FScreenProbeIntegrateCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeIntegrateCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeIntegrateCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<UNORM float>, RWLightIsMoving)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWBackfaceDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWRoughSpecularIndirect)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, IntegrateTileData)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeGatherParameters, GatherParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeIntegrateParameters, ScreenProbeIntegrateParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenScreenSpaceBentNormalParameters, ScreenSpaceBentNormalParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenReflections::FCompositeParameters, ReflectionsCompositeParameters)
		SHADER_PARAMETER(float, FullResolutionJitterWidth)
		SHADER_PARAMETER(float, MaxRoughnessToEvaluateRoughSpecular)
		SHADER_PARAMETER(float, MaxRoughnessToEvaluateRoughSpecularForFoliage)
		SHADER_PARAMETER(uint32, ApplyMaterialAO)
		SHADER_PARAMETER(float, MaxAOMultibounceAlbedo)
		SHADER_PARAMETER(uint32, LumenReflectionInputIsSSR)
		SHADER_PARAMETER(uint32, DefaultDiffuseIntegrationMethod)
		SHADER_PARAMETER(FIntPoint, ViewportTileDimensions)
		SHADER_PARAMETER(FIntPoint, ViewportTileDimensionsWithOverflow)
		SHADER_PARAMETER(uint32, MaxClosurePerPixel)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOverflowTile>() && !Substrate::IsSubstrateEnabled())
		{
			return false;
		}
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOverflowTile>() && !Substrate::IsSubstrateEnabled())
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		if (PermutationVector.Get<FStochasticProbeInterpolation>() != (GLumenScreenProbeStochasticInterpolation != 0))
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		const bool bUseTileClassification = GLumenScreenProbeIntegrationTileClassification != 0 && LumenScreenProbeGather::GetDiffuseIntegralMethod() != 2;
		int TileClassificationMode = PermutationVector.Get<FTileClassificationMode>();
		if (bUseTileClassification)
		{
			if (TileClassificationMode == (uint32)EScreenProbeIntegrateTileClassification::Num)
			{
				return EShaderPermutationPrecacheRequest::NotUsed;
			}
		}
		else if (TileClassificationMode != (uint32)EScreenProbeIntegrateTileClassification::Num)
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		const bool bApplyShortRangeAO = LumenShortRangeAO::ShouldApplyDuringIntegration();
		if (PermutationVector.Get<FShortRangeAO>() && !bApplyShortRangeAO)
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		// If derived from engine show flags then precache request is optional if not set because debug modes may allow those permutations to be used
		FEngineShowFlags DefaultShowEngineFlags(ESFIM_Game);
		if (PermutationVector.Get<FProbeIrradianceFormat>() != LumenScreenProbeGather::GetScreenProbeIrradianceFormat(DefaultShowEngineFlags))
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		const int32 RoughSpecularSamplingMode = GLumenScreenProbeRoughSpecularSamplingMode > 0 ? 1 : 0;
		if (PermutationVector.Get<FRoughSpecularSamplingMode>() != RoughSpecularSamplingMode)
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	class FShortRangeAO : SHADER_PERMUTATION_BOOL("SHORT_RANGE_AO");
	class FTileClassificationMode : SHADER_PERMUTATION_INT("INTEGRATE_TILE_CLASSIFICATION_MODE", 4);
	class FProbeIrradianceFormat : SHADER_PERMUTATION_ENUM_CLASS("PROBE_IRRADIANCE_FORMAT", EScreenProbeIrradianceFormat);
	class FStochasticProbeInterpolation : SHADER_PERMUTATION_BOOL("STOCHASTIC_PROBE_INTERPOLATION");
	class FOverflowTile : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	class FSupportBackfaceDiffuse : SHADER_PERMUTATION_BOOL("SUPPORT_BACKFACE_DIFFUSE");
	class FRoughSpecularSamplingMode : SHADER_PERMUTATION_INT("ROUGH_SPECULAR_SAMPLING_MODE", 2);
	class FIntegrateDownsampleFactor : SHADER_PERMUTATION_RANGE_INT("INTEGRATE_DOWNSAMPLE_FACTOR", 1, 2);
	class FScreenProbeExtraAO : SHADER_PERMUTATION_BOOL("SCREEN_PROBE_EXTRA_AO");
	using FPermutationDomain = TShaderPermutationDomain<FTileClassificationMode, FShortRangeAO, FProbeIrradianceFormat, FStochasticProbeInterpolation, FOverflowTile, FSupportBackfaceDiffuse, FRoughSpecularSamplingMode, FScreenProbeExtraAO, FIntegrateDownsampleFactor>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeIntegrateCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeIntegrateCS", SF_Compute);


class FScreenProbeTemporalReprojectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeTemporalReprojectionCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeTemporalReprojectionCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWNewHistoryDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWNewHistoryBackfaceDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWNewHistoryRoughSpecularIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<UNORM float>, RWNewHistoryFastUpdateMode_NumFramesAccumulated)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<UNORM float>, RWNewHistoryShortRangeAO)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeGatherCommonParameters, ScreenProbeGatherCommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeIntegrateParameters, ScreenProbeIntegrateParameters)
		SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, DiffuseIndirectHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, BackfaceDiffuseIndirectHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, RoughSpecularIndirectHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, ShortRangeAOHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, HistoryFastUpdateMode_NumFramesAccumulated)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DiffuseIndirectDepthHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DiffuseIndirectNormalHistory)
		SHADER_PARAMETER(float, HistoryDistanceThreshold)
		SHADER_PARAMETER(float, HistoryDistanceThresholdForFoliage)
		SHADER_PARAMETER(float,PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER(float,InvFractionOfLightingMovingForFastUpdateMode)
		SHADER_PARAMETER(float,MaxFastUpdateModeAmount)
		SHADER_PARAMETER(float,MaxFramesAccumulated)
		SHADER_PARAMETER(float,HistoryNormalCosThreshold)
		SHADER_PARAMETER(FIntPoint, ShortRangeAOViewMin)
		SHADER_PARAMETER(FIntPoint, ShortRangeAOViewSize)
		SHADER_PARAMETER(float, ShortRangeAOTemporalNeighborhoodClampScale)
		SHADER_PARAMETER(FVector4f,HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f,HistoryUVToScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f,HistoryUVMinMax)
		SHADER_PARAMETER(FIntVector4,HistoryViewportMinMax)
		SHADER_PARAMETER(FVector4f, HistoryBufferSizeAndInvSize)
		SHADER_PARAMETER(FVector3f, TargetFormatQuantizationError)
		SHADER_PARAMETER(uint32, bIsSubstrateTileHistoryValid)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, DiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, LightIsMoving)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, BackfaceDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, RoughSpecularIndirect)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, ShortRangeAOTexture)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FValidHistory : SHADER_PERMUTATION_BOOL("VALID_HISTORY");
	class FHistoryRejectBasedOnNormal : SHADER_PERMUTATION_BOOL("HISTORY_REJECT_BASED_ON_NORMAL");
	class FFastUpdateModeNeighborhoodClamp : SHADER_PERMUTATION_BOOL("FAST_UPDATE_MODE_NEIGHBORHOOD_CLAMP");
	class FOverflowTile : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	class FSupportBackfaceDiffuse : SHADER_PERMUTATION_BOOL("SUPPORT_BACKFACE_DIFFUSE");
	class FIntegrateDownsampleFactor : SHADER_PERMUTATION_RANGE_INT("INTEGRATE_DOWNSAMPLE_FACTOR", 1, 2);
	class FShortRangeAOMode : SHADER_PERMUTATION_RANGE_INT("SHORT_RANGE_AO_MODE", 0, 3);
	class FShortRangeAODownsampleFactor : SHADER_PERMUTATION_RANGE_INT("SHORT_RANGE_AO_DOWNSAMPLE_FACTOR", 1, 2);
	using FPermutationDomain = TShaderPermutationDomain<FValidHistory, FFastUpdateModeNeighborhoodClamp, FHistoryRejectBasedOnNormal, FOverflowTile, FSupportBackfaceDiffuse, FIntegrateDownsampleFactor, FShortRangeAOMode, FShortRangeAODownsampleFactor>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (PermutationVector.Get<FShortRangeAOMode>() == 0)
		{
			PermutationVector.Set<FShortRangeAODownsampleFactor>(1);
		}

		if (!Substrate::IsSubstrateEnabled())
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

		const bool bCompile = DoesPlatformSupportLumenGI(Parameters.Platform);

#if WITH_EDITOR
		if (bCompile)
		{
			ensureMsgf(VelocityEncodeDepth(Parameters.Platform), TEXT("Platform did not return true from VelocityEncodeDepth().  Lumen requires velocity depth."));
		}
#endif

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize() 
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeTemporalReprojectionCS, "/Engine/Private/Lumen/LumenScreenProbeGatherTemporal.usf", "ScreenProbeTemporalReprojectionCS", SF_Compute);

class FLumenScreenProbeSubstrateDebugPass : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenScreenProbeSubstrateDebugPass)
	SHADER_USE_PARAMETER_STRUCT(FLumenScreenProbeSubstrateDebugPass, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, LayerCount)
		SHADER_PARAMETER(uint32, MaxClosurePerPixel)
		SHADER_PARAMETER(FIntPoint, ViewportIntegrateTileDimensions)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrint)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, IntegrateTileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, IntegrateIndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

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
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenScreenProbeSubstrateDebugPass, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeDebugMain", SF_Compute);

class FScreenProbeGatherDebugCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeGatherDebugCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeGatherDebugCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSceneColor)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeGatherCommonParameters, ScreenProbeGatherCommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER(uint32, VisualizeProbePlacement)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

public:

	static uint32 GetGroupSize()
	{
		return 8;
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeGatherDebugCS, "/Engine/Private/Lumen/LumenScreenProbeDebug.usf", "ScreenProbeGatherDebugCS", SF_Compute);

void AddLumenScreenProbeDebugPass(
	FRDGBuilder& GraphBuilder, 
	FViewInfo& View,
	const FIntPoint& ViewportIntegrateTileDimensions,
	const FIntPoint& ViewportIntegrateTileDimensionsWithOverflow,
	FRDGBufferRef IntegrateTileData,
	FRDGBufferRef IntegrateIndirectArgs)
{
	// Force ShaderPrint on.
	ShaderPrint::SetEnabled(true);

	ShaderPrint::RequestSpaceForCharacters(1024);
	ShaderPrint::RequestSpaceForLines(1024);
	ShaderPrint::RequestSpaceForTriangles(ViewportIntegrateTileDimensionsWithOverflow.X * ViewportIntegrateTileDimensionsWithOverflow.Y * 2);

	FLumenScreenProbeSubstrateDebugPass::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenScreenProbeSubstrateDebugPass::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->MaxClosurePerPixel = Substrate::GetSubstrateMaxClosureCount(View);
	PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
	PassParameters->LayerCount = Substrate::GetSubstrateMaxClosureCount(View);
	PassParameters->ViewportIntegrateTileDimensions = ViewportIntegrateTileDimensions;
	PassParameters->IntegrateTileData = GraphBuilder.CreateSRV(IntegrateTileData);
	PassParameters->IntegrateIndirectArgs = GraphBuilder.CreateSRV(IntegrateIndirectArgs, PF_R32_UINT);
	ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrint);

	FLumenScreenProbeSubstrateDebugPass::FPermutationDomain PermutationVector;
	auto ComputeShader = View.ShaderMap->GetShader<FLumenScreenProbeSubstrateDebugPass>(PermutationVector);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ScreenProbeDebug"),
		ComputeShader,
		PassParameters,
		FIntVector(ViewportIntegrateTileDimensions.X, ViewportIntegrateTileDimensions.Y, 1));
}

const TCHAR* GetClassificationModeString(EScreenProbeIntegrateTileClassification Mode)
{
	if (Mode == EScreenProbeIntegrateTileClassification::SimpleDiffuse)
	{
		return TEXT("SimpleDiffuse");
	}
	else if (Mode == EScreenProbeIntegrateTileClassification::SupportImportanceSampleBRDF)
	{
		return TEXT("SupportImportanceSampleBRDF");
	}
	else if (Mode == EScreenProbeIntegrateTileClassification::SupportAll)
	{
		return TEXT("SupportAll");
	}

	return TEXT("");
}

extern float GLumenMaxShortRangeAOMultibounceAlbedo;

void InterpolateAndIntegrate(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	FViewInfo& View,
	const FScreenProbeParameters& ScreenProbeParameters,
	const FScreenProbeGatherParameters& GatherParameters,
	FScreenProbeIntegrateParameters& IntegrateParameters,
	const FLumenScreenSpaceBentNormalParameters& ScreenSpaceBentNormalParameters,
	bool bSSREnabled,
	FRDGTextureRef DiffuseIndirect,
	FRDGTextureRef LightIsMoving,
	FRDGTextureRef BackfaceDiffuseIndirect,
	FRDGTextureRef RoughSpecularIndirect,
	ERDGPassFlags ComputePassFlags)
{
	const bool bApplyShortRangeAO = ScreenSpaceBentNormalParameters.ShortRangeAOTexture && LumenShortRangeAO::ShouldApplyDuringIntegration();
	const bool bUseTileClassification = GLumenScreenProbeIntegrationTileClassification != 0 && LumenScreenProbeGather::GetDiffuseIntegralMethod() != 2;
	const bool bSupportBackfaceDiffuse = BackfaceDiffuseIndirect != nullptr;
	const int32 RoughSpecularSamplingMode = GLumenScreenProbeRoughSpecularSamplingMode > 0 ? 1 : 0;
	const int32 IntegrateDownsampleFactor = LumenScreenProbeGather::GetIntegrateDownsampleFactor(View);
	const FIntPoint IntegrateBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, IntegrateDownsampleFactor);
	const FIntPoint IntegrateViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), IntegrateDownsampleFactor);
	const FIntPoint ViewportIntegrateTileDimensions = FIntPoint::DivideAndRoundUp(IntegrateViewSize, GScreenProbeIntegrateTileSize);

	LumenReflections::FCompositeParameters ReflectionsCompositeParameters;
	LumenReflections::SetupCompositeParameters(View, ReflectionsCompositeParameters);

	if (bSSREnabled)
	{
		// SSR may not have a hit for any pixel, we need to have rough reflections to fall back to
		ReflectionsCompositeParameters.MaxRoughnessToTrace = -.1f;
	}

	if (ScreenSpaceBentNormalParameters.DownsampledSceneDepth)
	{
		// Can reuse existing downsampled scene depth and normal
		IntegrateParameters.DownsampledSceneDepth = ScreenSpaceBentNormalParameters.DownsampledSceneDepth;
		IntegrateParameters.DownsampledSceneWorldNormal = ScreenSpaceBentNormalParameters.DownsampledSceneWorldNormal;
	}
	else if (IntegrateDownsampleFactor > 1)
	{
		IntegrateParameters.DownsampledSceneDepth = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(IntegrateBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("Lumen.ScreenProbeGather.DownsampledSceneDepth"));

		IntegrateParameters.DownsampledSceneWorldNormal = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(IntegrateBufferSize, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("Lumen.ScreenProbeGather.DownsampledSceneWorldNormal"));
	}

	if (bUseTileClassification)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Integrate");

		const uint32 ClassificationScaleFactor = Substrate::IsSubstrateEnabled() ? 2u : 1u;
		FRDGBufferRef IntegrateIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(ClassificationScaleFactor * (uint32)EScreenProbeIntegrateTileClassification::Num), TEXT("Lumen.ScreenProbeGather.IntegrateIndirectArgs"));
		if (Substrate::IsSubstrateEnabled())
		{
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(IntegrateIndirectArgs, PF_R32_UINT), 0u);
		}

		checkf(ViewportIntegrateTileDimensions.X > 0 && ViewportIntegrateTileDimensions.Y > 0, TEXT("Compute shader needs non-zero dispatch to clear next pass's indirect args"));

		const FIntPoint EffectiveBufferResolution = Substrate::GetSubstrateTextureResolution(View, SceneTextures.Config.Extent);
		const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);
		const FIntPoint TileClassificationBufferDimensions(
			FMath::DivideAndRoundUp(EffectiveBufferResolution.X, GScreenProbeIntegrateTileSize),
			FMath::DivideAndRoundUp(EffectiveBufferResolution.Y, GScreenProbeIntegrateTileSize));

		FRDGTextureDesc TileClassificationModesDesc = FRDGTextureDesc::Create2DArray(TileClassificationBufferDimensions, PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
		FRDGTextureRef TileClassificationModes = GraphBuilder.CreateTexture(TileClassificationModesDesc, TEXT("Lumen.ScreenProbeGather.TileClassificationModes"));

		{
			FRDGTextureUAVRef RWDiffuseIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef RWLightIsMoving = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(LightIsMoving), ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef RWBackfaceDiffuseIndirect = bSupportBackfaceDiffuse ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(BackfaceDiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
			FRDGTextureUAVRef RWRoughSpecularIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RoughSpecularIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef DownsampledSceneDepthUAV = IntegrateParameters.DownsampledSceneDepth ? GraphBuilder.CreateUAV(IntegrateParameters.DownsampledSceneDepth, ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
			FRDGTextureUAVRef DownsampledSceneWorldNormalUAV = IntegrateParameters.DownsampledSceneWorldNormal ? GraphBuilder.CreateUAV(IntegrateParameters.DownsampledSceneWorldNormal, ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
			FRDGBufferUAVRef RWIntegrateIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(IntegrateIndirectArgs, PF_R32_UINT), ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef RWTileClassificationModes = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TileClassificationModes), ERDGUnorderedAccessViewFlags::SkipBarrier);

			auto ScreenProbeTileClassificationMark = [&](bool bOverflow)
			{
				FScreenProbeTileClassificationMarkCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeTileClassificationMarkCS::FParameters>();
				PassParameters->RWDiffuseIndirect = RWDiffuseIndirect;
				PassParameters->RWLightIsMoving = RWLightIsMoving;
				PassParameters->RWBackfaceDiffuseIndirect = RWBackfaceDiffuseIndirect;
				PassParameters->RWRoughSpecularIndirect = RWRoughSpecularIndirect;
				PassParameters->RWIntegrateIndirectArgs = RWIntegrateIndirectArgs;
				PassParameters->RWTileClassificationModes = RWTileClassificationModes;
				PassParameters->RWDownsampledSceneDepth = DownsampledSceneDepthUAV;
				PassParameters->RWDownsampledSceneWorldNormal = DownsampledSceneWorldNormalUAV;
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
				PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
				PassParameters->DefaultDiffuseIntegrationMethod = (uint32)LumenScreenProbeGather::GetDiffuseIntegralMethod();
				PassParameters->ReflectionsCompositeParameters = ReflectionsCompositeParameters;
				PassParameters->ScreenProbeIntegrateParameters = IntegrateParameters;
				PassParameters->MaxRoughnessToEvaluateRoughSpecular = GVarLumenScreenProbeMaxRoughnessToEvaluateRoughSpecular.GetValueOnRenderThread();
				PassParameters->MaxRoughnessToEvaluateRoughSpecularForFoliage = GVarLumenScreenProbeMaxRoughnessToEvaluateRoughSpecularForFoliage.GetValueOnRenderThread();

				FScreenProbeTileClassificationMarkCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FScreenProbeTileClassificationMarkCS::FOverflowTile>(bOverflow);
				PermutationVector.Set<FScreenProbeTileClassificationMarkCS::FSupportBackfaceDiffuse>(bSupportBackfaceDiffuse);
				PermutationVector.Set<FScreenProbeTileClassificationMarkCS::FOutputDownsampledDepth>(LumenScreenProbeGather::GetIntegrateDownsampleFactor(View) != 1 && !ScreenSpaceBentNormalParameters.DownsampledSceneDepth);
				PermutationVector.Set<FScreenProbeTileClassificationMarkCS::FIntegrateDownsampleFactor>(IntegrateDownsampleFactor);
				PermutationVector = FScreenProbeTileClassificationMarkCS::RemapPermutation(PermutationVector);
				auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeTileClassificationMarkCS>(PermutationVector);

				if (bOverflow)
				{
					PassParameters->TileIndirectBuffer = View.SubstrateViewData.ClosureTileDispatchIndirectBuffer;
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("TileClassificationMark(Overflow)"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						View.SubstrateViewData.ClosureTileDispatchIndirectBuffer,
						Substrate::GetClosureTileIndirectArgsOffset(IntegrateDownsampleFactor));
				}
				else
				{
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("TileClassificationMark"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						FIntVector(ViewportIntegrateTileDimensions.X, ViewportIntegrateTileDimensions.Y, 1));
				}
			};
		
			ScreenProbeTileClassificationMark(false);
			if (Lumen::SupportsMultipleClosureEvaluation(View))
			{
				ScreenProbeTileClassificationMark(true);
			}
		}

		// * Closure 0 is always present, and the max tile data count is TileClassificationDimensions.X x TileClassificationDimensions.Y
		// * Closures 1-N are optional. The number of tiles dependent on the max. closure count per pixel, and are multiplied by TileClassificationDimensions.X x TileClassificationDimensions.Y.
		// For each integration techniques, we preallocate a convervative number of tile count, to ensure there is no overflow.
		const uint32 MaxClosurePerPixel = Substrate::GetSubstrateMaxClosureCount(View);
		const uint32 TileDataCount_Closure0  = TileClassificationBufferDimensions.X * TileClassificationBufferDimensions.Y * (uint32)EScreenProbeIntegrateTileClassification::Num;
		const uint32 TileDataCount_Closure1N = TileClassificationBufferDimensions.X * TileClassificationBufferDimensions.Y * (uint32)EScreenProbeIntegrateTileClassification::Num * (MaxClosurePerPixel-1u);
		FRDGBufferRef IntegrateTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileDataCount_Closure0 + TileDataCount_Closure1N), TEXT("Lumen.ScreenProbeGather.IntegrateTileData"));
		{
			FRDGBufferUAVRef RWIntegrateIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(IntegrateIndirectArgs, PF_R32_UINT), ERDGUnorderedAccessViewFlags::SkipBarrier );
			FRDGBufferUAVRef RWIntegrateTileData = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(IntegrateTileData), ERDGUnorderedAccessViewFlags::SkipBarrier);

			auto ScreenProbeTileClassificationBuildLists = [&](bool bOverflow)
			{
				FScreenProbeTileClassificationBuildListsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeTileClassificationBuildListsCS::FParameters>();
				PassParameters->RWIntegrateIndirectArgs = RWIntegrateIndirectArgs;
				PassParameters->RWIntegrateTileData = RWIntegrateTileData;
				PassParameters->TileClassificationModes = TileClassificationModes;
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
				PassParameters->ViewportTileDimensions = ViewportIntegrateTileDimensions;
				PassParameters->ViewportTileDimensionsWithOverflow = TileClassificationBufferDimensions;
				PassParameters->MaxClosurePerPixel = MaxClosurePerPixel;

				FScreenProbeTileClassificationBuildListsCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FScreenProbeTileClassificationBuildListsCS::FOverflowTile>(bOverflow);
				auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeTileClassificationBuildListsCS>(PermutationVector);

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
						FComputeShaderUtils::GetGroupCount(ViewportIntegrateTileDimensions, 8));
				}
			};
		
			ScreenProbeTileClassificationBuildLists(false);
			if (Lumen::SupportsMultipleClosureEvaluation(View))
			{
				ScreenProbeTileClassificationBuildLists(true);
			}
		}

		// Allow integration passes to overlap
		FRDGTextureUAVRef DiffuseIndirectUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef LightIsMovingUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(LightIsMoving), ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef BackfaceDiffuseIndirectUAV = bSupportBackfaceDiffuse ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(BackfaceDiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
		FRDGTextureUAVRef RoughSpecularIndirectUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RoughSpecularIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);

		for (uint32 ClassificationMode = 0; ClassificationMode < (uint32)EScreenProbeIntegrateTileClassification::Num; ClassificationMode++)
		{
			auto ScreenProbeIntegrate = [&](bool bOverflow)
			{
				FScreenProbeIntegrateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeIntegrateCS::FParameters>();
				PassParameters->RWDiffuseIndirect = DiffuseIndirectUAV;
				PassParameters->RWLightIsMoving = LightIsMovingUAV;
				PassParameters->RWBackfaceDiffuseIndirect = BackfaceDiffuseIndirectUAV;
				PassParameters->RWRoughSpecularIndirect = RoughSpecularIndirectUAV;
				PassParameters->IntegrateTileData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(IntegrateTileData));
				PassParameters->GatherParameters = GatherParameters;
				PassParameters->ScreenProbeIntegrateParameters = IntegrateParameters;
				PassParameters->ScreenProbeParameters = ScreenProbeParameters;
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
				PassParameters->FullResolutionJitterWidth = LumenScreenProbeGather::GetScreenProbeFullResolutionJitterWidth(View);
				PassParameters->ReflectionsCompositeParameters = ReflectionsCompositeParameters;
				PassParameters->MaxRoughnessToEvaluateRoughSpecular = GVarLumenScreenProbeMaxRoughnessToEvaluateRoughSpecular.GetValueOnRenderThread();
				PassParameters->MaxRoughnessToEvaluateRoughSpecularForFoliage = GVarLumenScreenProbeMaxRoughnessToEvaluateRoughSpecularForFoliage.GetValueOnRenderThread();
				PassParameters->ApplyMaterialAO = GLumenScreenProbeMaterialAO;
				PassParameters->MaxAOMultibounceAlbedo = GLumenMaxShortRangeAOMultibounceAlbedo;
				PassParameters->LumenReflectionInputIsSSR = bSSREnabled ? 1 : 0;
				PassParameters->ScreenSpaceBentNormalParameters = ScreenSpaceBentNormalParameters;
				PassParameters->DefaultDiffuseIntegrationMethod = (uint32)LumenScreenProbeGather::GetDiffuseIntegralMethod();
				PassParameters->ViewportTileDimensions = ViewportIntegrateTileDimensions;
				PassParameters->ViewportTileDimensionsWithOverflow = TileClassificationBufferDimensions;
				PassParameters->IndirectArgs = IntegrateIndirectArgs;
				PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
				PassParameters->MaxClosurePerPixel = MaxClosurePerPixel;

				FScreenProbeIntegrateCS::FPermutationDomain PermutationVector;
				PermutationVector.Set< FScreenProbeIntegrateCS::FOverflowTile >(bOverflow);
				PermutationVector.Set< FScreenProbeIntegrateCS::FTileClassificationMode >(ClassificationMode);
				PermutationVector.Set< FScreenProbeIntegrateCS::FShortRangeAO >(bApplyShortRangeAO);
				PermutationVector.Set< FScreenProbeIntegrateCS::FProbeIrradianceFormat >(LumenScreenProbeGather::GetScreenProbeIrradianceFormat(View.Family->EngineShowFlags));
				PermutationVector.Set< FScreenProbeIntegrateCS::FStochasticProbeInterpolation >(GLumenScreenProbeStochasticInterpolation != 0);
				PermutationVector.Set< FScreenProbeIntegrateCS::FSupportBackfaceDiffuse >(bSupportBackfaceDiffuse);
				PermutationVector.Set< FScreenProbeIntegrateCS::FRoughSpecularSamplingMode >(RoughSpecularSamplingMode);
				PermutationVector.Set< FScreenProbeIntegrateCS::FIntegrateDownsampleFactor >(IntegrateDownsampleFactor);
				PermutationVector.Set< FScreenProbeIntegrateCS::FScreenProbeExtraAO >(GatherParameters.ScreenProbeExtraAOWithBorder != nullptr);
				auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeIntegrateCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("%s%s DownsampleFactor:%d", 
						GetClassificationModeString((EScreenProbeIntegrateTileClassification)ClassificationMode),
						bOverflow ? TEXT("(Overflow)") : TEXT(""),
						IntegrateDownsampleFactor),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					IntegrateIndirectArgs,
					((bOverflow ? uint32(EScreenProbeIntegrateTileClassification::Num) : 0u) + ClassificationMode) * sizeof(FRHIDispatchIndirectParameters));
			};

			ScreenProbeIntegrate(false);
			if (Lumen::SupportsMultipleClosureEvaluation(View))
			{
				ScreenProbeIntegrate(true);
			}
		}

		// Debug pass
		if (GLumenScreenProbeTileDebugMode > 0)
		{
			AddLumenScreenProbeDebugPass(GraphBuilder, View, ViewportIntegrateTileDimensions, TileClassificationBufferDimensions, IntegrateTileData, IntegrateIndirectArgs);
		}
	}
	else // No tile classification
	{
		const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);

		auto ScreenProbeIntegrate = [&](bool bOverflow)
		{
			FScreenProbeIntegrateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeIntegrateCS::FParameters>();
			PassParameters->RWDiffuseIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DiffuseIndirect));
			PassParameters->RWLightIsMoving = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(LightIsMoving));
			PassParameters->RWBackfaceDiffuseIndirect = bSupportBackfaceDiffuse ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(BackfaceDiffuseIndirect)) : nullptr;
			PassParameters->RWRoughSpecularIndirect =  GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RoughSpecularIndirect));
			PassParameters->GatherParameters = GatherParameters;

			const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
			if (!PassParameters->GatherParameters.ScreenProbeRadianceSHAmbient)
			{
				PassParameters->GatherParameters.ScreenProbeRadianceSHAmbient = SystemTextures.Black;
				PassParameters->GatherParameters.ScreenProbeRadianceSHDirectional = SystemTextures.Black;
			}

			PassParameters->ScreenProbeParameters = ScreenProbeParameters;
			PassParameters->ScreenProbeIntegrateParameters = IntegrateParameters;
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
			PassParameters->FullResolutionJitterWidth = LumenScreenProbeGather::GetScreenProbeFullResolutionJitterWidth(View);
			PassParameters->ReflectionsCompositeParameters = ReflectionsCompositeParameters;
			PassParameters->MaxRoughnessToEvaluateRoughSpecular = GVarLumenScreenProbeMaxRoughnessToEvaluateRoughSpecular.GetValueOnRenderThread();
			PassParameters->MaxRoughnessToEvaluateRoughSpecularForFoliage = GVarLumenScreenProbeMaxRoughnessToEvaluateRoughSpecularForFoliage.GetValueOnRenderThread();
			PassParameters->ApplyMaterialAO = GLumenScreenProbeMaterialAO;
			PassParameters->MaxAOMultibounceAlbedo = GLumenMaxShortRangeAOMultibounceAlbedo;
			PassParameters->ScreenSpaceBentNormalParameters = ScreenSpaceBentNormalParameters;
			PassParameters->DefaultDiffuseIntegrationMethod = (uint32)LumenScreenProbeGather::GetDiffuseIntegralMethod();
			PassParameters->ViewportTileDimensions = FIntPoint(0, 0);
			PassParameters->ViewportTileDimensionsWithOverflow = FIntPoint(0, 0);
			PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

			FScreenProbeIntegrateCS::FPermutationDomain PermutationVector;
			PermutationVector.Set< FScreenProbeIntegrateCS::FOverflowTile >(bOverflow);
			PermutationVector.Set< FScreenProbeIntegrateCS::FTileClassificationMode >((uint32)EScreenProbeIntegrateTileClassification::Num);
			PermutationVector.Set< FScreenProbeIntegrateCS::FShortRangeAO >(bApplyShortRangeAO);
			PermutationVector.Set< FScreenProbeIntegrateCS::FProbeIrradianceFormat >(LumenScreenProbeGather::GetScreenProbeIrradianceFormat(View.Family->EngineShowFlags));
			PermutationVector.Set< FScreenProbeIntegrateCS::FStochasticProbeInterpolation >(GLumenScreenProbeStochasticInterpolation != 0);
			PermutationVector.Set< FScreenProbeIntegrateCS::FSupportBackfaceDiffuse >(bSupportBackfaceDiffuse);
			PermutationVector.Set< FScreenProbeIntegrateCS::FRoughSpecularSamplingMode >(RoughSpecularSamplingMode);
			PermutationVector.Set< FScreenProbeIntegrateCS::FIntegrateDownsampleFactor >(IntegrateDownsampleFactor);
			PermutationVector.Set< FScreenProbeIntegrateCS::FScreenProbeExtraAO >(GatherParameters.ScreenProbeExtraAOWithBorder != nullptr);
			auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeIntegrateCS>(PermutationVector);

			const FIntPoint DispatchViewRect = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), IntegrateDownsampleFactor);
			FIntVector DispatchCount = FComputeShaderUtils::GetGroupCount(DispatchViewRect, GScreenProbeIntegrateTileSize);
			DispatchCount.Z = ClosureCount;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Integrate%s DownsampleFactor:%d", bOverflow ? TEXT("(Overflow)") : TEXT(""), IntegrateDownsampleFactor),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				DispatchCount);
		};

		ScreenProbeIntegrate(false);
		if (Lumen::SupportsMultipleClosureEvaluation(View))
		{
			ScreenProbeIntegrate(true);
		}

	}
}

void UpdateHistoryScreenProbeGather(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, 
	const FSceneTextures& SceneTextures,
	FLumenSceneFrameTemporaries& FrameTemporaries,
	const FScreenProbeGatherCommonParameters& ScreenProbeGatherCommonParameters,
	const FScreenProbeIntegrateParameters& IntegrateParameters,
	FLumenScreenSpaceBentNormalParameters& ScreenSpaceBentNormalParameters,
	bool bPropagateGlobalLightingChange,
	FRDGTextureRef& DiffuseIndirect,
	FRDGTextureRef LightIsMoving,
	FRDGTextureRef& BackfaceDiffuseIndirect,
	FRDGTextureRef& RoughSpecularIndirect,
	ERDGPassFlags ComputePassFlags)
{
	LLM_SCOPE_BYTAG(Lumen);
	
	const bool bSupportBackfaceDiffuse = BackfaceDiffuseIndirect != nullptr;
	const bool bSupportShortRangeAO = ScreenSpaceBentNormalParameters.ShortRangeAOMode != 0 && LumenShortRangeAO::UseTemporal();
	const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);
	const EPixelFormat LightingDataFormat = Lumen::GetLightingDataFormat();
	const EPixelFormat ShortRangeAOFormat = LumenShortRangeAO::GetTextureFormat();

	bool bOverflowTileHistoryValid = false;
	FIntPoint HistoryEffectiveResolution(0, 0);
	FVector4f DiffuseIndirectHistoryScreenPositionScaleBias(1.0f, 1.0f, 0.0f, 0.0f);
	FIntRect DiffuseIndirectHistoryViewRect(0, 0, 0, 0);
	FVector4f HistoryBufferSizeAndInvSize(0, 0, 0, 0);
	FRDGTextureRef OldDepthHistory = nullptr;
	FRDGTextureRef OldNormalHistory = nullptr;
	FRDGTextureRef OldDiffuseIndirectHistory = nullptr;
	FRDGTextureRef OldBackfaceDiffuseIndirectHistory = nullptr;
	FRDGTextureRef OldRoughSpecularIndirectHistory = nullptr;
	FRDGTextureRef OldFastUpdateMode_NumFramesAccumulatedHistory = nullptr;
	FRDGTextureRef OldShortRangeAOHistory = nullptr;

	// Load last frame's history
	if (View.ViewState)
	{
		ensureMsgf(SceneTextures.Velocity->Desc.Format != PF_G16R16, TEXT("Lumen requires 3d velocity.  Update Velocity format code."));

		const FScreenProbeGatherTemporalState& ScreenProbeGatherState = View.ViewState->Lumen.ScreenProbeGatherState;
		if (GLumenScreenProbeTemporalFilter != 0
			&& !View.bCameraCut	
			&& !View.bPrevTransformsReset
			&& !GLumenScreenProbeClearHistoryEveryFrame
			&& ScreenProbeGatherState.LumenGatherCvars == GLumenGatherCvars
			&& !bPropagateGlobalLightingChange
			&& (ScreenProbeGatherState.RoughSpecularIndirectHistoryRT && LightingDataFormat == ScreenProbeGatherState.RoughSpecularIndirectHistoryRT->GetDesc().Format)
			&& (!bSupportBackfaceDiffuse || ScreenProbeGatherState.BackfaceDiffuseIndirectHistoryRT)
			&& (!bSupportShortRangeAO || (ScreenProbeGatherState.ShortRangeAOHistoryRT && ScreenProbeGatherState.ShortRangeAOHistoryRT->GetDesc().Format == ShortRangeAOFormat)))
		{
			OldDiffuseIndirectHistory = GraphBuilder.RegisterExternalTexture(ScreenProbeGatherState.DiffuseIndirectHistoryRT);
			OldBackfaceDiffuseIndirectHistory = bSupportBackfaceDiffuse ? GraphBuilder.RegisterExternalTexture(ScreenProbeGatherState.BackfaceDiffuseIndirectHistoryRT) : nullptr;

			OldDepthHistory = View.ViewState->StochasticLighting.SceneDepthHistory ? GraphBuilder.RegisterExternalTexture(View.ViewState->StochasticLighting.SceneDepthHistory) : nullptr;
			OldNormalHistory = View.ViewState->StochasticLighting.SceneNormalHistory ? GraphBuilder.RegisterExternalTexture(View.ViewState->StochasticLighting.SceneNormalHistory) : nullptr;

			OldRoughSpecularIndirectHistory = GraphBuilder.RegisterExternalTexture(ScreenProbeGatherState.RoughSpecularIndirectHistoryRT);
			OldFastUpdateMode_NumFramesAccumulatedHistory = GraphBuilder.RegisterExternalTexture(ScreenProbeGatherState.FastUpdateMode_NumFramesAccumulatedHistoryRT);
			OldShortRangeAOHistory = ScreenProbeGatherState.ShortRangeAOHistoryRT ? GraphBuilder.RegisterExternalTexture(ScreenProbeGatherState.ShortRangeAOHistoryRT) : nullptr;

			bOverflowTileHistoryValid = Substrate::IsSubstrateEnabled() ? ClosureCount == ScreenProbeGatherState.HistorySubstrateMaxClosureCount : true;
			HistoryEffectiveResolution = ScreenProbeGatherState.HistoryEffectiveResolution;
			DiffuseIndirectHistoryScreenPositionScaleBias = ScreenProbeGatherState.DiffuseIndirectHistoryScreenPositionScaleBias;
			DiffuseIndirectHistoryViewRect = ScreenProbeGatherState.DiffuseIndirectHistoryViewRect;
			HistoryBufferSizeAndInvSize = ScreenProbeGatherState.HistoryBufferSizeAndInvSize;
		}
	}

	const bool bRejectBasedOnNormal = LumenScreenProbeGather::UseRejectBasedOnNormal() && OldNormalHistory;
	
	// If the scene render targets reallocate, toss the history so we don't read uninitialized data
	const FIntPoint EffectiveResolution = Substrate::GetSubstrateTextureResolution(View, SceneTextures.Config.Extent);
	const FIntPoint EffectiveViewExtent = FrameTemporaries.ViewExtent;
	const FIntRect NewHistoryViewRect = View.ViewRect;

	FRDGTextureDesc DiffuseIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, LightingDataFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
	FRDGTextureDesc RoughSpecularIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, LightingDataFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);

	FRDGTextureRef NewDiffuseIndirect = FrameTemporaries.NewDiffuseIndirect.CreateSharedRT(GraphBuilder, DiffuseIndirectDesc, EffectiveViewExtent, TEXT("Lumen.ScreenProbeGather.DiffuseIndirect"));
	FRDGTextureRef NewBackfaceDiffuseIndirect = bSupportBackfaceDiffuse ? FrameTemporaries.NewBackfaceDiffuseIndirect.CreateSharedRT(GraphBuilder, RoughSpecularIndirectDesc, EffectiveViewExtent, TEXT("Lumen.ScreenProbeGather.BackfaceDiffuseIndirect")) : nullptr;
	FRDGTextureRef NewRoughSpecularIndirect = FrameTemporaries.NewRoughSpecularIndirect.CreateSharedRT(GraphBuilder, RoughSpecularIndirectDesc, EffectiveViewExtent, TEXT("Lumen.ScreenProbeGather.RoughSpecularIndirect"));

	FRDGTextureRef NewShortRangeAO = nullptr;
	if (bSupportShortRangeAO)
	{
		NewShortRangeAO = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2DArray(EffectiveResolution, ShortRangeAOFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount),
			TEXT("Lumen.ScreenProbeGather.ShortRangeAO"));
	}

	FRDGTextureDesc HistoryFastUpdateMode_NumFramesAccumulatedDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_R8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
	FRDGTextureRef NewHistoryFastUpdateMode_NumFramesAccumulated = FrameTemporaries.NewHistoryFastUpdateMode_NumFramesAccumulated.CreateSharedRT(GraphBuilder, HistoryFastUpdateMode_NumFramesAccumulatedDesc, EffectiveViewExtent, TEXT("Lumen.ScreenProbeGather.HistoryFastUpdateMode_NumFramesAccumulated"));

	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	TUniformBufferRef<FBlueNoise> BlueNoiseUniformBuffer = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

	FRDGTextureUAVRef RWNewHistoryDiffuseIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewDiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
	FRDGTextureUAVRef RWNewHistoryBackfaceDiffuseIndirect = bSupportBackfaceDiffuse ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewBackfaceDiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
	FRDGTextureUAVRef RWNewHistoryRoughSpecularIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewRoughSpecularIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
	FRDGTextureUAVRef RWHistoryFastUpdateMode_NumFramesAccumulated = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewHistoryFastUpdateMode_NumFramesAccumulated), ERDGUnorderedAccessViewFlags::SkipBarrier);
	FRDGTextureUAVRef RWNewShortRangeAO = NewShortRangeAO ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewShortRangeAO), ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;

	auto ScreenProbeTemporalReprojection = [&](bool bOverflow)
	{
		FScreenProbeTemporalReprojectionCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FValidHistory >(OldDiffuseIndirectHistory != nullptr && OldDepthHistory != nullptr);
		PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FOverflowTile>(bOverflow);
		PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FFastUpdateModeNeighborhoodClamp>(GLumenScreenProbeTemporalFastUpdateModeUseNeighborhoodClamp != 0);
		PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FHistoryRejectBasedOnNormal>(bRejectBasedOnNormal);
		PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FSupportBackfaceDiffuse >(bSupportBackfaceDiffuse);
		PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FIntegrateDownsampleFactor >(LumenScreenProbeGather::GetIntegrateDownsampleFactor(View));
		PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FShortRangeAOMode >(bSupportShortRangeAO ? (LumenShortRangeAO::UseBentNormal() ? 2 : 1) : 0);
		PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FShortRangeAODownsampleFactor >(LumenShortRangeAO::GetDownsampleFactor());
		PermutationVector = FScreenProbeTemporalReprojectionCS::RemapPermutation(PermutationVector);
		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeTemporalReprojectionCS>(PermutationVector);

		FScreenProbeTemporalReprojectionCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeTemporalReprojectionCS::FParameters>();
		PassParameters->RWNewHistoryDiffuseIndirect = RWNewHistoryDiffuseIndirect;
		PassParameters->RWNewHistoryBackfaceDiffuseIndirect = RWNewHistoryBackfaceDiffuseIndirect;
		PassParameters->RWNewHistoryRoughSpecularIndirect = RWNewHistoryRoughSpecularIndirect;
		PassParameters->RWNewHistoryFastUpdateMode_NumFramesAccumulated = RWHistoryFastUpdateMode_NumFramesAccumulated;
		PassParameters->RWNewHistoryShortRangeAO = RWNewShortRangeAO;

		PassParameters->ScreenProbeGatherCommonParameters = ScreenProbeGatherCommonParameters;
		PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->ScreenProbeIntegrateParameters = IntegrateParameters;
		PassParameters->BlueNoise = BlueNoiseUniformBuffer;

		PassParameters->DiffuseIndirectHistory = OldDiffuseIndirectHistory;
		PassParameters->BackfaceDiffuseIndirectHistory = OldBackfaceDiffuseIndirectHistory;
		PassParameters->RoughSpecularIndirectHistory = OldRoughSpecularIndirectHistory;
		PassParameters->HistoryFastUpdateMode_NumFramesAccumulated = OldFastUpdateMode_NumFramesAccumulatedHistory;
		PassParameters->ShortRangeAOTexture = ScreenSpaceBentNormalParameters.ShortRangeAOTexture;
		PassParameters->DiffuseIndirectDepthHistory = OldDepthHistory;
		PassParameters->DiffuseIndirectNormalHistory = OldNormalHistory;
		PassParameters->ShortRangeAOHistory = OldShortRangeAOHistory;

		PassParameters->HistoryDistanceThreshold = CVarLumenScreenProbeHistoryDistanceThreshold.GetValueOnRenderThread();
		PassParameters->HistoryDistanceThresholdForFoliage = CVarLumenScreenProbeHistoryDistanceThresholdForFoliage.GetValueOnRenderThread();
		PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
		PassParameters->InvFractionOfLightingMovingForFastUpdateMode = 1.0f / FMath::Max(GLumenScreenProbeFractionOfLightingMovingForFastUpdateMode, .001f);
		PassParameters->MaxFastUpdateModeAmount = GLumenScreenProbeTemporalMaxFastUpdateModeAmount;
		PassParameters->bIsSubstrateTileHistoryValid = bOverflowTileHistoryValid ? 1u : 0u;

		PassParameters->ShortRangeAOViewMin = ScreenSpaceBentNormalParameters.ShortRangeAOViewMin;
		PassParameters->ShortRangeAOViewSize = ScreenSpaceBentNormalParameters.ShortRangeAOViewSize;
		PassParameters->ShortRangeAOTemporalNeighborhoodClampScale = LumenShortRangeAO::GetTemporalNeighborhoodClampScale();

		const float MaxFramesAccumulatedScale = 1.0f / FMath::Sqrt(FMath::Clamp(View.FinalPostProcessSettings.LumenFinalGatherLightingUpdateSpeed, .5f, 8.0f));
		const float EditingScale = View.Family->bCurrentlyBeingEdited ? .5f : 1.0f;
		PassParameters->MaxFramesAccumulated = FMath::RoundToInt(GLumenScreenProbeTemporalMaxFramesAccumulated * MaxFramesAccumulatedScale * EditingScale);
		PassParameters->HistoryNormalCosThreshold = FMath::Cos(GLumenScreenProbeTemporalHistoryNormalThreshold * (float)PI / 180.0f);
		PassParameters->HistoryScreenPositionScaleBias = DiffuseIndirectHistoryScreenPositionScaleBias;

		const FVector2f HistoryUVToScreenPositionScale(1.0f / PassParameters->HistoryScreenPositionScaleBias.X, 1.0f / PassParameters->HistoryScreenPositionScaleBias.Y);
		const FVector2f HistoryUVToScreenPositionBias = -FVector2f(PassParameters->HistoryScreenPositionScaleBias.W, PassParameters->HistoryScreenPositionScaleBias.Z) * HistoryUVToScreenPositionScale;
		PassParameters->HistoryUVToScreenPositionScaleBias = FVector4f(HistoryUVToScreenPositionScale, HistoryUVToScreenPositionBias);

		// Pull in the max UV to exclude the region which will read outside the viewport due to bilinear filtering
		PassParameters->HistoryUVMinMax = FVector4f(
			(DiffuseIndirectHistoryViewRect.Min.X + 0.5f) * HistoryBufferSizeAndInvSize.Z,
			(DiffuseIndirectHistoryViewRect.Min.Y + 0.5f) * HistoryBufferSizeAndInvSize.W,
			(DiffuseIndirectHistoryViewRect.Max.X - 1.0f) * HistoryBufferSizeAndInvSize.Z,
			(DiffuseIndirectHistoryViewRect.Max.Y - 1.0f) * HistoryBufferSizeAndInvSize.W);

		PassParameters->HistoryViewportMinMax = FIntVector4(
			DiffuseIndirectHistoryViewRect.Min.X,
			DiffuseIndirectHistoryViewRect.Min.Y,
			DiffuseIndirectHistoryViewRect.Max.X,
			DiffuseIndirectHistoryViewRect.Max.Y);

		PassParameters->HistoryBufferSizeAndInvSize = HistoryBufferSizeAndInvSize;
		PassParameters->DiffuseIndirect = DiffuseIndirect;
		PassParameters->LightIsMoving = LightIsMoving;
		PassParameters->BackfaceDiffuseIndirect = BackfaceDiffuseIndirect;
		PassParameters->RoughSpecularIndirect = RoughSpecularIndirect;
		PassParameters->TargetFormatQuantizationError = Lumen::GetLightingQuantizationError();

		// SUBSTRATE_TODO: Reenable once history tracking is correct
		#if 0
		if (bOverflow)
		{
			PassParameters->TileIndirectBuffer = View.SubstrateViewData.ClosureTileDispatchIndirectBuffer;
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TemporalReprojection(Overflow)"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				View.SubstrateViewData.ClosureTileDispatchIndirectBuffer,
				Substrate::GetClosureTileIndirectArgsOffset(1u /*DownsampleFactor*/));
		}
		else
		#endif
		{
			FIntVector DispatchCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FScreenProbeTemporalReprojectionCS::GetGroupSize());
			DispatchCount.Z = Lumen::SupportsMultipleClosureEvaluation(View) ? ClosureCount : 1u;
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TemporalReprojection(%ux%u)", View.ViewRect.Width(), View.ViewRect.Height()),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				DispatchCount);
		}
	};

	ScreenProbeTemporalReprojection(false);
	// SUBSTRATE_TODO: Reenable once history tracking is correct
	#if 0
	if (SupportsMultipleClosureEvaluation(View))
	{
		ScreenProbeTemporalReprojection(true);
	}
	#endif

	// Store history for the next frame
	if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
	{
		FScreenProbeGatherTemporalState& ScreenProbeGatherState = View.ViewState->Lumen.ScreenProbeGatherState;

		ScreenProbeGatherState.DiffuseIndirectHistoryRT = nullptr;
		ScreenProbeGatherState.BackfaceDiffuseIndirectHistoryRT = nullptr;
		ScreenProbeGatherState.RoughSpecularIndirectHistoryRT = nullptr;
		ScreenProbeGatherState.FastUpdateMode_NumFramesAccumulatedHistoryRT = nullptr;

		ScreenProbeGatherState.DiffuseIndirectHistoryViewRect = NewHistoryViewRect;
		ScreenProbeGatherState.DiffuseIndirectHistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(SceneTextures.Config.Extent, View.ViewRect);
		ScreenProbeGatherState.LumenGatherCvars = GLumenGatherCvars;
		ScreenProbeGatherState.HistoryEffectiveResolution = EffectiveResolution;
		ScreenProbeGatherState.HistorySubstrateMaxClosureCount = ClosureCount;

		ScreenProbeGatherState.HistoryBufferSizeAndInvSize = FVector4f(
			SceneTextures.Config.Extent.X,
			SceneTextures.Config.Extent.Y,
			1.0f / SceneTextures.Config.Extent.X,
			1.0f / SceneTextures.Config.Extent.Y);

		if (GLumenScreenProbeTemporalFilter != 0)
		{
			// Queue updating the view state's render target reference with the new history
			GraphBuilder.QueueTextureExtraction(NewDiffuseIndirect, &ScreenProbeGatherState.DiffuseIndirectHistoryRT);

			if (bSupportBackfaceDiffuse)
			{
				GraphBuilder.QueueTextureExtraction(NewBackfaceDiffuseIndirect, &ScreenProbeGatherState.BackfaceDiffuseIndirectHistoryRT);
			}

			GraphBuilder.QueueTextureExtraction(NewRoughSpecularIndirect, &ScreenProbeGatherState.RoughSpecularIndirectHistoryRT);
			GraphBuilder.QueueTextureExtraction(NewHistoryFastUpdateMode_NumFramesAccumulated, &ScreenProbeGatherState.FastUpdateMode_NumFramesAccumulatedHistoryRT);

			if (bSupportShortRangeAO)
			{
				GraphBuilder.QueueTextureExtraction(NewShortRangeAO, &ScreenProbeGatherState.ShortRangeAOHistoryRT);
			}
			else
			{
				ScreenProbeGatherState.ShortRangeAOHistoryRT = nullptr;
			}
		}
	}

	RoughSpecularIndirect = NewRoughSpecularIndirect;
	DiffuseIndirect = NewDiffuseIndirect;
	BackfaceDiffuseIndirect = NewBackfaceDiffuseIndirect;

	if (bSupportShortRangeAO)
	{
		ScreenSpaceBentNormalParameters.ShortRangeAOTexture = NewShortRangeAO;
	}
}

static void ScreenGatherMarkUsedProbes(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	FScreenProbeParameters& ScreenProbeParameters,
	const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters,
	ERDGPassFlags ComputePassFlags)
{
	FMarkRadianceProbesUsedByScreenProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarkRadianceProbesUsedByScreenProbesCS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
	PassParameters->ScreenProbeParameters = ScreenProbeParameters;
	PassParameters->RadianceCacheMarkParameters = RadianceCacheMarkParameters;

	auto ComputeShader = View.ShaderMap->GetShader<FMarkRadianceProbesUsedByScreenProbesCS>(0);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MarkRadianceProbes(ScreenProbes) %ux%u", PassParameters->ScreenProbeParameters.ScreenProbeAtlasViewSize.X, PassParameters->ScreenProbeParameters.ScreenProbeAtlasViewSize.Y),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		PassParameters->ScreenProbeParameters.ProbeIndirectArgs,
		(uint32)EScreenProbeIndirectArgs::ThreadPerProbe * sizeof(FRHIDispatchIndirectParameters));
}

static void HairStrandsMarkUsedProbes(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters,
	ERDGPassFlags ComputePassFlags)
{
	check(View.HairStrandsViewData.VisibilityData.TileData.IsValid());
	const uint32 TileMip = 3u; // 8x8 tiles
	const int32 TileSize = 1u<<TileMip;
	const FIntPoint Resolution(View.ViewRect.Width(), View.ViewRect.Height());
	const FIntPoint TileResolution = FIntPoint(
		FMath::DivideAndRoundUp(Resolution.X, TileSize), 
		FMath::DivideAndRoundUp(Resolution.Y, TileSize));

	FMarkRadianceProbesUsedByHairStrandsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarkRadianceProbesUsedByHairStrandsCS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->HairStrandsResolution = TileResolution;
	PassParameters->HairStrandsInvResolution = FVector2f(1.f / float(TileResolution.X), 1.f / float(TileResolution.Y));
	PassParameters->HairStrandsMip = TileMip;
	PassParameters->HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
	PassParameters->RadianceCacheMarkParameters = RadianceCacheMarkParameters;
	PassParameters->IndirectBufferArgs = View.HairStrandsViewData.VisibilityData.TileData.TilePerThreadIndirectDispatchBuffer;

	auto ComputeShader = View.ShaderMap->GetShader<FMarkRadianceProbesUsedByHairStrandsCS>();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MarkRadianceProbes(HairStrands,Tile)"),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		View.HairStrandsViewData.VisibilityData.TileData.TilePerThreadIndirectDispatchBuffer,
		0);
}

DECLARE_GPU_STAT(LumenScreenProbeGather);


FSSDSignalTextures FDeferredShadingSceneRenderer::RenderLumenFinalGather(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	FLumenSceneFrameTemporaries& FrameTemporaries,
	FRDGTextureRef LightingChannelsTexture,
	FViewInfo& View,
	FPreviousViewInfo* PreviousViewInfos,
	FLumenMeshSDFGridParameters& MeshSDFGridParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	FLumenScreenSpaceBentNormalParameters& ScreenSpaceBentNormalParameters,
	ERDGPassFlags ComputePassFlags)
{
	LLM_SCOPE_BYTAG(Lumen);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
	ScreenSpaceBentNormalParameters.ShortRangeAOMode = 0;
	ScreenSpaceBentNormalParameters.ShortRangeAOTexture = SystemTextures.Black;
	RadianceCacheParameters.RadianceProbeIndirectionTexture = nullptr;

	FSSDSignalTextures Outputs;
	LumenRadianceCache::FRadianceCacheInterpolationParameters TranslucencyVolumeRadianceCacheParameters;

	if (GLumenIrradianceFieldGather != 0)
	{
		Outputs = RenderLumenIrradianceFieldGather(GraphBuilder, SceneTextures, FrameTemporaries, View, TranslucencyVolumeRadianceCacheParameters, ComputePassFlags);
	}
	else if (Lumen::UseReSTIRGather(*View.Family, ShaderPlatform))
	{
		Outputs = RenderLumenReSTIRGather(
			GraphBuilder,
			SceneTextures,
			FrameTemporaries,
			LightingChannelsTexture,
			View,
			PreviousViewInfos,
			ComputePassFlags,
			ScreenSpaceBentNormalParameters);
	}
	else
	{
		Outputs = RenderLumenScreenProbeGather(
			GraphBuilder, 
			SceneTextures, 
			FrameTemporaries, 
			LightingChannelsTexture, 
			View, 
			PreviousViewInfos, 
			MeshSDFGridParameters, 
			RadianceCacheParameters, 
			ScreenSpaceBentNormalParameters,
			TranslucencyVolumeRadianceCacheParameters,
			ComputePassFlags);
	}

	ComputeLumenTranslucencyGIVolume(GraphBuilder, View, FrameTemporaries, TranslucencyVolumeRadianceCacheParameters, ComputePassFlags);

	return Outputs;
}

FSSDSignalTextures FDeferredShadingSceneRenderer::RenderLumenScreenProbeGather(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	FLumenSceneFrameTemporaries& FrameTemporaries,
	FRDGTextureRef LightingChannelsTexture,
	FViewInfo& View,
	FPreviousViewInfo* PreviousViewInfos,
	FLumenMeshSDFGridParameters& MeshSDFGridParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	FLumenScreenSpaceBentNormalParameters& ScreenSpaceBentNormalParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters& TranslucencyVolumeRadianceCacheParameters,
	ERDGPassFlags ComputePassFlags)
{
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	RDG_EVENT_SCOPE_STAT(GraphBuilder, LumenScreenProbeGather, "LumenScreenProbeGather");
	RDG_GPU_STAT_SCOPE(GraphBuilder, LumenScreenProbeGather);

	check(ShouldRenderLumenDiffuseGI(Scene, View));

	if (!LightingChannelsTexture)
	{
		LightingChannelsTexture = SystemTextures.Black;
	}

	if (!GLumenScreenProbeGather)
	{
		FSSDSignalTextures ScreenSpaceDenoiserInputs;
		ScreenSpaceDenoiserInputs.Textures[0] = SystemTextures.Black;
		ScreenSpaceDenoiserInputs.Textures[1] = SystemTextures.Black;
		FRDGTextureDesc RoughSpecularIndirectDesc = FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
		ScreenSpaceDenoiserInputs.Textures[2] = GraphBuilder.CreateTexture(RoughSpecularIndirectDesc, TEXT("Lumen.ScreenProbeGather.RoughSpecularIndirect"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenSpaceDenoiserInputs.Textures[2])), FLinearColor::Black);
		return ScreenSpaceDenoiserInputs;
	}

	// Pull from uniform buffer to get fallback textures.
	const FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);

	FScreenProbeGatherCommonParameters ScreenProbeGatherCommonParameters;
	ScreenProbeGatherCommonParameters.View = View.ViewUniformBuffer;
	if (GVarLumenScreenProbeGatherDebug.GetValueOnRenderThread() != 0)
	{
		ShaderPrint::SetEnabled(true);
		ShaderPrint::RequestSpaceForLines(256 * 1024u);
		ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, ScreenProbeGatherCommonParameters.ShaderPrintUniformBuffer);
	}

	FScreenProbeParameters ScreenProbeParameters;
	ScreenProbeParameters.ScreenProbeTracingOctahedronResolution = LumenScreenProbeGather::GetTracingOctahedronResolution(View);
	ensureMsgf(ScreenProbeParameters.ScreenProbeTracingOctahedronResolution < (1 << 6) - 1, TEXT("Tracing resolution %u was larger than supported by PackRayInfo()"), ScreenProbeParameters.ScreenProbeTracingOctahedronResolution);
	ScreenProbeParameters.ScreenProbeGatherOctahedronResolution = LumenScreenProbeGather::GetGatherOctahedronResolution(ScreenProbeParameters.ScreenProbeTracingOctahedronResolution);
	ScreenProbeParameters.ScreenProbeGatherOctahedronResolutionWithBorder = ScreenProbeParameters.ScreenProbeGatherOctahedronResolution + 2 * (1 << (GLumenScreenProbeGatherNumMips - 1));
	ScreenProbeParameters.ScreenProbeDownsampleFactor = LumenScreenProbeGather::GetScreenDownsampleFactor(View, SceneTextures);

	ScreenProbeParameters.ScreenProbeViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), (int32)ScreenProbeParameters.ScreenProbeDownsampleFactor);
	ScreenProbeParameters.ScreenProbeAtlasViewSize = ScreenProbeParameters.ScreenProbeViewSize;
	ScreenProbeParameters.ScreenProbeAtlasViewSize.Y += FMath::TruncToInt(ScreenProbeParameters.ScreenProbeViewSize.Y * GLumenScreenProbeGatherAdaptiveProbeAllocationFraction);

	ScreenProbeParameters.ScreenProbeAtlasBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, (int32)ScreenProbeParameters.ScreenProbeDownsampleFactor);
	ScreenProbeParameters.ScreenProbeAtlasBufferSize.Y += FMath::TruncToInt(ScreenProbeParameters.ScreenProbeAtlasBufferSize.Y * GLumenScreenProbeGatherAdaptiveProbeAllocationFraction);

	ScreenProbeParameters.ScreenProbeGatherMaxMip = GLumenScreenProbeGatherNumMips - 1;
	ScreenProbeParameters.RelativeSpeedDifferenceToConsiderLightingMoving = GLumenScreenProbeRelativeSpeedDifferenceToConsiderLightingMoving;
	ScreenProbeParameters.ScreenTraceNoFallbackThicknessScale = (Lumen::UseHardwareRayTracedScreenProbeGather(ViewFamily) ? 1.0f : GLumenScreenProbeScreenTracesThicknessScaleWhenNoFallback) * View.ViewMatrices.GetPerProjectionDepthThicknessScale();
	ScreenProbeParameters.ExtraAOMaxDistanceWorldSpace = FMath::Clamp<float>(GLumenScreenProbeExtraAmbientOcclusionMaxDistanceWorldSpace, .0001f, 1000000.0f);
	ScreenProbeParameters.ExtraAOExponent = FMath::Clamp<float>(GLumenScreenProbeExtraAmbientOcclusionExponent, .01f, 100.0f);
	ScreenProbeParameters.ScreenProbeInterpolationDepthWeight = -200.0f * CVarLumenScreenProbeInterpolationDepthWeight.GetValueOnRenderThread();
	ScreenProbeParameters.ScreenProbeInterpolationDepthWeightForFoliage = -200.0f * CVarLumenScreenProbeInterpolationDepthWeightForFoliage.GetValueOnRenderThread();
	ScreenProbeParameters.NumUniformScreenProbes = ScreenProbeParameters.ScreenProbeViewSize.X * ScreenProbeParameters.ScreenProbeViewSize.Y;
	ScreenProbeParameters.MaxNumAdaptiveProbes = FMath::TruncToInt(ScreenProbeParameters.NumUniformScreenProbes * GLumenScreenProbeGatherAdaptiveProbeAllocationFraction);
	
	ScreenProbeParameters.FixedJitterIndex = GLumenScreenProbeFixedJitterIndex;
	if (ScreenProbeParameters.FixedJitterIndex < 0)
	{
		ScreenProbeParameters.FixedJitterIndex = CVarLumenScreenProbeFixedStateFrameIndex.GetValueOnRenderThread();
	}

	{
		FVector2f InvAtlasWithBorderBufferSize = FVector2f(1.0f) / (FVector2f(ScreenProbeParameters.ScreenProbeGatherOctahedronResolutionWithBorder) * FVector2f(ScreenProbeParameters.ScreenProbeAtlasBufferSize));
		ScreenProbeParameters.SampleRadianceProbeUVMul = FVector2f(ScreenProbeParameters.ScreenProbeGatherOctahedronResolution) * InvAtlasWithBorderBufferSize;
		ScreenProbeParameters.SampleRadianceProbeUVAdd = FMath::Exp2(ScreenProbeParameters.ScreenProbeGatherMaxMip) * InvAtlasWithBorderBufferSize;
		ScreenProbeParameters.SampleRadianceAtlasUVMul = FVector2f(ScreenProbeParameters.ScreenProbeGatherOctahedronResolutionWithBorder) * InvAtlasWithBorderBufferSize;
	}

	extern int32 GLumenScreenProbeGatherVisualizeTraces;
	// Automatically set a fixed jitter if we are visualizing, but don't override existing fixed jitter
	if (GLumenScreenProbeGatherVisualizeTraces != 0 && ScreenProbeParameters.FixedJitterIndex < 0)
	{
		ScreenProbeParameters.FixedJitterIndex = 6;
	}

	uint32 StateFrameIndex = View.ViewState ? View.ViewState->GetFrameIndex() : 0;
	if (ScreenProbeParameters.FixedJitterIndex >= 0)
	{
		StateFrameIndex = ScreenProbeParameters.FixedJitterIndex;
	}
	ScreenProbeParameters.ScreenProbeRayDirectionFrameIndex = StateFrameIndex % FMath::Max(CVarLumenScreenProbeTemporalMaxRayDirections.GetValueOnRenderThread(), 1);
	ScreenProbeParameters.bSupportsHairScreenTraces = SupportsHairScreenTraces() ? 1u : 0u;
	ScreenProbeParameters.TargetFormatQuantizationError = Lumen::GetLightingQuantizationError();

	FRDGTextureDesc DownsampledDepthDesc(FRDGTextureDesc::Create2D(ScreenProbeParameters.ScreenProbeAtlasBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.ScreenProbeSceneDepth = GraphBuilder.CreateTexture(DownsampledDepthDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeSceneDepth"));

	FRDGTextureDesc DownsampledNormalDesc(FRDGTextureDesc::Create2D(ScreenProbeParameters.ScreenProbeAtlasBufferSize, PF_R8G8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.ScreenProbeWorldNormal = GraphBuilder.CreateTexture(DownsampledNormalDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeWorldNormal"));

	FRDGTextureDesc DownsampledSpeedDesc(FRDGTextureDesc::Create2D(ScreenProbeParameters.ScreenProbeAtlasBufferSize, PF_R16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.ScreenProbeWorldSpeed = GraphBuilder.CreateTexture(DownsampledSpeedDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeWorldSpeed"));

	FRDGTextureDesc DownsampledWorldPositionDesc(FRDGTextureDesc::Create2D(ScreenProbeParameters.ScreenProbeAtlasBufferSize, PF_A32B32G32R32F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.ScreenProbeTranslatedWorldPosition = GraphBuilder.CreateTexture(DownsampledWorldPositionDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeTranslatedWorldPosition"));


	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	ScreenProbeParameters.BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);
	
	{
		FScreenProbeDownsampleDepthUniformCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeDownsampleDepthUniformCS::FParameters>();
		PassParameters->RWScreenProbeSceneDepth = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeSceneDepth));
		PassParameters->RWScreenProbeWorldNormal = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeWorldNormal));
		PassParameters->RWScreenProbeWorldSpeed = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeWorldSpeed));
		PassParameters->RWScreenProbeTranslatedWorldPosition = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeTranslatedWorldPosition));
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->SceneTextures = SceneTextureParameters;
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;

		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeDownsampleDepthUniformCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("UniformPlacement DownsampleFactor=%u", ScreenProbeParameters.ScreenProbeDownsampleFactor),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(ScreenProbeParameters.ScreenProbeViewSize, FScreenProbeDownsampleDepthUniformCS::GetGroupSize()));
	}

	FRDGBufferRef NumAdaptiveScreenProbes = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.ScreenProbeGather.NumAdaptiveScreenProbes"));
	FRDGBufferRef AdaptiveScreenProbeData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), FMath::Max<uint32>(ScreenProbeParameters.MaxNumAdaptiveProbes, 1)), TEXT("Lumen.ScreenProbeGather.AdaptiveScreenProbeData"));

	ScreenProbeParameters.NumAdaptiveScreenProbes = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(NumAdaptiveScreenProbes, PF_R32_UINT));
	ScreenProbeParameters.AdaptiveScreenProbeData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(AdaptiveScreenProbeData, PF_R32_UINT));

	const FIntPoint ScreenProbeViewportBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, (int32)ScreenProbeParameters.ScreenProbeDownsampleFactor);
	FRDGTextureDesc ScreenTileAdaptiveProbeHeaderDesc(FRDGTextureDesc::Create2D(ScreenProbeViewportBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_AtomicCompatible));
	FIntPoint ScreenTileAdaptiveProbeIndicesBufferSize = FIntPoint(ScreenProbeViewportBufferSize.X * ScreenProbeParameters.ScreenProbeDownsampleFactor, ScreenProbeViewportBufferSize.Y * ScreenProbeParameters.ScreenProbeDownsampleFactor);
	FRDGTextureDesc ScreenTileAdaptiveProbeIndicesDesc(FRDGTextureDesc::Create2D(ScreenTileAdaptiveProbeIndicesBufferSize, PF_R16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.ScreenTileAdaptiveProbeHeader = GraphBuilder.CreateTexture(ScreenTileAdaptiveProbeHeaderDesc, TEXT("Lumen.ScreenProbeGather.ScreenTileAdaptiveProbeHeader"));
	ScreenProbeParameters.ScreenTileAdaptiveProbeIndices = GraphBuilder.CreateTexture(ScreenTileAdaptiveProbeIndicesDesc, TEXT("Lumen.ScreenProbeGather.ScreenTileAdaptiveProbeIndices"));

	FUintVector4 ClearValues(0, 0, 0, 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenTileAdaptiveProbeHeader)), ClearValues, ComputePassFlags);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(NumAdaptiveScreenProbes), 0, ComputePassFlags);

	int32 NumAdaptiveProbes = FMath::Clamp(CVarLumenScreenProbeGatherNumAdaptiveProbes.GetValueOnRenderThread(), 0, 64);

	if (ScreenProbeParameters.MaxNumAdaptiveProbes > 0 && NumAdaptiveProbes > 0)
	{ 
		const FIntPoint NumSamplesPerUniformProbe2D = LumenScreenProbeGather::GetNumSamplesPerUniformProbe2D(NumAdaptiveProbes);
		const uint32 NumSamplesPerUniformProbe = NumSamplesPerUniformProbe2D.X * NumSamplesPerUniformProbe2D.Y;

		const FIntPoint AdaptiveProbePlacementMaskSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, (int32)ScreenProbeParameters.ScreenProbeDownsampleFactor);

		FRDGTextureRef AdaptiveProbePlacementMask = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(AdaptiveProbePlacementMaskSize, PF_R16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("Lumen.ScreenProbeGather.AdaptiveProbePlacementMask"));

		const FIntPoint NumUniformScreenProbes = FIntPoint::DivideAndRoundDown(View.ViewRect.Size(), (int32)ScreenProbeParameters.ScreenProbeDownsampleFactor);
		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(NumUniformScreenProbes * NumSamplesPerUniformProbe2D, FScreenProbeAdaptivePlacementMarkCS::GetGroupSize());

		// Mark probes to be placed
		{
			FScreenProbeAdaptivePlacementMarkCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeAdaptivePlacementMarkCS::FParameters>();
			PassParameters->RWAdaptiveProbePlacementMask = GraphBuilder.CreateUAV(AdaptiveProbePlacementMask);
			PassParameters->ScreenProbeGatherCommonParameters = ScreenProbeGatherCommonParameters;
			PassParameters->ScreenProbeParameters = ScreenProbeParameters;
			PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
			PassParameters->SceneTextures = SceneTextureParameters;
			PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
			PassParameters->ScreenProbeParameters = ScreenProbeParameters;

			FScreenProbeAdaptivePlacementMarkCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FScreenProbeAdaptivePlacementMarkCS::FNumSamplesPerUniformProbe>(NumSamplesPerUniformProbe);
			auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeAdaptivePlacementMarkCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("AdaptivePlacementMark %dx%d", NumSamplesPerUniformProbe2D.X, NumSamplesPerUniformProbe2D.Y),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				GroupCount);
		}

		// Spawn probes in previously marked locations
		{
			FScreenProbeAdaptivePlacementSpawnCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeAdaptivePlacementSpawnCS::FParameters>();
			PassParameters->RWScreenProbeSceneDepth = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeSceneDepth));
			PassParameters->RWScreenProbeWorldNormal = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeWorldNormal));
			PassParameters->RWScreenProbeWorldSpeed = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeWorldSpeed));
			PassParameters->RWScreenProbeTranslatedWorldPosition = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeTranslatedWorldPosition));
			PassParameters->RWNumAdaptiveScreenProbes = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(NumAdaptiveScreenProbes, PF_R32_UINT));
			PassParameters->RWAdaptiveScreenProbeData = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(AdaptiveScreenProbeData, PF_R32_UINT));
			PassParameters->RWScreenTileAdaptiveProbeHeader = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenTileAdaptiveProbeHeader));
			PassParameters->RWScreenTileAdaptiveProbeIndices = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenTileAdaptiveProbeIndices));
			PassParameters->AdaptiveProbePlacementMask = AdaptiveProbePlacementMask;
			PassParameters->ScreenProbeGatherCommonParameters = ScreenProbeGatherCommonParameters;
			PassParameters->ScreenProbeParameters = ScreenProbeParameters;
			PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
			PassParameters->SceneTextures = SceneTextureParameters;
			PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
			PassParameters->ScreenProbeParameters = ScreenProbeParameters;

			FScreenProbeAdaptivePlacementSpawnCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FScreenProbeAdaptivePlacementSpawnCS::FNumSamplesPerUniformProbe>(NumSamplesPerUniformProbe);
			auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeAdaptivePlacementSpawnCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("AdaptivePlacementSpawn"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				GroupCount);
		}
	}
	else
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(AdaptiveScreenProbeData), 0, ComputePassFlags);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenTileAdaptiveProbeIndices)), ClearValues, ComputePassFlags);
	}

	FRDGBufferRef ScreenProbeIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((uint32)EScreenProbeIndirectArgs::Max), TEXT("Lumen.ScreenProbeGather.ScreenProbeIndirectArgs"));

	{
		FSetupAdaptiveProbeIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSetupAdaptiveProbeIndirectArgsCS::FParameters>();
		PassParameters->RWScreenProbeIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ScreenProbeIndirectArgs, PF_R32_UINT));
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;

		auto ComputeShader = View.ShaderMap->GetShader<FSetupAdaptiveProbeIndirectArgsCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SetupAdaptiveProbeIndirectArgs"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	ScreenProbeParameters.ProbeIndirectArgs = ScreenProbeIndirectArgs;

	FRDGTextureRef BRDFProbabilityDensityFunction = nullptr;
	FRDGBufferSRVRef BRDFProbabilityDensityFunctionSH = nullptr;
	GenerateBRDF_PDF(GraphBuilder, View, SceneTextures, BRDFProbabilityDensityFunction, BRDFProbabilityDensityFunctionSH, ScreenProbeParameters, ComputePassFlags);

	const LumenRadianceCache::FRadianceCacheInputs RadianceCacheInputs = LumenScreenProbeGatherRadianceCache::SetupRadianceCacheInputs(View);

	FRadianceCacheConfiguration RadianceCacheConfiguration;
	RadianceCacheConfiguration.bSkyVisibility = CVarScreenProbeGatherRadianceCacheSkyVisibility.GetValueOnRenderThread() != 0;

	if (LumenScreenProbeGather::UseRadianceCache())
	{
		// Using !View.IsInstancedSceneView() to skip actual secondary stereo views only, View.ShouldRenderView() returns false for empty views as well
		if (!ShouldUseStereoLumenOptimizations() || !View.IsInstancedSceneView())
		{
			FMarkUsedRadianceCacheProbes GraphicsMarkUsedRadianceCacheProbesCallbacks;
			FMarkUsedRadianceCacheProbes ComputeMarkUsedRadianceCacheProbesCallbacks;

			ComputeMarkUsedRadianceCacheProbesCallbacks.AddLambda([ComputePassFlags](
				FRDGBuilder& GraphBuilder,
				const FViewInfo& View,
				const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
				{
					MarkUsedProbesForVisualize(GraphBuilder, View, RadianceCacheMarkParameters, ComputePassFlags);
				});

			// Mark radiance caches for screen probes
			ComputeMarkUsedRadianceCacheProbesCallbacks.AddLambda([&SceneTextures, &ScreenProbeParameters, ComputePassFlags](
				FRDGBuilder& GraphBuilder,
				const FViewInfo& View,
				const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
				{
					ScreenGatherMarkUsedProbes(
						GraphBuilder,
						View,
						SceneTextures,
						ScreenProbeParameters,
						RadianceCacheMarkParameters,
						ComputePassFlags);
				});

			// Mark radiance caches for hair strands
			if (HairStrands::HasViewHairStrandsData(View))
			{
				ComputeMarkUsedRadianceCacheProbesCallbacks.AddLambda([ComputePassFlags](
					FRDGBuilder& GraphBuilder,
					const FViewInfo& View,
					const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
					{
						HairStrandsMarkUsedProbes(
							GraphBuilder,
							View,
							RadianceCacheMarkParameters,
							ComputePassFlags);
					});
			}

			if (Lumen::UseLumenTranslucencyRadianceCacheReflections(ViewFamily))
			{
				const FSceneRenderer& SceneRenderer = *this;
				FViewInfo& ViewNonConst = View;

				GraphicsMarkUsedRadianceCacheProbesCallbacks.AddLambda([&SceneTextures, &SceneRenderer, &ViewNonConst](
					FRDGBuilder& GraphBuilder,
					const FViewInfo& View,
					const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
					{
						LumenTranslucencyReflectionsMarkUsedProbes(
							GraphBuilder,
							SceneRenderer,
							ViewNonConst,
							SceneTextures,
							&RadianceCacheMarkParameters);
					});
			}

			LumenRadianceCache::TInlineArray<LumenRadianceCache::FUpdateInputs> InputArray;
			LumenRadianceCache::TInlineArray<LumenRadianceCache::FUpdateOutputs> OutputArray;

			InputArray.Add(LumenRadianceCache::FUpdateInputs(
				RadianceCacheInputs,
				RadianceCacheConfiguration,
				View,
				nullptr,
				nullptr,
				MoveTemp(GraphicsMarkUsedRadianceCacheProbesCallbacks),
				MoveTemp(ComputeMarkUsedRadianceCacheProbesCallbacks)));

			OutputArray.Add(LumenRadianceCache::FUpdateOutputs(
				View.ViewState->Lumen.RadianceCacheState,
				RadianceCacheParameters));

			// Add the Translucency Volume radiance cache to the update so its dispatches can overlap
			{
				LumenRadianceCache::FUpdateInputs TranslucencyVolumeRadianceCacheUpdateInputs = GetLumenTranslucencyGIVolumeRadianceCacheInputs(
					GraphBuilder,
					View,
					FrameTemporaries,
					ComputePassFlags);

				if (TranslucencyVolumeRadianceCacheUpdateInputs.IsAnyCallbackBound())
				{
					InputArray.Add(TranslucencyVolumeRadianceCacheUpdateInputs);
					OutputArray.Add(LumenRadianceCache::FUpdateOutputs(
						View.ViewState->Lumen.TranslucencyVolumeRadianceCacheState,
						TranslucencyVolumeRadianceCacheParameters));
				}
			}

			LumenRadianceCache::UpdateRadianceCaches(
				GraphBuilder,
				FrameTemporaries,
				InputArray,
				OutputArray,
				Scene,
				ViewFamily,
				LumenCardRenderer.bPropagateGlobalLightingChange,
				ComputePassFlags);

			if (Lumen::UseLumenTranslucencyRadianceCacheReflections(ViewFamily))
			{
				View.GetOwnLumenTranslucencyGIVolume().RadianceCacheInterpolationParameters = RadianceCacheParameters;

				extern float GLumenTranslucencyReflectionsRadianceCacheReprojectionRadiusScale;
				extern float GLumenTranslucencyVolumeRadianceCacheClipmapFadeSize;
				View.GetOwnLumenTranslucencyGIVolume().RadianceCacheInterpolationParameters.RadianceCacheInputs.ReprojectionRadiusScale = GLumenTranslucencyReflectionsRadianceCacheReprojectionRadiusScale;
				View.GetOwnLumenTranslucencyGIVolume().RadianceCacheInterpolationParameters.RadianceCacheInputs.InvClipmapFadeSize = 1.0f / FMath::Clamp(GLumenTranslucencyVolumeRadianceCacheClipmapFadeSize, .001f, 16.0f);
			}
		}
		else
		{
			RadianceCacheParameters = View.GetLumenTranslucencyGIVolume().RadianceCacheInterpolationParameters;
		}
	}

	if (LumenScreenProbeGather::UseImportanceSampling(View))
	{
		GenerateImportanceSamplingRays(
			GraphBuilder,
			View,
			SceneTextures,
			RadianceCacheParameters,
			BRDFProbabilityDensityFunction,
			BRDFProbabilityDensityFunctionSH,
			ScreenProbeParameters,
			ComputePassFlags);
	}

	const EPixelFormat LightingDataFormat = Lumen::GetLightingDataFormat();
	
	const FIntPoint ScreenProbeTraceBufferSize = ScreenProbeParameters.ScreenProbeAtlasBufferSize * ScreenProbeParameters.ScreenProbeTracingOctahedronResolution;
	FRDGTextureDesc TraceRadianceDesc(FRDGTextureDesc::Create2D(ScreenProbeTraceBufferSize, LightingDataFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.TraceRadiance = GraphBuilder.CreateTexture(TraceRadianceDesc, TEXT("Lumen.ScreenProbeGather.TraceRadiance"));
	ScreenProbeParameters.RWTraceRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.TraceRadiance));

	ScreenProbeParameters.TraceHit = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ScreenProbeTraceBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("Lumen.ScreenProbeGather.TraceHit"));
	ScreenProbeParameters.RWTraceHit = GraphBuilder.CreateUAV(ScreenProbeParameters.TraceHit);

	TraceScreenProbes(
		GraphBuilder, 
		Scene,
		View, 
		FrameTemporaries,
		GLumenGatherCvars.TraceMeshSDFs != 0 && Lumen::UseMeshSDFTracing(ViewFamily.EngineShowFlags),
		SceneTextures,
		LightingChannelsTexture,
		RadianceCacheParameters,
		ScreenProbeParameters,
		MeshSDFGridParameters,
		ComputePassFlags);
	
	FScreenProbeGatherParameters GatherParameters;
	FilterScreenProbes(GraphBuilder, View, SceneTextures, ScreenProbeParameters, GatherParameters, ComputePassFlags);

	if (LumenScreenProbeGather::UseShortRangeAmbientOcclusion(ViewFamily.EngineShowFlags))
	{
		float MaxScreenTraceFraction = ScreenProbeParameters.ScreenProbeDownsampleFactor * 2.0f / (float)View.ViewRect.Width();
		ScreenSpaceBentNormalParameters = ComputeScreenSpaceShortRangeAO(GraphBuilder, Scene, View, SceneTextures, LightingChannelsTexture, BlueNoise, MaxScreenTraceFraction, ScreenProbeParameters.ScreenTraceNoFallbackThicknessScale, ComputePassFlags);
	}

	const FIntPoint EffectiveResolution = Substrate::GetSubstrateTextureResolution(View, SceneTextures.Config.Extent);
	const FIntPoint EffectiveViewExtent = FrameTemporaries.ViewExtent;
	const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);

	FRDGTextureRef DiffuseIndirect = FrameTemporaries.DiffuseIndirect.CreateSharedRT(
		GraphBuilder, 
		FRDGTextureDesc::Create2DArray(EffectiveResolution, LightingDataFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount),
		EffectiveViewExtent,
		TEXT("Lumen.ScreenProbeGather.DiffuseIndirect"));

	FRDGTextureRef LightIsMoving = FrameTemporaries.LightIsMoving.CreateSharedRT(
		GraphBuilder,
		FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_R8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount),
		EffectiveViewExtent,
		TEXT("Lumen.ScreenProbeGather.LightIsMoving"));

	const bool bSupportBackfaceDiffuse = GLumenScreenProbeSupportTwoSidedFoliageBackfaceDiffuse != 0;
	FRDGTextureRef BackfaceDiffuseIndirect = nullptr;

	if (bSupportBackfaceDiffuse)
	{
		FRDGTextureDesc BackfaceDiffuseIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, LightingDataFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
		BackfaceDiffuseIndirect = FrameTemporaries.BackfaceDiffuseIndirect.CreateSharedRT(GraphBuilder, BackfaceDiffuseIndirectDesc, EffectiveViewExtent, TEXT("Lumen.ScreenProbeGather.BackfaceDiffuseIndirect"));
	}

	FRDGTextureDesc RoughSpecularIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, LightingDataFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
	FRDGTextureRef RoughSpecularIndirect = FrameTemporaries.RoughSpecularIndirect.CreateSharedRT(GraphBuilder, RoughSpecularIndirectDesc, EffectiveViewExtent, TEXT("Lumen.ScreenProbeGather.RoughSpecularIndirect"));

	FScreenProbeIntegrateParameters IntegrateParameters;
	{
		const int32 IntegrateDownsampleFactor = LumenScreenProbeGather::GetIntegrateDownsampleFactor(View);
		const int32 ShortRangeAODownsampleFactor = LumenShortRangeAO::GetDownsampleFactor();

		IntegrateParameters.DownsampledSceneDepth = nullptr;
		IntegrateParameters.DownsampledSceneWorldNormal = nullptr;
		IntegrateParameters.IntegrateViewMin = FIntPoint::DivideAndRoundUp(View.ViewRect.Min, IntegrateDownsampleFactor);
		IntegrateParameters.IntegrateViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), IntegrateDownsampleFactor);
		IntegrateParameters.DownsampledBufferInvSize = FVector2f(1.0f) / FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, FMath::Max(IntegrateDownsampleFactor, ShortRangeAODownsampleFactor));
		IntegrateParameters.ScreenProbeGatherStateFrameIndex = LumenScreenProbeGather::GetStateFrameIndex(View.ViewState);
	}

	const bool bSSREnabled = GetViewPipelineState(View).ReflectionsMethod == EReflectionsMethod::SSR;

	InterpolateAndIntegrate(
		GraphBuilder,
		SceneTextures,
		View,
		ScreenProbeParameters,
		GatherParameters,
		IntegrateParameters,
		ScreenSpaceBentNormalParameters,
		bSSREnabled,
		DiffuseIndirect,
		LightIsMoving,
		BackfaceDiffuseIndirect,
		RoughSpecularIndirect,
		ComputePassFlags);

	// Set for DiffuseIndirectComposite
	if (LumenShortRangeAO::ShouldApplyDuringIntegration())
	{
		ScreenSpaceBentNormalParameters.ShortRangeAOMode = 0;
		ScreenSpaceBentNormalParameters.ShortRangeAOTexture = nullptr;
	}

	UpdateHistoryScreenProbeGather(
		GraphBuilder,
		View,
		SceneTextures,
		FrameTemporaries,
		ScreenProbeGatherCommonParameters,
		IntegrateParameters,
		ScreenSpaceBentNormalParameters,
		LumenCardRenderer.bPropagateGlobalLightingChange,
		DiffuseIndirect,
		LightIsMoving,
		BackfaceDiffuseIndirect,
		RoughSpecularIndirect,
		ComputePassFlags);

	FSSDSignalTextures DenoiserOutputs;
	DenoiserOutputs.Textures[0] = DiffuseIndirect;
	DenoiserOutputs.Textures[1] = bSupportBackfaceDiffuse ? BackfaceDiffuseIndirect : SystemTextures.Black;
	DenoiserOutputs.Textures[2] = RoughSpecularIndirect;

	if (GVarLumenScreenProbeGatherDebug.GetValueOnRenderThread() != 0)
	{
		FScreenProbeGatherDebugCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeGatherDebugCS::FParameters>();
		PassParameters->RWSceneColor = GraphBuilder.CreateUAV(SceneTextures.Color.Target);
		PassParameters->ScreenProbeGatherCommonParameters = ScreenProbeGatherCommonParameters;
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->VisualizeProbePlacement = GVarLumenScreenProbeGatherDebugProbePlacement.GetValueOnRenderThread();

		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeGatherDebugCS>();

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FScreenProbeGatherDebugCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ScreenProbeGatherDebug"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	// Sample radiance caches for hair strands lighting. Only used wht radiance cache is enabled
	if (LumenScreenProbeGather::UseRadianceCache() && HairStrands::HasViewHairStrandsData(View))
	{
		RenderHairStrandsLumenLighting(GraphBuilder, Scene, View);
	}

	return DenoiserOutputs;
}