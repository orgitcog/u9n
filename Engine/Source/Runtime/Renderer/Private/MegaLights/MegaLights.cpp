// Copyright Epic Games, Inc. All Rights Reserved.

#include "MegaLights.h"
#include "MegaLightsDefinitions.h"
#include "MegaLightsInternal.h"
#include "RendererPrivate.h"
#include "PixelShaderUtils.h"
#include "BasePassRendering.h"
#include "VolumetricFogShared.h"
#include "Shadows/ShadowSceneRenderer.h"
#include "HairStrands/HairStrandsData.h"

static TAutoConsoleVariable<int32> CVarMegaLightsProjectSetting(
	TEXT("r.MegaLights.EnableForProject"),
	0,
	TEXT("Whether to use MegaLights by default, but this can still be overridden by Post Process Volumes, or disabled per-light. MegaLights uses stochastic sampling to render many shadow casting lights efficiently, with a consistent low GPU cost. MegaLights requires Hardware Ray Tracing, and does not support Directional Lights. Experimental feature."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsAllowed(
	TEXT("r.MegaLights.Allowed"),
	1,
	TEXT("Whether the MegaLights feature is allowed by scalability and device profiles."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDownsampleFactor(
	TEXT("r.MegaLights.DownsampleFactor"),
	2,
	TEXT("Downsample factor from the main viewport to trace rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsNumSamplesPerPixel(
	TEXT("r.MegaLights.NumSamplesPerPixel"),
	4,
	TEXT("Number of samples per pixel. Supported values: 2, 4 and 16."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsMinSampleWeight(
	TEXT("r.MegaLights.MinSampleWeight"),
	0.001f,
	TEXT("Determines minimal sample influence on final pixels. Used to skip samples which would have minimal impact to the final image even if light is fully visible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsMaxShadingWeight(
	TEXT("r.MegaLights.MaxShadingWeight"),
	20.0f,
	TEXT("Clamps low-probability samples in order to reduce fireflies."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsGuideByHistory(
	TEXT("r.MegaLights.GuideByHistory"),
	2,
	TEXT("Whether to reduce sampling chance for lights which were hidden last frame. Reduces noise in areas where bright lights are shadowed.\n")
	TEXT("0 - disabled\n")
	TEXT("1 - more rays towards visible lights\n")
	TEXT("2 - more rays towards visible parts of lights"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsGuideByHistoryVisibleSamplePercentage(
	TEXT("r.MegaLights.GuideByHistory.VisibleSamplePercentage"),
	0.8f,
	TEXT("Percentage of samples which should be used to sample visible lights. Higher values reduce noise, but are slower to discover new visible lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsGuideByHistoryFilter(
	TEXT("r.MegaLights.GuideByHistory.Filter"),
	1,
	TEXT("Whether to filter history by sharing visibility between nearby tiles."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsGuideByHistoryAreaLightHiddenWeight(
	TEXT("r.MegaLights.GuideByHistory.AreaLightHiddenWeight"),
	0.25f,
	TEXT("PDF weight for hidden parts of an area light. 1 will disable area light guiding. Lower values will improve static quality, but will cause more artifacts in motion when area light guiding is wrong."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsShadingConfidence(
	TEXT("r.MegaLights.ShadingConfidence"),
	1,
	TEXT("Whether to use shading confidence to reduce denoising and passthrough original signal to TSR for pixels which are well sampled."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTemporal(
	TEXT("r.MegaLights.Temporal"),
	1,
	TEXT("Whether to use temporal accumulation for shadow mask."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTemporalMinFramesAccumulatedForHistoryMiss(
	TEXT("r.MegaLights.Temporal.MinFramesAccumulatedForHistoryMiss"),
	4,
	TEXT("Minimal amount of history length when reducing history length due to a history miss. Higher values than 1 soften and slowdown transitions."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTemporalMinFramesAccumulatedForHighConfidence(
	TEXT("r.MegaLights.Temporal.MinFramesAccumulatedForHighConfidence"),
	2,
	TEXT("Minimal amount of history length when reducing history length due to a high confidence. Higher values than 1 soften image, but reduce noise in high confidence areas."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTemporalMaxFramesAccumulated(
	TEXT("r.MegaLights.Temporal.MaxFramesAccumulated"),
	12,
	TEXT("Max history length when accumulating frames. Lower values have less ghosting, but more noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsTemporalNeighborhoodClampScale(
	TEXT("r.MegaLights.Temporal.NeighborhoodClampScale"),
	1.0f,
	TEXT("Scales how permissive is neighborhood clamp. Higher values increase ghosting, but reduce noise and instability."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsSpatial(
	TEXT("r.MegaLights.Spatial"),
	1,
	TEXT("Whether denoiser should run spatial filter."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsSpatialDepthWeightScale(
	TEXT("r.MegaLights.Spatial.DepthWeightScale"),
	10000.0f,
	TEXT("Scales the depth weight of the spatial filter. Smaller values allow for more sample reuse, but also introduce more bluriness between unrelated surfaces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsSpatialKernelRadius(
	TEXT("r.MegaLights.Spatial.KernelRadius"),
	8.0f,
	TEXT("Spatial filter kernel radius in pixels"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsSpatialNumSamples(
	TEXT("r.MegaLights.Spatial.NumSamples"),
	4,
	TEXT("Number of spatial filter samples."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsSpatialMaxDisocclusionFrames(
	TEXT("r.MegaLights.Spatial.MaxDisocclusionFrames"),
	3,
	TEXT("Number of of history frames to boost spatial filtering in order to minimize noise after disocclusion."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsWaveOps(
	TEXT("r.MegaLights.WaveOps"),
	1,
	TEXT("Whether to use wave ops. Useful for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDebug(
	TEXT("r.MegaLights.Debug"),
	0,
	TEXT("Whether to enabled debug mode, which prints various extra debug information from shaders.")
	TEXT("0 - Disable\n")
	TEXT("1 - Visualize tracing\n")
	TEXT("2 - Visualize sampling"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDebugCursorX(
	TEXT("r.MegaLights.Debug.CursorX"),
	-1,
	TEXT("Override default debug visualization cursor position."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDebugCursorY(
	TEXT("r.MegaLights.Debug.CursorY"),
	-1,
	TEXT("Override default debug visualization cursor position."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDebugLightId(
	TEXT("r.MegaLights.Debug.LightId"),
	-1,
	TEXT("Which light to show debug info for. When set to -1, uses the currently selected light in editor."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDebugVisualizeLight(
	TEXT("r.MegaLights.Debug.VisualizeLight"),
	0,
	TEXT("Whether to visualize selected light. Useful to find in in the level."),
	ECVF_RenderThreadSafe
);

int32 GMegaLightsReset = 0;
FAutoConsoleVariableRef CVarMegaLightsReset(
	TEXT("r.MegaLights.Reset"),
	GMegaLightsReset,
	TEXT("Reset history for debugging."),
	ECVF_RenderThreadSafe
);

int32 GMegaLightsResetEveryNthFrame = 0;
	FAutoConsoleVariableRef CVarMegaLightsResetEveryNthFrame(
	TEXT("r.MegaLights.ResetEveryNthFrame"),
		GMegaLightsResetEveryNthFrame,
	TEXT("Reset history every Nth frame for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsFixedStateFrameIndex(
	TEXT("r.MegaLights.FixedStateFrameIndex"),
	-1,
	TEXT("Whether to override View.StateFrameIndex for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTexturedRectLights(
	TEXT("r.MegaLights.TexturedRectLights"),
	1,
	TEXT("Whether to support textured rect lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsLightFunctions(
	TEXT("r.MegaLights.LightFunctions"),
	1,
	TEXT("Whether to support light functions."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarMegaLightLightingChannels(
	TEXT("r.MegaLights.LightingChannels"),
	1,
	TEXT("Whether to enable lighting channels to block shadowing"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsIESProfiles(
	TEXT("r.MegaLights.IESProfiles"),
	1,
	TEXT("Whether to support IES profiles on lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDirectionalLights(
	TEXT("r.MegaLights.DirectionalLights"),
	0,
	TEXT("Whether to support directional lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsVolume(
	TEXT("r.MegaLights.Volume"),
	1,
	TEXT("Whether to enable a translucency volume used for Volumetric Fog and Volume Lit Translucency."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsVolumeDownsampleFactor(
	TEXT("r.MegaLights.Volume.DownsampleFactor"),
	2,
	TEXT("Downsample factor applied to Volumetric Fog resolution. Affects the resolution at which rays are traced."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsVolumeHZBOcclusionTest(
	TEXT("r.MegaLights.Volume.HZBOcclusionTest"),
	1,
	TEXT("Whether to skip computation for cells occluded by HZB."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsVolumeNumSamplesPerVoxel(
	TEXT("r.MegaLights.Volume.NumSamplesPerVoxel"),
	2,
	TEXT("Number of samples (shadow rays) per half-res voxel. Supported values: 2 and 4."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsVolumeMinSampleWeight(
	TEXT("r.MegaLights.Volume.MinSampleWeight"),
	0.1f,
	TEXT("Determines minimal sample influence on lighting cached in a volume. Used to skip samples which would have minimal impact to the final image even if light is fully visible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsVolumeMaxShadingWeight(
	TEXT("r.MegaLights.Volume.MaxShadingWeight"),
	20.0f,
	TEXT("Clamps low-probability samples in order to reduce fireflies."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsVolumeLightFunctions(
	TEXT("r.MegaLights.Volume.LightFunctions"),
	1,
	TEXT("Whether to support light functions inside the mega light translucency volume."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsVolumeGuideByHistory(
	TEXT("r.MegaLights.Volume.GuideByHistory"),
	1,
	TEXT("Whether to reduce sampling chance for lights which were hidden last frame. Reduces noise in areas where bright lights are shadowed.\n")
	TEXT("0 - disabled\n")
	TEXT("1 - more rays towards visible lights"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsVolumeGuideByHistoryVisibleSamplePercentage(
	TEXT("r.MegaLights.Volume.GuideByHistory.VisibleSamplePercentage"),
	0.8f,
	TEXT("Percentage of samples which should be used to sample visible lights. Higher values reduce noise, but are slower to discover new visible lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsVolumeGuideByHistoryFilter(
	TEXT("r.MegaLights.Volume.GuideByHistory.Filter"),
	1,
	TEXT("Whether to filter history by sharing visibility between nearby voxels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsVolumeDebug(
	TEXT("r.MegaLights.Volume.Debug"),
	0,
	TEXT("Whether to enabled debug mode, which prints various extra debug information from volume shaders.")
	TEXT("0 - Disable\n")
	TEXT("1 - Visualize tracing\n")
	TEXT("2 - Visualize sampling"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMegaLightsVolumeDebugSliceIndex(
	TEXT("r.MegaLights.Volume.DebugSliceIndex"),
	16,
	TEXT("Which volume slice to visualize."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMegaLightsTranslucencyVolume(
	TEXT("r.MegaLights.TranslucencyVolume"),
	1,
	TEXT("Whether to enable Lit Translucency Volume."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMegaLightsTranslucencyVolumeDownsampleFactor(
	TEXT("r.MegaLights.TranslucencyVolume.DownsampleFactor"),
	2,
	TEXT("Downsample factor applied to Translucency Lighting Volume resolution. Affects the resolution at which rays are traced."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTranslucencyVolumeNumSamplesPerVoxel(
	TEXT("r.MegaLights.TranslucencyVolume.NumSamplesPerVoxel"),
	2,
	TEXT("Number of samples (shadow rays) per half-res voxel. Supported values: 2 and 4."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsTranslucencyVolumeMinSampleWeight(
	TEXT("r.MegaLights.TranslucencyVolume.MinSampleWeight"),
	0.1f,
	TEXT("Determines minimal sample influence on lighting cached in a volume. Used to skip samples which would have minimal impact to the final image even if light is fully visible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsTranslucencyVolumeMaxShadingWeight(
	TEXT("r.MegaLights.TranslucencyVolume.MaxShadingWeight"),
	20.0f,
	TEXT("Clamps low-probability samples in order to reduce fireflies."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTranslucencyVolumeLightFunctions(
	TEXT("r.MegaLights.TranslucencyVolume.LightFunctions"),
	1,
	TEXT("Whether to support light functions inside the mega light translucency volume."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTranslucencyVolumeGuideByHistory(
	TEXT("r.MegaLights.TranslucencyVolume.GuideByHistory"),
	1,
	TEXT("Whether to reduce sampling chance for lights which were hidden last frame. Reduces noise in areas where bright lights are shadowed.\n")
	TEXT("0 - disabled\n")
	TEXT("1 - more rays towards visible lights"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsTranslucencyVolumeGuideByHistoryVisibleSamplePercentage(
	TEXT("r.MegaLights.TranslucencyVolume.GuideByHistory.VisibleSamplePercentage"),
	0.8f,
	TEXT("Percentage of samples which should be used to sample visible lights. Higher values reduce noise, but are slower to discover new visible lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTranslucencyVolumeGuideByHistoryFilter(
	TEXT("r.MegaLights.TranslucencyVolume.GuideByHistory.Filter"),
	1,
	TEXT("Whether to filter history by sharing visibility between nearby voxels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTranslucencyVolumeDebug(
	TEXT("r.MegaLights.TranslucencyVolume.Debug"),
	0,
	TEXT("Whether to enabled debug mode, which prints various extra debug information from Translucency Volume shaders.")
	TEXT("0 - Disable\n")
	TEXT("1 - Visualize tracing\n")
	TEXT("2 - Visualize sampling"),
	ECVF_RenderThreadSafe);

// Rendering project setting
int32 GMegaLightsDefaultShadowMethod = 0;
FAutoConsoleVariableRef CMegaLightsDefaultShadowMethod(
	TEXT("r.MegaLights.DefaultShadowMethod"),
	GMegaLightsDefaultShadowMethod,
	TEXT("The default shadowing method for MegaLights, unless over-ridden on the light component.\n")
	TEXT("0 - Ray Tracing. Preferred method, which guarantees fixed MegaLights cost and correct area shadows, but is dependent on the BVH representation quality.\n")
	TEXT("1 - Virtual Shadow Maps. Has a significant per light cost, but can cast shadows directly from the Nanite geometry using rasterization."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsEnableHairStrands(
	TEXT("r.MegaLights.HairStrands"),
	1,
	TEXT("Wheter to enable hair strands support for MegaLights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDownsampleFactorHairStrands(
	TEXT("r.MegaLights.HairStrands.DownsampleFactor"),
	1,
	TEXT("Downsample factor from the main viewport to trace rays with hair strands."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsNumSamplesPerPixelHairStrands(
	TEXT("r.MegaLights.HairStrands.NumSamplesPerPixel"),
	4,
	TEXT("Number of samples per pixel with hair strands. Supported values: 2, 4 and 16."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDebugHairStrands(
	TEXT("r.MegaLights.HairStrands.Debug"),
	0,
	TEXT("Whether to enabled debug mode for hairstrands, which prints various extra debug information from shaders.")
	TEXT("0 - Disable\n")
	TEXT("1 - Visualize tracing\n")
	TEXT("2 - Visualize sampling"),
	ECVF_RenderThreadSafe
);

namespace MegaLights
{
	constexpr int32 TileSize = TILE_SIZE;
	constexpr int32 VisibleLightHashSize = VISIBLE_LIGHT_HASH_SIZE;

	bool ShouldCompileShaders(EShaderPlatform ShaderPlatform)
	{
		if (IsMobilePlatform(ShaderPlatform))
		{
			return false;
		}

		// SM6 because it uses typed loads to accumulate lights
		return IsFeatureLevelSupported(ShaderPlatform, ERHIFeatureLevel::SM6) && RHISupportsWaveOperations(ShaderPlatform);
	}

	bool IsRequested(const FSceneViewFamily& ViewFamily)
	{
		return ViewFamily.Views[0]->FinalPostProcessSettings.bMegaLights
			&& CVarMegaLightsAllowed.GetValueOnRenderThread() != 0
			&& ViewFamily.EngineShowFlags.MegaLights
			&& ShouldCompileShaders(ViewFamily.GetShaderPlatform());
	}

	bool HasRequiredTracingData(const FSceneViewFamily& ViewFamily)
	{
		return IsHardwareRayTracingSupported(ViewFamily) || IsSoftwareRayTracingSupported(ViewFamily);
	}

	bool IsEnabled(const FSceneViewFamily& ViewFamily)
	{
		return IsRequested(ViewFamily) && HasRequiredTracingData(ViewFamily);
	}

	uint32 GetSampleMargin()
	{
		// #ml_todo: should be calculated based on DownsampleFactor / Volume.DownsampleFactor
		return 3;
	}

	bool UseVolume()
	{
		return CVarMegaLightsVolume.GetValueOnRenderThread() != 0;
	}

	bool UseTranslucencyVolume()
	{
		return CVarMegaLightsTranslucencyVolume.GetValueOnRenderThread() != 0;
	}

	bool IsUsingLightFunctions(const FSceneViewFamily& ViewFamily)
	{
		return IsEnabled(ViewFamily) && CVarMegaLightsLightFunctions.GetValueOnRenderThread() != 0;
	}

	bool IsUsingLightingChannels()
	{
		return CVarMegaLightLightingChannels.GetValueOnRenderThread() != 0;
	}

	EMegaLightsMode GetMegaLightsMode(const FSceneViewFamily& ViewFamily, uint8 LightType, bool bLightAllowsMegaLights, TEnumAsByte<EMegaLightsShadowMethod::Type> ShadowMethod)
	{
		if ((LightType != LightType_Directional || CVarMegaLightsDirectionalLights.GetValueOnRenderThread())
			&& IsEnabled(ViewFamily) 
			&& bLightAllowsMegaLights)
		{
			// Resolve  default
			if (ShadowMethod == EMegaLightsShadowMethod::Default)
			{
				if (GMegaLightsDefaultShadowMethod == 1)
				{
					ShadowMethod = EMegaLightsShadowMethod::VirtualShadowMap;
				}
				else
				{
					ShadowMethod = EMegaLightsShadowMethod::RayTracing;
				}
			}

			const bool bUseVSM = ShadowMethod == EMegaLightsShadowMethod::VirtualShadowMap;

			if (bUseVSM)
			{
				return EMegaLightsMode::EnabledVSM;
			}
			// Just check first view, assuming the ray tracing flag is the same for all views.  See comment in the ShouldRenderRayTracingEffect function that accepts a ViewFamily.
			else if (ViewFamily.Views[0]->IsRayTracingAllowedForView())
			{
				return EMegaLightsMode::EnabledRT;
			}
		}

		return EMegaLightsMode::Disabled;
	}

	uint32 GetStateFrameIndex(FSceneViewState* ViewState)
	{
		uint32 StateFrameIndex = ViewState ? ViewState->GetFrameIndex() : 0;

		if (CVarMegaLightsFixedStateFrameIndex.GetValueOnRenderThread() >= 0)
		{
			StateFrameIndex = CVarMegaLightsFixedStateFrameIndex.GetValueOnRenderThread();
		}

		return StateFrameIndex;
	}

	uint32 GetDownsampleFactor(EMegaLightsInput InputType)
	{
		switch (InputType)
		{
			case EMegaLightsInput::GBuffer: return FMath::Clamp(CVarMegaLightsDownsampleFactor.GetValueOnAnyThread(), 1, 2);
			case EMegaLightsInput::HairStrands: return FMath::Clamp(CVarMegaLightsDownsampleFactorHairStrands.GetValueOnAnyThread(), 1, 2);
			default: checkf(false, TEXT("MegaLight::GetDownsampleFactor not implemented")); return 1;
		};
	}

	FIntPoint GetNumSamplesPerPixel2d(int32 NumSamplesPerPixel1d)
	{
		if (NumSamplesPerPixel1d >= 16)
		{
			return FIntPoint(4, 4);
		}
		else if (NumSamplesPerPixel1d >= 4)
		{
			return FIntPoint(2, 2);
		}
		else
		{
			return FIntPoint(2, 1);
		}
	}

	FIntPoint GetNumSamplesPerPixel2d(EMegaLightsInput InputType)
	{
		switch (InputType)
		{
			case EMegaLightsInput::GBuffer: return GetNumSamplesPerPixel2d(CVarMegaLightsNumSamplesPerPixel.GetValueOnAnyThread());
			case EMegaLightsInput::HairStrands: return GetNumSamplesPerPixel2d(CVarMegaLightsNumSamplesPerPixelHairStrands.GetValueOnAnyThread());
			default: checkf(false, TEXT("MegaLight::GetNumSamplesPerPixel2d not implemented")); return false;
		};
	}

	FIntVector GetNumSamplesPerVoxel3d(int32 NumSamplesPerVoxel1d)
	{
		if (NumSamplesPerVoxel1d >= 4)
		{
			return FIntVector(2, 2, 1);
		}
		else
		{
			return FIntVector(2, 1, 1);
		}
	}

	int32 GetDebugMode(EMegaLightsInput InputType)
	{
		if (CVarMegaLightsVolumeDebug.GetValueOnRenderThread() != 0 || CVarMegaLightsTranslucencyVolumeDebug.GetValueOnRenderThread() != 0)
		{
			return 0;
		}
		switch (InputType)
		{
			case EMegaLightsInput::GBuffer: return CVarMegaLightsDebug.GetValueOnRenderThread();
			case EMegaLightsInput::HairStrands: return CVarMegaLightsDebugHairStrands.GetValueOnRenderThread();
		};
		return 0;
	}
	
	bool SupportsGuideByHistory(EMegaLightsInput InputType)
	{
		switch (InputType)
		{
			case EMegaLightsInput::GBuffer: return true;
			case EMegaLightsInput::HairStrands: return false;
			default: checkf(false, TEXT("MegaLight::SupportsGuideByHistory not implemented")); return false;
		};
	}

	bool SupportsSpatialFilter(EMegaLightsInput InputType)
	{
		switch (InputType)
		{
			case EMegaLightsInput::GBuffer: return true;
			case EMegaLightsInput::HairStrands: return false; // Disable for now due to lack of proper reconstruction filter
			default: checkf(false, TEXT("MegaLight::SupportsSpatialFilter not implemented")); return false;
		};
	}

	bool SupportsTemporalFilter(EMegaLightsInput InputType)
	{
		switch (InputType)
		{
			case EMegaLightsInput::GBuffer: return true;
			case EMegaLightsInput::HairStrands: return false; // Disable for now due to lack of proper temporal reprojection
			default: checkf(false, TEXT("MegaLight::SupportsTemporalFilter not implemented")); return false;
		};
	}

	bool UseWaveOps(EShaderPlatform ShaderPlatform)
	{
		return CVarMegaLightsWaveOps.GetValueOnRenderThread() != 0
			&& GRHISupportsWaveOperations
			&& RHISupportsWaveOperations(ShaderPlatform);
	}

	void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		ShaderPrint::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
	}

	// Keep in sync with TILE_TYPE_* in shaders
	enum class ETileType : uint8
	{
		SimpleShading = TILE_MODE_SIMPLE_SHADING,
		ComplexShading = TILE_MODE_COMPLEX_SHADING,
		SimpleShading_Rect = TILE_MODE_SIMPLE_SHADING_RECT,
		ComplexShading_Rect = TILE_MODE_COMPLEX_SHADING_RECT,
		SimpleShading_Rect_Textured = TILE_MODE_SIMPLE_SHADING_RECT_TEXTURED,
		ComplexShading_Rect_Textured = TILE_MODE_COMPLEX_SHADING_RECT_TEXTURED,
		SHADING_MAX_LEGACY = TILE_MODE_EMPTY,
		
		Empty = TILE_MODE_EMPTY,
		MAX_LEGACY = TILE_MODE_MAX_LEGACY,

		SHADING_MIN_SUBSTRATE = TILE_MODE_SINGLE_SHADING,
		SingleShading = TILE_MODE_SINGLE_SHADING,
		ComplexSpecialShading = TILE_MODE_COMPLEX_SPECIAL_SHADING,
		SingleShading_Rect = TILE_MODE_SINGLE_SHADING_RECT,
		ComplexSpecialShading_Rect = TILE_MODE_COMPLEX_SPECIAL_SHADING_RECT,
		SingleShading_Rect_Textured = TILE_MODE_SINGLE_SHADING_RECT_TEXTURED,
		ComplexSpecialShading_Rect_Textured = TILE_MODE_COMPLEX_SPECIAL_SHADING_RECT_TEXTURED,
		SHADING_MAX_SUBSTRATE = TILE_MODE_MAX,

		MAX_SUBSTRATE = TILE_MODE_MAX
	};

	const TCHAR* GetTileTypeString(ETileType TileType)
	{
		switch (TileType)
		{
		case ETileType::SimpleShading:						return TEXT("Simple");
		case ETileType::SingleShading:						return TEXT("Single");
		case ETileType::ComplexShading:						return TEXT("Complex");
		case ETileType::ComplexSpecialShading:				return TEXT("Complex Special ");

		case ETileType::SimpleShading_Rect:					return TEXT("Simple Rect");
		case ETileType::SingleShading_Rect:					return TEXT("Single Rect");
		case ETileType::ComplexShading_Rect:				return TEXT("Complex Rect");
		case ETileType::ComplexSpecialShading_Rect:			return TEXT("Complex Special Rect");

		case ETileType::SimpleShading_Rect_Textured:		return TEXT("Simple Textured Rect");
		case ETileType::SingleShading_Rect_Textured:		return TEXT("Single Textured Rect");
		case ETileType::ComplexShading_Rect_Textured:		return TEXT("Complex Textured Rect");
		case ETileType::ComplexSpecialShading_Rect_Textured:return TEXT("Complex Special Textured Rect");

		case ETileType::Empty:								return TEXT("Empty");
		
		default:
			return nullptr;
		}
	}

	bool IsRectLightTileType(ETileType TileType)
	{
		return TileType == MegaLights::ETileType::SimpleShading_Rect
			|| TileType == MegaLights::ETileType::ComplexShading_Rect
			|| TileType == MegaLights::ETileType::SimpleShading_Rect_Textured
			|| TileType == MegaLights::ETileType::ComplexShading_Rect_Textured

			|| TileType == MegaLights::ETileType::SingleShading_Rect
			|| TileType == MegaLights::ETileType::ComplexSpecialShading_Rect
			|| TileType == MegaLights::ETileType::SingleShading_Rect_Textured
			|| TileType == MegaLights::ETileType::ComplexSpecialShading_Rect_Textured;
	}

	bool IsTexturedLightTileType(ETileType TileType)
	{
		return TileType == MegaLights::ETileType::SimpleShading_Rect_Textured
			|| TileType == MegaLights::ETileType::ComplexShading_Rect_Textured
			|| TileType == MegaLights::ETileType::SingleShading_Rect_Textured
			|| TileType == MegaLights::ETileType::ComplexSpecialShading_Rect_Textured;
	}

	float GetTemporalMaxFramesAccumulated()
	{
		return FMath::Max(CVarMegaLightsTemporalMaxFramesAccumulated.GetValueOnRenderThread(), 1.0f);
	}

	float GetSpatialFilterMaxDisocclusionFrames()
	{
		return FMath::Max(FMath::Min(CVarMegaLightsSpatialMaxDisocclusionFrames.GetValueOnRenderThread(), GetTemporalMaxFramesAccumulated() - 1.0f), 0.0f);
	}

	TArray<int32> GetShadingTileTypes(EMegaLightsInput InputType)
	{
		// Build available tile types
		TArray<int32> Out;
		if (InputType == EMegaLightsInput::GBuffer)
		{
			for (int32 TileType = 0; TileType < (int32)MegaLights::ETileType::SHADING_MAX_LEGACY; ++TileType)
			{
				Out.Add(TileType);
			}
			if (Substrate::IsSubstrateEnabled())
			{
				for (int32 TileType = (int32)MegaLights::ETileType::SHADING_MIN_SUBSTRATE; TileType < (int32)MegaLights::ETileType::SHADING_MAX_SUBSTRATE; ++TileType)
				{
					Out.Add(TileType);
				}
			}
		}
		else if (InputType == EMegaLightsInput::HairStrands)
		{
			// Hair only uses complex tiles
			Out.Add(int32(MegaLights::ETileType::ComplexShading));
			Out.Add(int32(MegaLights::ETileType::ComplexShading_Rect));
			Out.Add(int32(MegaLights::ETileType::ComplexShading_Rect_Textured));
		}
		return Out;
	}

	float GetGuideByHistoryHiddenRatio(float VisibleSamplePercentage)
	{
		VisibleSamplePercentage = FMath::Clamp(VisibleSamplePercentage, 0.0f, 1.0f);
		if (VisibleSamplePercentage > 0.0f)
		{
			return (1 - VisibleSamplePercentage) / VisibleSamplePercentage;
		}
		else
		{
			return 0.0f;
		}
	}

	float GetGuideByHistoryHiddenRatio()
	{
		return GetGuideByHistoryHiddenRatio(CVarMegaLightsGuideByHistoryVisibleSamplePercentage.GetValueOnRenderThread());
	}
};

namespace MegaLightsVolume
{
	uint32 GetDownsampleFactor()
	{
		return FMath::Clamp(CVarMegaLightsVolumeDownsampleFactor.GetValueOnAnyThread(), 1, 2);
	}

	FIntVector GetNumSamplesPerVoxel3d()
	{
		return MegaLights::GetNumSamplesPerVoxel3d(CVarMegaLightsVolumeNumSamplesPerVoxel.GetValueOnAnyThread());
	}

	bool UsesLightFunction()
	{
		return CVarMegaLightsVolumeLightFunctions.GetValueOnRenderThread() != 0;
	}

	float GetGuideByHistoryHiddenRatio()
	{
		return MegaLights::GetGuideByHistoryHiddenRatio(CVarMegaLightsVolumeGuideByHistoryVisibleSamplePercentage.GetValueOnRenderThread());
	}

	int32 GetDebugMode()
	{
		return CVarMegaLightsVolumeDebug.GetValueOnRenderThread();
	}
}

namespace MegaLightsTranslucencyVolume
{
	uint32 GetDownsampleFactor()
	{
		return FMath::Clamp(CVarMegaLightsTranslucencyVolumeDownsampleFactor.GetValueOnAnyThread(), 1, 2);
	}

	FIntVector GetNumSamplesPerVoxel3d()
	{
		return MegaLights::GetNumSamplesPerVoxel3d(CVarMegaLightsTranslucencyVolumeNumSamplesPerVoxel.GetValueOnAnyThread());
	}

	bool UsesLightFunction()
	{
		return CVarMegaLightsTranslucencyVolumeLightFunctions.GetValueOnRenderThread() != 0;
	}

	float GetGuideByHistoryHiddenRatio()
	{
		return MegaLights::GetGuideByHistoryHiddenRatio(CVarMegaLightsTranslucencyVolumeGuideByHistoryVisibleSamplePercentage.GetValueOnRenderThread());
	}

	int32 GetDebugMode()
	{
		return CVarMegaLightsTranslucencyVolumeDebug.GetValueOnRenderThread();
	}
}

class FTileClassificationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTileClassificationCS)
	SHADER_USE_PARAMETER_STRUCT(FTileClassificationCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWTileData)
		SHADER_PARAMETER(uint32, EnableTexturedRectLights)
	END_SHADER_PARAMETER_STRUCT()

	class FDownsampledClassification : SHADER_PERMUTATION_BOOL("DOWNSAMPLED_CLASSIFICATION");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	class FInputType : SHADER_PERMUTATION_INT("INPUT_TYPE", int32(EMegaLightsInput::Count));
	using FPermutationDomain = TShaderPermutationDomain<FDownsampledClassification, FDebugMode, FInputType>;

	static int32 GetGroupSize()
	{	
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FTileClassificationCS, "/Engine/Private/MegaLights/MegaLights.usf", "TileClassificationCS", SF_Compute);

class FInitTileIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitTileIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitTileIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDownsampledTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static int32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitTileIndirectArgsCS, "/Engine/Private/MegaLights/MegaLights.usf", "InitTileIndirectArgsCS", SF_Compute);

class FGenerateLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FGenerateLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSampleRays)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, VisibleLightHashHistory)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, VisibleLightMaskHashHistory)
		SHADER_PARAMETER(uint32, GuideByHistoryMode)
		SHADER_PARAMETER(float, AreaLightHiddenPDFWeight)
		SHADER_PARAMETER(float, GuideByHistoryHiddenRatio)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ShadowMaskBits)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, MegaLightsDepthHistory)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryGatherUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryBufferSizeAndInvSize)
		SHADER_PARAMETER(FIntPoint, HistoryVisibleLightHashViewMinInTiles)
		SHADER_PARAMETER(FIntPoint, HistoryVisibleLightHashViewSizeInTiles)
	END_SHADER_PARAMETER_STRUCT()

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)MegaLights::ETileType::SHADING_MAX_SUBSTRATE);
	class FNumSamplesPerPixel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL_1D", 2, 4, 16);
	class FGuideByHistory : SHADER_PERMUTATION_BOOL("GUIDE_BY_HISTORY");
	class FInputType : SHADER_PERMUTATION_INT("INPUT_TYPE", int32(EMegaLightsInput::Count));
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FNumSamplesPerPixel1d, FGuideByHistory, FInputType, FDebugMode>;

	static int32 GetGroupSize()
	{	
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		EMegaLightsInput InputType = EMegaLightsInput(PermutationVector.Get<FInputType>());
		if (MegaLights::GetShadingTileTypes(InputType).Find(PermutationVector.Get<FTileType>()) == INDEX_NONE)
		{
			return false;
		}

		if (PermutationVector.Get<FGuideByHistory>() && !MegaLights::SupportsGuideByHistory(InputType))
		{
			return false;
		}
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// precache all tile types
		EMegaLightsInput InputType = EMegaLightsInput(PermutationVector.Get<FInputType>());
		int NumSamplesPerPixel1d = PermutationVector.Get<FNumSamplesPerPixel1d>();
		const FIntPoint NumSamplesPerPixel2d = MegaLights::GetNumSamplesPerPixel2d(InputType);
		if (NumSamplesPerPixel1d != (NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y))
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}
		
		if (PermutationVector.Get<FDebugMode>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 NumSamplesPerPixel1d = PermutationVector.Get<FNumSamplesPerPixel1d>();
		const FIntPoint NumSamplesPerPixel2d = MegaLights::GetNumSamplesPerPixel2d(NumSamplesPerPixel1d);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_X"), NumSamplesPerPixel2d.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_Y"), NumSamplesPerPixel2d.Y);

		if (IsMetalPlatform(Parameters.Platform))
		{
			OutEnvironment.SetDefine(TEXT("FORCE_DISABLE_GLINTS_AA"), 1); // SUBSTRATE_TODO Temporary, while Metal compute does not have derivatives.
		}

		OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGenerateLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsSampling.usf", "GenerateLightSamplesCS", SF_Compute);

class FVolumeGenerateLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVolumeGenerateLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FVolumeGenerateLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsVolumeParameters, MegaLightsVolumeParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, VolumeVisibleLightHashHistory)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeLightSamples)
		SHADER_PARAMETER(FIntVector, HistoryVolumeVisibleLightHashViewSizeInTiles)
		SHADER_PARAMETER(FIntVector, VolumeVisibleLightHashTileSize)
		SHADER_PARAMETER(float, VolumeGuideByHistoryHiddenRatio)
	END_SHADER_PARAMETER_STRUCT()

	class FTranslucencyLightingVolume : SHADER_PERMUTATION_BOOL("TRANSLUCENCY_LIGHTING_VOLUME");
	class FNumSamplesPerVoxel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_VOXEL_1D", 2, 4);
	class FLightSoftFading : SHADER_PERMUTATION_BOOL("USE_LIGHT_SOFT_FADING");
	class FUseLightFunctionAtlas : SHADER_PERMUTATION_BOOL("USE_LIGHT_FUNCTION_ATLAS");
	class FGuideByHistory : SHADER_PERMUTATION_BOOL("GUIDE_BY_HISTORY");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTranslucencyLightingVolume, FNumSamplesPerVoxel1d, FLightSoftFading, FUseLightFunctionAtlas, FGuideByHistory, FDebugMode>;

	static int32 GetGroupSize()
	{
		return 4;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 NumSamplesPerVoxel1d = PermutationVector.Get<FNumSamplesPerVoxel1d>();
		const FIntVector NumSamplesPerVoxel3d = MegaLights::GetNumSamplesPerVoxel3d(NumSamplesPerVoxel1d);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_VOXEL_3D_X"), NumSamplesPerVoxel3d.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_VOXEL_3D_Y"), NumSamplesPerVoxel3d.Y);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_VOXEL_3D_Z"), NumSamplesPerVoxel3d.Z);

		OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
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

IMPLEMENT_GLOBAL_SHADER(FVolumeGenerateLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsVolumeSampling.usf", "VolumeGenerateLightSamplesCS", SF_Compute);

class FClearLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FClearLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSampleRays)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileData)
	END_SHADER_PARAMETER_STRUCT()

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
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

IMPLEMENT_GLOBAL_SHADER(FClearLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsSampling.usf", "ClearLightSamplesCS", SF_Compute);

class FShadeLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadeLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FShadeLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWShadingConfidence)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, HairTransmittanceMaskTexture)
		SHADER_PARAMETER(uint32, UseShadingConfidence)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)MegaLights::ETileType::SHADING_MAX_SUBSTRATE);
	class FDownsampleFactor : SHADER_PERMUTATION_RANGE_INT("DOWNSAMPLE_FACTOR", 1, 2);
	class FNumSamplesPerPixel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL_1D", 2, 4, 16);
	class FInputType : SHADER_PERMUTATION_INT("INPUT_TYPE", int32(EMegaLightsInput::Count));
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FDownsampleFactor, FNumSamplesPerPixel1d, FInputType, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		EMegaLightsInput InputType = EMegaLightsInput(PermutationVector.Get<FInputType>());
		if (MegaLights::GetShadingTileTypes(InputType).Find(PermutationVector.Get<FTileType>()) == INDEX_NONE)
		{
			return false;
		}
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FDebugMode>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 NumSamplesPerPixel1d = PermutationVector.Get<FNumSamplesPerPixel1d>();
		const FIntPoint NumSamplesPerPixel2d = MegaLights::GetNumSamplesPerPixel2d(NumSamplesPerPixel1d);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_X"), NumSamplesPerPixel2d.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_Y"), NumSamplesPerPixel2d.Y);

		if (IsMetalPlatform(Parameters.Platform))
		{
			OutEnvironment.SetDefine(TEXT("FORCE_DISABLE_GLINTS_AA"), 1); // SUBSTRATE_TODO Temporary, while Metal compute does not have derivatives.
		}

		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FShadeLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsShading.usf", "ShadeLightSamplesCS", SF_Compute);

class FVisibleLightHashCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisibleLightHashCS)
	SHADER_USE_PARAMETER_STRUCT(FVisibleLightHashCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWVisibleLightHash)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWVisibleLightMaskHash)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSampleRays)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FNumSamplesPerPixel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL_1D", 2, 4, 16);
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerPixel1d, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FDebugMode>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 NumSamplesPerPixel1d = PermutationVector.Get<FNumSamplesPerPixel1d>();
		const FIntPoint NumSamplesPerPixel2d = MegaLights::GetNumSamplesPerPixel2d(NumSamplesPerPixel1d);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_X"), NumSamplesPerPixel2d.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_Y"), NumSamplesPerPixel2d.Y);

		OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisibleLightHashCS, "/Engine/Private/MegaLights/MegaLightsVisibleLightHash.usf", "VisibleLightHashCS", SF_Compute);

class FVolumeShadeLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVolumeShadeLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FVolumeShadeLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeResolvedLighting)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWTranslucencyVolumeResolvedLightingAmbient)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWTranslucencyVolumeResolvedLightingDirectional)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsVolumeParameters, MegaLightsVolumeParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, VolumeLightSamples)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 4;
	}

	class FTranslucencyLightingVolume : SHADER_PERMUTATION_BOOL("TRANSLUCENCY_LIGHTING_VOLUME");
	class FDownsampleFactor : SHADER_PERMUTATION_RANGE_INT("VOLUME_DOWNSAMPLE_FACTOR", 1, 2);
	class FNumSamplesPerVoxel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_VOXEL_1D", 2, 4);
	class FLightSoftFading : SHADER_PERMUTATION_BOOL("USE_LIGHT_SOFT_FADING");
	class FUseLightFunctionAtlas : SHADER_PERMUTATION_BOOL("USE_LIGHT_FUNCTION_ATLAS");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTranslucencyLightingVolume, FDownsampleFactor, FNumSamplesPerVoxel1d, FLightSoftFading, FUseLightFunctionAtlas, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 NumSamplesPerVoxel1d = PermutationVector.Get<FNumSamplesPerVoxel1d>();
		const FIntVector NumSamplesPerVoxel3d = MegaLights::GetNumSamplesPerVoxel3d(NumSamplesPerVoxel1d);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_VOXEL_3D_X"), NumSamplesPerVoxel3d.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_VOXEL_3D_Y"), NumSamplesPerVoxel3d.Y);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_VOXEL_3D_Z"), NumSamplesPerVoxel3d.Z);

		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
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

IMPLEMENT_GLOBAL_SHADER(FVolumeShadeLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsVolumeShading.usf", "VolumeShadeLightSamplesCS", SF_Compute);

class FVolumeVisibleLightHashCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVolumeVisibleLightHashCS)
	SHADER_USE_PARAMETER_STRUCT(FVolumeVisibleLightHashCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWVisibleLightHash)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, LightSamples)
		SHADER_PARAMETER(FIntVector, VolumeVisibleLightHashTileSize)
		SHADER_PARAMETER(FIntVector, VolumeVisibleLightHashViewSizeInTiles)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 4;
	}

	class FNumSamplesPerVoxel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_VOXEL_1D", 2, 4);
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerVoxel1d, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FDebugMode>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 NumSamplesPerPixel1d = PermutationVector.Get<FNumSamplesPerVoxel1d>();
		const int32 NumSamplesPerVoxel1d = PermutationVector.Get<FNumSamplesPerVoxel1d>();
		const FIntVector NumSamplesPerVoxel3d = MegaLights::GetNumSamplesPerVoxel3d(NumSamplesPerVoxel1d);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_VOXEL_3D_X"), NumSamplesPerVoxel3d.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_VOXEL_3D_Y"), NumSamplesPerVoxel3d.Y);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_VOXEL_3D_Z"), NumSamplesPerVoxel3d.Z);

		OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVolumeVisibleLightHashCS, "/Engine/Private/MegaLights/MegaLightsVisibleLightHash.usf", "VolumeVisibleLightHashCS", SF_Compute);

class FVolumeFilterVisibleLightHashCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVolumeFilterVisibleLightHashCS)
	SHADER_USE_PARAMETER_STRUCT(FVolumeFilterVisibleLightHashCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER(FIntVector, VolumeVisibleLightHashViewSizeInTiles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWVisibleLightHash)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, VisibleLightHashBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 4;
	}

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FDebugMode>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVolumeFilterVisibleLightHashCS, "/Engine/Private/MegaLights/MegaLightsFilterVisibleLightHash.usf", "VolumeFilterVisibleLightHashCS", SF_Compute);

class FClearResolvedLightingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearResolvedLightingCS)
	SHADER_USE_PARAMETER_STRUCT(FClearResolvedLightingCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileData)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearResolvedLightingCS, "/Engine/Private/MegaLights/MegaLightsShading.usf", "ClearResolvedLightingCS", SF_Compute);

class FDenoiserTemporalCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDenoiserTemporalCS)
	SHADER_USE_PARAMETER_STRUCT(FDenoiserTemporalCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ResolvedDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ShadingConfidenceTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, DiffuseLightingAndSecondMomentHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SpecularLightingAndSecondMomentHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float>, NumFramesAccumulatedHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, MegaLightsDepthHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, MegaLightsNormalAndShading)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryGatherUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryBufferSizeAndInvSize)
		SHADER_PARAMETER(float, PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER(float, MinFramesAccumulatedForHistoryMiss)
		SHADER_PARAMETER(float, MinFramesAccumulatedForHighConfidence)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWDiffuseLightingAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSpecularLightingAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float>, RWNumFramesAccumulated)
	END_SHADER_PARAMETER_STRUCT()

	class FValidHistory : SHADER_PERMUTATION_BOOL("VALID_HISTORY");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	class FInputType : SHADER_PERMUTATION_INT("INPUT_TYPE", int32(EMegaLightsInput::Count));
	using FPermutationDomain = TShaderPermutationDomain<FValidHistory, FDebugMode, FInputType>;

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		EMegaLightsInput InputType = EMegaLightsInput(PermutationVector.Get<FInputType>());
		if (PermutationVector.Get<FValidHistory>() && !MegaLights::SupportsTemporalFilter(InputType))
		{
			return false;
		}
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
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

IMPLEMENT_GLOBAL_SHADER(FDenoiserTemporalCS, "/Engine/Private/MegaLights/MegaLightsDenoiserTemporal.usf", "DenoiserTemporalCS", SF_Compute);

class FDenoiserSpatialCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDenoiserSpatialCS)
	SHADER_USE_PARAMETER_STRUCT(FDenoiserSpatialCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSceneColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, DiffuseLightingAndSecondMomentTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, SpecularLightingAndSecondMomentTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ShadingConfidenceTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float>, NumFramesAccumulatedTexture)
		SHADER_PARAMETER(float, SpatialFilterDepthWeightScale)
		SHADER_PARAMETER(float, SpatialFilterKernelRadius)
		SHADER_PARAMETER(uint32, SpatialFilterNumSamples)
		SHADER_PARAMETER(float, SpatialFilterMaxDisocclusionFrames)
	END_SHADER_PARAMETER_STRUCT()

	class FSpatialFilter : SHADER_PERMUTATION_BOOL("SPATIAL_FILTER");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	class FInputType : SHADER_PERMUTATION_INT("INPUT_TYPE", int32(EMegaLightsInput::Count));
	using FPermutationDomain = TShaderPermutationDomain<FSpatialFilter, FDebugMode, FInputType>;

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		EMegaLightsInput InputType = EMegaLightsInput(PermutationVector.Get<FInputType>());
		if (PermutationVector.Get<FSpatialFilter>() && !MegaLights::SupportsSpatialFilter(InputType))
		{
			return false;
		}
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
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

IMPLEMENT_GLOBAL_SHADER(FDenoiserSpatialCS, "/Engine/Private/MegaLights/MegaLightsDenoiserSpatial.usf", "DenoiserSpatialCS", SF_Compute);

class FFilterVisibleLightHashCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFilterVisibleLightHashCS)
	SHADER_USE_PARAMETER_STRUCT(FFilterVisibleLightHashCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWVisibleLightHash)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWVisibleLightMaskHash)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, VisibleLightHashBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, VisibleLightMaskHashBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FDebugMode>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFilterVisibleLightHashCS, "/Engine/Private/MegaLights/MegaLightsFilterVisibleLightHash.usf", "FilterVisibleLightHashCS", SF_Compute);

class FMegaLightHairTransmittanceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMegaLightHairTransmittanceCS)
	SHADER_USE_PARAMETER_STRUCT(FMegaLightHairTransmittanceCS, FGlobalShader)

	class FNumSamplesPerPixel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL_1D", 2, 4, 16);
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerPixel1d>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, VirtualVoxel)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSampleRays)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWTransmittanceMaskTexture)
	END_SHADER_PARAMETER_STRUCT()

		static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.SetDefine(TEXT("INPUT_TYPE"), TEXT("INPUT_TYPE_HAIRSTRANDS"));

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 NumSamplesPerPixel1d = PermutationVector.Get<FNumSamplesPerPixel1d>();
		const FIntPoint NumSamplesPerPixel2d = MegaLights::GetNumSamplesPerPixel2d(NumSamplesPerPixel1d);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_X"), NumSamplesPerPixel2d.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_Y"), NumSamplesPerPixel2d.Y);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMegaLightHairTransmittanceCS, "/Engine/Private/MegaLights/MegaLights.usf", "HairTransmittanceCS", SF_Compute);

DECLARE_GPU_STAT(MegaLights);

extern int32 GetTranslucencyLightingVolumeDim();

/**
 * Single pass batched light rendering using ray tracing (distance field or triangle) for stochastic light (BRDF and visibility) sampling.
 */
static void InternalRenderMegaLights(
	FRDGBuilder& GraphBuilder, 
	const int32 ViewIndex,
	const FViewInfo& View,
	const FSceneViewFamily& ViewFamily,
	const FScene* Scene,
	const FSceneTextures& SceneTextures, 
	FRDGTextureRef LightingChannelsTexture, 
	const FSortedLightSetSceneInfo& SortedLightSet,
	const FVirtualShadowMapArray* VirtualShadowMapArray,
	FMegaLightsVolume* MegaLightsVolume,
	const bool bShouldRenderVolumetricFog,
	const bool bShouldRenderTranslucencyVolume,
	TUniformBufferRef<FBlueNoise> BlueNoiseUniformBuffer, 
	EMegaLightsInput InputType,
	FRDGTextureRef OutputColorTarget)
{
	{
		// History reset for debugging purposes
		bool bResetHistory = false;

		if (GMegaLightsResetEveryNthFrame > 0 && (ViewFamily.FrameNumber % (uint32)GMegaLightsResetEveryNthFrame) == 0)
		{
			bResetHistory = true;
		}

		if (GMegaLightsReset != 0)
		{
			GMegaLightsReset = 0;
			bResetHistory = true;
		}

		if (MegaLightsVolume)
		{
			MegaLightsVolume->Texture = nullptr;
		}

		const bool bDebug = MegaLights::GetDebugMode(InputType) != 0;
		const bool bVolumeDebug = MegaLightsVolume::GetDebugMode() != 0;
		const bool bTranslucencyVolumeDebug = MegaLightsTranslucencyVolume::GetDebugMode() != 0;

		const FIntPoint NumSamplesPerPixel2d = MegaLights::GetNumSamplesPerPixel2d(InputType);
		const FIntVector NumSamplesPerVoxel3d = MegaLightsVolume::GetNumSamplesPerVoxel3d();
		const FIntVector NumSamplesPerTranslucencyVoxel3d = MegaLightsTranslucencyVolume::GetNumSamplesPerVoxel3d();

		const uint32 DownsampleFactor = MegaLights::GetDownsampleFactor(InputType);
		const FIntPoint DownsampledViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), DownsampleFactor);
		const FIntPoint SampleViewSize = DownsampledViewSize * NumSamplesPerPixel2d;
		const FIntPoint DownsampledBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, DownsampleFactor);
		const FIntPoint SampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;
		const FIntPoint DonwnsampledSampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;

		FRDGTextureRef DownsampledSceneDepth = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.DownsampledSceneDepth"));

		FRDGTextureRef DownsampledSceneWorldNormal = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.DownsampledSceneWorldNormal"));

		FRDGTextureRef LightSamples = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.LightSamples"));

		FRDGTextureRef LightSampleRays = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.LightSampleRays"));

		const bool bSpatial  = MegaLights::SupportsSpatialFilter(InputType) && CVarMegaLightsSpatial.GetValueOnRenderThread() != 0;
		const bool bTemporal = MegaLights::SupportsTemporalFilter(InputType) && CVarMegaLightsTemporal.GetValueOnRenderThread() != 0;

		const FIntPoint VisibleLightHashSizeInTiles = FMath::DivideAndRoundUp<FIntPoint>(SceneTextures.Config.Extent, MegaLights::TileSize);
		const FIntPoint VisibleLightHashViewMinInTiles = FMath::DivideAndRoundUp<FIntPoint>(View.ViewRect.Min, MegaLights::TileSize);
		const FIntPoint VisibleLightHashViewSizeInTiles = FMath::DivideAndRoundUp<FIntPoint>(View.ViewRect.Size(), MegaLights::TileSize);
		const uint32 VisibleLightHashBufferSize = VisibleLightHashSizeInTiles.X * VisibleLightHashSizeInTiles.Y * MegaLights::VisibleLightHashSize;

		FVolumetricFogGlobalData VolumetricFogParamaters;
		if (bShouldRenderVolumetricFog)
		{
			SetupVolumetricFogGlobalData(View, VolumetricFogParamaters);
		}

		const uint32 VolumeDownsampleFactor = MegaLightsVolume::GetDownsampleFactor();
		const FIntVector VolumeViewSize = VolumetricFogParamaters.ViewGridSizeInt;
		const FIntVector VolumeBufferSize = VolumetricFogParamaters.ResourceGridSizeInt;
		const FIntVector VolumeDownsampledBufferSize = FIntVector::DivideAndRoundUp(VolumetricFogParamaters.ResourceGridSizeInt, VolumeDownsampleFactor);
		const FIntVector VolumeDownsampledViewSize = FIntVector::DivideAndRoundUp(VolumetricFogParamaters.ViewGridSizeInt, VolumeDownsampleFactor);
		const FIntVector VolumeSampleViewSize = VolumeDownsampledViewSize * NumSamplesPerVoxel3d;
		const FIntVector VolumeSampleBufferSize = VolumeDownsampledBufferSize * NumSamplesPerVoxel3d;

		const FIntVector VolumeVisibleLightHashTileSize = FIntVector(2, 2, 1);

		const FIntVector VolumeVisibleLightHashSizeInTiles = FIntVector(
			FMath::DivideAndRoundUp(VolumeDownsampledBufferSize.X, VolumeVisibleLightHashTileSize.X),
			FMath::DivideAndRoundUp(VolumeDownsampledBufferSize.Y, VolumeVisibleLightHashTileSize.Y),
			FMath::DivideAndRoundUp(VolumeDownsampledBufferSize.Z, VolumeVisibleLightHashTileSize.Z));
		const FIntVector VolumeVisibleLightHashViewSizeInTiles = FIntVector(
			FMath::DivideAndRoundUp(VolumeDownsampledViewSize.X, VolumeVisibleLightHashTileSize.X),
			FMath::DivideAndRoundUp(VolumeDownsampledViewSize.Y, VolumeVisibleLightHashTileSize.Y),
			FMath::DivideAndRoundUp(VolumeDownsampledViewSize.Z, VolumeVisibleLightHashTileSize.Z));
		const uint32 VolumeVisibleLightHashBufferSize = VolumeVisibleLightHashSizeInTiles.X * VolumeVisibleLightHashSizeInTiles.Y * VolumeVisibleLightHashSizeInTiles.Z * MegaLights::VisibleLightHashSize;

		const uint32 TranslucencyVolumeDownsampleFactor = MegaLightsTranslucencyVolume::GetDownsampleFactor();
		const FIntVector TranslucencyVolumeBufferSize = FIntVector(GetTranslucencyLightingVolumeDim());
		const FIntVector TranslucencyVolumeDownsampledBufferSize = FIntVector::DivideAndRoundUp(TranslucencyVolumeBufferSize, TranslucencyVolumeDownsampleFactor);
		const FIntVector TranslucencyVolumeSampleBufferSize = TranslucencyVolumeDownsampledBufferSize * NumSamplesPerTranslucencyVoxel3d;

		const FIntVector TranslucencyVolumeVisibleLightHashTileSize = FIntVector(1, 1, 1);

		const FIntVector TranslucencyVolumeVisibleLightHashSizeInTiles = FIntVector(
			FMath::DivideAndRoundUp(TranslucencyVolumeDownsampledBufferSize.X, TranslucencyVolumeVisibleLightHashTileSize.X),
			FMath::DivideAndRoundUp(TranslucencyVolumeDownsampledBufferSize.Y, TranslucencyVolumeVisibleLightHashTileSize.Y),
			FMath::DivideAndRoundUp(TranslucencyVolumeDownsampledBufferSize.Z, TranslucencyVolumeVisibleLightHashTileSize.Z));
		const uint32 TranslucencyVolumeVisibleLightHashBufferSize =
			TranslucencyVolumeVisibleLightHashSizeInTiles.X *
			TranslucencyVolumeVisibleLightHashSizeInTiles.Y *
			TranslucencyVolumeVisibleLightHashSizeInTiles.Z *
			MegaLights::VisibleLightHashSize;

		const bool bGuideByHistory = MegaLights::SupportsGuideByHistory(InputType) && CVarMegaLightsGuideByHistory.GetValueOnRenderThread() != 0;
		const bool bVolumeGuideByHistory = MegaLights::SupportsGuideByHistory(InputType) && CVarMegaLightsVolumeGuideByHistory.GetValueOnRenderThread() != 0;
		const bool bTranslucencyVolumeGuideByHistory = MegaLights::SupportsGuideByHistory(InputType) && CVarMegaLightsTranslucencyVolumeGuideByHistory.GetValueOnRenderThread() != 0;

		FVector4f HistoryScreenPositionScaleBias = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FVector4f HistoryGatherUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FVector4f HistoryBufferSizeAndInvSize = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FIntPoint HistoryVisibleLightHashViewMinInTiles = 0;
		FIntPoint HistoryVisibleLightHashViewSizeInTiles = 0;
		FRDGTextureRef DiffuseLightingAndSecondMomentHistory = nullptr;
		FRDGTextureRef SpecularLightingAndSecondMomentHistory = nullptr;
		FRDGTextureRef SceneDepthHistory = nullptr;
		FRDGTextureRef SceneNormalAndShadingHistory = nullptr;
		FRDGTextureRef NumFramesAccumulatedHistory = nullptr;
		FRDGBufferRef VisibleLightHashHistory = nullptr;
		FRDGBufferRef VisibleLightMaskHashHistory = nullptr;

		FIntVector HistoryVolumeVisibleLightHashViewSizeInTiles = FIntVector::ZeroValue;
		FRDGBufferRef VolumeVisibleLightHashHistory = nullptr;

		FIntVector HistoryTranslucencyVolumeVisibleLightHashSizeInTiles = FIntVector::ZeroValue;
		FRDGBufferRef TranslucencyVolumeVisibleLightHashHistory[TVC_MAX] = {};

		if (View.ViewState)
		{
			const FMegaLightsViewState& MegaLightsViewState = View.ViewState->MegaLights;
			const FStochasticLightingViewState& StochasticLightingViewState = View.ViewState->StochasticLighting;

			if (!View.bCameraCut 
				&& !View.bPrevTransformsReset
				&& !bResetHistory)
			{
				HistoryScreenPositionScaleBias = MegaLightsViewState.HistoryScreenPositionScaleBias;
				HistoryUVMinMax = MegaLightsViewState.HistoryUVMinMax;
				HistoryGatherUVMinMax = MegaLightsViewState.HistoryGatherUVMinMax;
				HistoryBufferSizeAndInvSize = MegaLightsViewState.HistoryBufferSizeAndInvSize;
				HistoryVisibleLightHashViewMinInTiles = MegaLightsViewState.HistoryVisibleLightHashViewMinInTiles;
				HistoryVisibleLightHashViewSizeInTiles = MegaLightsViewState.HistoryVisibleLightHashViewSizeInTiles;

				HistoryVolumeVisibleLightHashViewSizeInTiles = MegaLightsViewState.HistoryVolumeVisibleLightHashViewSizeInTiles;
				HistoryTranslucencyVolumeVisibleLightHashSizeInTiles = MegaLightsViewState.HistoryTranslucencyVolumeVisibleLightHashSizeInTiles;

				if (StochasticLightingViewState.SceneDepthHistory)
				{
					SceneDepthHistory = GraphBuilder.RegisterExternalTexture(StochasticLightingViewState.SceneDepthHistory);
				}

				if (StochasticLightingViewState.SceneNormalHistory)
				{
					SceneNormalAndShadingHistory = GraphBuilder.RegisterExternalTexture(StochasticLightingViewState.SceneNormalHistory);
				}

				if (bTemporal &&
					MegaLightsViewState.DiffuseLightingAndSecondMomentHistory
					&& MegaLightsViewState.SpecularLightingAndSecondMomentHistory
					&& MegaLightsViewState.NumFramesAccumulatedHistory)
				{
					DiffuseLightingAndSecondMomentHistory = GraphBuilder.RegisterExternalTexture(MegaLightsViewState.DiffuseLightingAndSecondMomentHistory);
					SpecularLightingAndSecondMomentHistory = GraphBuilder.RegisterExternalTexture(MegaLightsViewState.SpecularLightingAndSecondMomentHistory);
					NumFramesAccumulatedHistory = GraphBuilder.RegisterExternalTexture(MegaLightsViewState.NumFramesAccumulatedHistory);
				}

				if (bGuideByHistory
					&& MegaLightsViewState.VisibleLightHashHistory
					&& MegaLightsViewState.VisibleLightMaskHashHistory)
				{
					VisibleLightHashHistory = GraphBuilder.RegisterExternalBuffer(MegaLightsViewState.VisibleLightHashHistory);
					VisibleLightMaskHashHistory = GraphBuilder.RegisterExternalBuffer(MegaLightsViewState.VisibleLightMaskHashHistory);
				}

				if (bVolumeGuideByHistory
					&& MegaLightsViewState.VolumeVisibleLightHashHistory)
				{
					VolumeVisibleLightHashHistory = GraphBuilder.RegisterExternalBuffer(MegaLightsViewState.VolumeVisibleLightHashHistory);
				}

				if (bTranslucencyVolumeGuideByHistory
					&& MegaLightsViewState.TranslucencyVolume0VisibleLightHashHistory
					&& MegaLightsViewState.TranslucencyVolume1VisibleLightHashHistory
					&& TranslucencyVolumeVisibleLightHashBufferSize == MegaLightsViewState.TranslucencyVolume0VisibleLightHashHistory->GetSize() / sizeof(uint32)
					&& TranslucencyVolumeVisibleLightHashBufferSize == MegaLightsViewState.TranslucencyVolume1VisibleLightHashHistory->GetSize() / sizeof(uint32))
				{
					TranslucencyVolumeVisibleLightHashHistory[0] = GraphBuilder.RegisterExternalBuffer(MegaLightsViewState.TranslucencyVolume0VisibleLightHashHistory);
					TranslucencyVolumeVisibleLightHashHistory[1] = GraphBuilder.RegisterExternalBuffer(MegaLightsViewState.TranslucencyVolume1VisibleLightHashHistory);
				}
			}
		}

		// Setup the light function atlas
		const bool bUseLightFunctionAtlas = LightFunctionAtlas::IsEnabled(View, LightFunctionAtlas::ELightFunctionAtlasSystem::MegaLights);

		const FIntPoint ViewSizeInTiles = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), MegaLights::TileSize);
		const int32 TileDataStride = ViewSizeInTiles.X * ViewSizeInTiles.Y;

		const FIntPoint DownsampledViewSizeInTiles = FIntPoint::DivideAndRoundUp(DownsampledViewSize, MegaLights::TileSize);
		const int32 DownsampledTileDataStride = DownsampledViewSizeInTiles.X * DownsampledViewSizeInTiles.Y;

		FMegaLightsParameters MegaLightsParameters;
		{
			MegaLightsParameters.ViewUniformBuffer = View.ViewUniformBuffer;
			MegaLightsParameters.Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
			MegaLightsParameters.SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
			MegaLightsParameters.SceneTexturesStruct = SceneTextures.UniformBuffer;
			MegaLightsParameters.Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
			MegaLightsParameters.HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
			MegaLightsParameters.ForwardLightStruct = View.ForwardLightingResources.ForwardLightUniformBuffer;
			MegaLightsParameters.LightFunctionAtlas = LightFunctionAtlas::BindGlobalParameters(GraphBuilder, View);
			MegaLightsParameters.LightingChannelParameters = GetSceneLightingChannelParameters(GraphBuilder, View, LightingChannelsTexture);
			MegaLightsParameters.BlueNoise = BlueNoiseUniformBuffer;
			MegaLightsParameters.PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRHI();
			MegaLightsParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			MegaLightsParameters.UnjitteredClipToTranslatedWorld = FMatrix44f(View.ViewMatrices.ComputeInvProjectionNoAAMatrix() * View.ViewMatrices.GetTranslatedViewMatrix().GetTransposed()); // LWC_TODO: Precision loss?
			MegaLightsParameters.UnjitteredPrevTranslatedWorldToClip = FMatrix44f(FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation()) * View.PrevViewInfo.ViewMatrices.GetViewMatrix() * View.PrevViewInfo.ViewMatrices.ComputeProjectionNoAAMatrix());

			MegaLightsParameters.DownsampledViewMin = FIntPoint::DivideAndRoundUp(View.ViewRect.Min, DownsampleFactor);
			MegaLightsParameters.DownsampledViewSize = DownsampledViewSize;
			MegaLightsParameters.SampleViewMin = FIntPoint::DivideAndRoundUp(View.ViewRect.Min, DownsampleFactor) * NumSamplesPerPixel2d;
			MegaLightsParameters.SampleViewSize = SampleViewSize;
			MegaLightsParameters.DownsampleFactorMultShift = FMath::FloorLog2(DownsampleFactor);
			MegaLightsParameters.NumSamplesPerPixel = NumSamplesPerPixel2d;
			MegaLightsParameters.NumSamplesPerPixelDivideShift.X = FMath::FloorLog2(NumSamplesPerPixel2d.X);
			MegaLightsParameters.NumSamplesPerPixelDivideShift.Y = FMath::FloorLog2(NumSamplesPerPixel2d.Y);
			MegaLightsParameters.MegaLightsStateFrameIndex = MegaLights::GetStateFrameIndex(View.ViewState);
			MegaLightsParameters.DownsampledSceneDepth = DownsampledSceneDepth;
			MegaLightsParameters.DownsampledSceneWorldNormal = DownsampledSceneWorldNormal;
			MegaLightsParameters.DownsampledBufferInvSize = FVector2f(1.0f) / DownsampledBufferSize;
			MegaLightsParameters.MinSampleWeight = FMath::Max(CVarMegaLightsMinSampleWeight.GetValueOnRenderThread(), 0.0f);
			MegaLightsParameters.MaxShadingWeight = FMath::Max(CVarMegaLightsMaxShadingWeight.GetValueOnRenderThread(), 0.0f);
			MegaLightsParameters.TileDataStride = TileDataStride;
			MegaLightsParameters.DownsampledTileDataStride = DownsampledTileDataStride;
			MegaLightsParameters.TemporalMaxFramesAccumulated = MegaLights::GetTemporalMaxFramesAccumulated();
			MegaLightsParameters.TemporalNeighborhoodClampScale = CVarMegaLightsTemporalNeighborhoodClampScale.GetValueOnRenderThread();
			MegaLightsParameters.DebugCursorPosition.X = CVarMegaLightsDebugCursorX.GetValueOnRenderThread();
			MegaLightsParameters.DebugCursorPosition.Y = CVarMegaLightsDebugCursorY.GetValueOnRenderThread();
			MegaLightsParameters.DebugMode = MegaLights::GetDebugMode(InputType);
			MegaLightsParameters.DebugLightId = INDEX_NONE;
			MegaLightsParameters.DebugVisualizeLight = CVarMegaLightsDebugVisualizeLight.GetValueOnRenderThread();
			MegaLightsParameters.UseIESProfiles = CVarMegaLightsIESProfiles.GetValueOnRenderThread() != 0;
			MegaLightsParameters.UseLightFunctionAtlas = bUseLightFunctionAtlas;

			// If editor is disabled then we don't have a valid cursor position and have to force it to the center of the screen
			if (!GIsEditor && (MegaLightsParameters.DebugCursorPosition.X < 0 || MegaLightsParameters.DebugCursorPosition.Y < 0))
			{		
				MegaLightsParameters.DebugCursorPosition.X = View.ViewRect.Min.X + View.ViewRect.Width() / 2;
				MegaLightsParameters.DebugCursorPosition.Y = View.ViewRect.Min.Y + View.ViewRect.Height() / 2;
			}

			// screen traces use ClosestHZB, volume sampling/shading uses FurthestHZB
			MegaLightsParameters.HZBParameters = GetHZBParameters(GraphBuilder, View, EHZBType::All);
			MegaLightsParameters.VisibleLightHashViewMinInTiles = VisibleLightHashViewMinInTiles;
			MegaLightsParameters.VisibleLightHashViewSizeInTiles = VisibleLightHashViewSizeInTiles;

			if (bDebug || bVolumeDebug || bTranslucencyVolumeDebug)
			{
				const FIntPoint TileCountXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), MegaLights::TileSize);
				const uint32 TileCount = TileCountXY.X * TileCountXY.Y;

				ShaderPrint::SetEnabled(true);
				ShaderPrint::RequestSpaceForLines(4096u + TileCount * 4u);
				ShaderPrint::RequestSpaceForTriangles(TileCount * 2u);
				ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, MegaLightsParameters.ShaderPrintUniformBuffer);

				MegaLightsParameters.DebugLightId = CVarMegaLightsDebugLightId.GetValueOnRenderThread();

				if (MegaLightsParameters.DebugLightId < 0)
				{
					for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
					{
						const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
						const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

						if (LightSceneInfo->Proxy->IsSelected())
						{
							MegaLightsParameters.DebugLightId = LightSceneInfo->Id;
							break;
						}
					}
				}
			}
		}

		FMegaLightsVolumeParameters MegaLightsVolumeParameters;
		{
			extern float GInverseSquaredLightDistanceBiasScale;
			MegaLightsVolumeParameters.VolumeMinSampleWeight = FMath::Max(CVarMegaLightsVolumeMinSampleWeight.GetValueOnRenderThread(), 0.0f);
			MegaLightsVolumeParameters.VolumeMaxShadingWeight = FMath::Max(CVarMegaLightsVolumeMaxShadingWeight.GetValueOnRenderThread(), 0.0f);
			MegaLightsVolumeParameters.VolumeDownsampleFactorMultShift = FMath::FloorLog2(VolumeDownsampleFactor);
			MegaLightsVolumeParameters.NumSamplesPerVoxel = NumSamplesPerVoxel3d;
			MegaLightsVolumeParameters.NumSamplesPerVoxelDivideShift.X = FMath::FloorLog2(NumSamplesPerVoxel3d.X);
			MegaLightsVolumeParameters.NumSamplesPerVoxelDivideShift.Y = FMath::FloorLog2(NumSamplesPerVoxel3d.Y);
			MegaLightsVolumeParameters.NumSamplesPerVoxelDivideShift.Z = FMath::FloorLog2(NumSamplesPerVoxel3d.Z);
			MegaLightsVolumeParameters.DownsampledVolumeViewSize = VolumeDownsampledViewSize;
			MegaLightsVolumeParameters.VolumeViewSize = VolumeViewSize;
			MegaLightsVolumeParameters.VolumeSampleViewSize = VolumeSampleViewSize;
			MegaLightsVolumeParameters.MegaLightsVolumeZParams = VolumetricFogParamaters.GridZParams;
			MegaLightsVolumeParameters.MegaLightsVolumePixelSize = VolumetricFogParamaters.FogGridToPixelXY.X;
			MegaLightsVolumeParameters.VolumePhaseG = Scene->ExponentialFogs.Num() > 0 ? Scene->ExponentialFogs[0].VolumetricFogScatteringDistribution : 0.0f;
			MegaLightsVolumeParameters.VolumeInverseSquaredLightDistanceBiasScale = GInverseSquaredLightDistanceBiasScale;
			MegaLightsVolumeParameters.VolumeFrameJitterOffset = VolumetricFogTemporalRandom(View.Family->FrameNumber);
			MegaLightsVolumeParameters.UseHZBOcclusionTest = CVarMegaLightsVolumeHZBOcclusionTest.GetValueOnRenderThread();
			MegaLightsVolumeParameters.VolumeDebugMode = MegaLightsVolume::GetDebugMode();
			MegaLightsVolumeParameters.VolumeDebugSliceIndex = CVarMegaLightsVolumeDebugSliceIndex.GetValueOnRenderThread();
			MegaLightsVolumeParameters.LightSoftFading = GetVolumetricFogLightSoftFading();
			MegaLightsVolumeParameters.TranslucencyVolumeCascadeIndex = 0;
			MegaLightsVolumeParameters.TranslucencyVolumeInvResolution = 0.0f;
		}

		FMegaLightsVolumeParameters MegaLightsTranslucencyVolumeParameters;
		{
			extern float GInverseSquaredLightDistanceBiasScale;
			MegaLightsTranslucencyVolumeParameters.VolumeMinSampleWeight = FMath::Max(CVarMegaLightsTranslucencyVolumeMinSampleWeight.GetValueOnRenderThread(), 0.0f);
			MegaLightsTranslucencyVolumeParameters.VolumeMaxShadingWeight = FMath::Max(CVarMegaLightsTranslucencyVolumeMaxShadingWeight.GetValueOnRenderThread(), 0.0f);
			MegaLightsTranslucencyVolumeParameters.VolumeDownsampleFactorMultShift = FMath::FloorLog2(TranslucencyVolumeDownsampleFactor);
			MegaLightsTranslucencyVolumeParameters.NumSamplesPerVoxel = NumSamplesPerTranslucencyVoxel3d;
			MegaLightsTranslucencyVolumeParameters.NumSamplesPerVoxelDivideShift.X = FMath::FloorLog2(NumSamplesPerTranslucencyVoxel3d.X);
			MegaLightsTranslucencyVolumeParameters.NumSamplesPerVoxelDivideShift.Y = FMath::FloorLog2(NumSamplesPerTranslucencyVoxel3d.Y);
			MegaLightsTranslucencyVolumeParameters.NumSamplesPerVoxelDivideShift.Z = FMath::FloorLog2(NumSamplesPerTranslucencyVoxel3d.Z);
			MegaLightsTranslucencyVolumeParameters.DownsampledVolumeViewSize = TranslucencyVolumeDownsampledBufferSize;
			MegaLightsTranslucencyVolumeParameters.VolumeViewSize = TranslucencyVolumeBufferSize;
			MegaLightsTranslucencyVolumeParameters.VolumeSampleViewSize = TranslucencyVolumeSampleBufferSize;
			MegaLightsTranslucencyVolumeParameters.MegaLightsVolumeZParams = VolumetricFogParamaters.GridZParams;
			MegaLightsTranslucencyVolumeParameters.MegaLightsVolumePixelSize = VolumetricFogParamaters.FogGridToPixelXY.X;
			MegaLightsTranslucencyVolumeParameters.VolumePhaseG = 0.0f;
			MegaLightsTranslucencyVolumeParameters.VolumeInverseSquaredLightDistanceBiasScale = 1.0f;
			MegaLightsTranslucencyVolumeParameters.VolumeFrameJitterOffset = FVector3f::ZeroVector;
			MegaLightsTranslucencyVolumeParameters.UseHZBOcclusionTest = false;
			MegaLightsTranslucencyVolumeParameters.VolumeDebugMode = MegaLightsTranslucencyVolume::GetDebugMode();
			MegaLightsTranslucencyVolumeParameters.VolumeDebugSliceIndex = 0;
			MegaLightsTranslucencyVolumeParameters.LightSoftFading = GetVolumetricFogLightSoftFading();
			MegaLightsTranslucencyVolumeParameters.TranslucencyVolumeCascadeIndex = 0;
			MegaLightsTranslucencyVolumeParameters.TranslucencyVolumeInvResolution = 1.0f / GetTranslucencyLightingVolumeDim();
		}

		const int32 TileTypeCount = Substrate::IsSubstrateEnabled() ? (int32)MegaLights::ETileType::MAX_SUBSTRATE : (int32)MegaLights::ETileType::MAX_LEGACY;
		FRDGBufferRef TileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileTypeCount), TEXT("MegaLights.TileAllocator"));
		FRDGBufferRef TileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileDataStride * TileTypeCount), TEXT("MegaLights.TileData"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TileAllocator), 0);

		FRDGBufferRef DownsampledTileAllocator = TileAllocator;
		FRDGBufferRef DownsampledTileData = TileData;

		// #ml_todo: merge classification passes or reuse downsampled one to create full res tiles
		// Run tile classification to generate tiles for the subsequent passes
		{
			{
				FTileClassificationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTileClassificationCS::FParameters>();
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->RWTileAllocator = GraphBuilder.CreateUAV(TileAllocator);
				PassParameters->RWTileData = GraphBuilder.CreateUAV(TileData);
				PassParameters->EnableTexturedRectLights = CVarMegaLightsTexturedRectLights.GetValueOnRenderThread();

				FTileClassificationCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FTileClassificationCS::FDownsampledClassification>(false);
				PermutationVector.Set<FTileClassificationCS::FDebugMode>(bDebug);
				PermutationVector.Set<FTileClassificationCS::FInputType>(uint32(InputType));
				auto ComputeShader = View.ShaderMap->GetShader<FTileClassificationCS>(PermutationVector);

				const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FTileClassificationCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("TileClassification %dx%d", View.ViewRect.Size().X, View.ViewRect.Size().Y),
					ComputeShader,
					PassParameters,
					GroupCount);
			}

			if (DownsampleFactor != 1)
			{
				DownsampledTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileTypeCount), TEXT("MegaLights.DownsampledTileAllocator"));
				DownsampledTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), DownsampledTileDataStride * TileTypeCount), TEXT("MegaLights.DownsampledTileData"));
				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DownsampledTileAllocator), 0);

				FTileClassificationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTileClassificationCS::FParameters>();
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->RWTileAllocator = GraphBuilder.CreateUAV(DownsampledTileAllocator);
				PassParameters->RWTileData = GraphBuilder.CreateUAV(DownsampledTileData);
				PassParameters->EnableTexturedRectLights = CVarMegaLightsTexturedRectLights.GetValueOnRenderThread();

				FTileClassificationCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FTileClassificationCS::FDownsampledClassification>(true);
				PermutationVector.Set<FTileClassificationCS::FDebugMode>(bDebug);
				PermutationVector.Set<FTileClassificationCS::FInputType>(uint32(InputType));
				auto ComputeShader = View.ShaderMap->GetShader<FTileClassificationCS>(PermutationVector);

				const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(DownsampledViewSize, FTileClassificationCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("DownsampledTileClassification %dx%d", DownsampledViewSize.X, DownsampledViewSize.Y),
					ComputeShader,
					PassParameters,
					GroupCount);
			}
		}

		FRDGBufferRef TileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(TileTypeCount), TEXT("MegaLights.TileIndirectArgs"));
		FRDGBufferRef DownsampledTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(TileTypeCount), TEXT("MegaLights.DownsampledTileIndirectArgs"));

		// Setup indirect args for classified tiles
		{
			FInitTileIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitTileIndirectArgsCS::FParameters>();
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->RWTileIndirectArgs = GraphBuilder.CreateUAV(TileIndirectArgs);
			PassParameters->RWDownsampledTileIndirectArgs = GraphBuilder.CreateUAV(DownsampledTileIndirectArgs);
			PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
			PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);

			auto ComputeShader = View.ShaderMap->GetShader<FInitTileIndirectArgsCS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("InitTileIndirectArgs"),
				ComputeShader,
				PassParameters,
				FIntVector(1, 1, 1));
		}

		// Build available tile types
		const TArray<int32> ShadingTileTypes = MegaLights::GetShadingTileTypes(InputType);

		// Generate new candidate light samples
		{
			FRDGTextureUAVRef DownsampledSceneDepthUAV = GraphBuilder.CreateUAV(DownsampledSceneDepth, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef DownsampledSceneWorldNormalUAV = GraphBuilder.CreateUAV(DownsampledSceneWorldNormal, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef LightSamplesUAV = GraphBuilder.CreateUAV(LightSamples, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef LightSampleRaysUAV = GraphBuilder.CreateUAV(LightSampleRays, ERDGUnorderedAccessViewFlags::SkipBarrier);

			// Clear tiles which don't contain any lights or geometry
			{
				FClearLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearLightSamplesCS::FParameters>();
				PassParameters->IndirectArgs = DownsampledTileIndirectArgs;
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->RWDownsampledSceneDepth = DownsampledSceneDepthUAV;
				PassParameters->RWDownsampledSceneWorldNormal = DownsampledSceneWorldNormalUAV;
				PassParameters->RWLightSamples = LightSamplesUAV;
				PassParameters->RWLightSampleRays = LightSampleRaysUAV;
				PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);
				PassParameters->DownsampledTileData = GraphBuilder.CreateSRV(DownsampledTileData);

				FClearLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FClearLightSamplesCS::FDebugMode>(bDebug);
				auto ComputeShader = View.ShaderMap->GetShader<FClearLightSamplesCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ClearLightSamples"),
					ComputeShader,
					PassParameters,
					DownsampledTileIndirectArgs,
					(int32)MegaLights::ETileType::Empty * sizeof(FRHIDispatchIndirectParameters));
			}

			for (const int32 TileType : ShadingTileTypes)
			{

				if (!View.bLightGridHasRectLights && IsRectLightTileType((MegaLights::ETileType)TileType))
				{
					continue;
				}

				if (!View.bLightGridHasTexturedLights && IsTexturedLightTileType((MegaLights::ETileType)TileType))
				{
					continue;
				}

				FGenerateLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateLightSamplesCS::FParameters>();
				PassParameters->IndirectArgs = DownsampledTileIndirectArgs;
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->RWDownsampledSceneDepth = DownsampledSceneDepthUAV;
				PassParameters->RWDownsampledSceneWorldNormal = DownsampledSceneWorldNormalUAV;
				PassParameters->RWLightSamples = LightSamplesUAV;
				PassParameters->RWLightSampleRays = LightSampleRaysUAV;
				PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);
				PassParameters->DownsampledTileData = GraphBuilder.CreateSRV(DownsampledTileData);
				PassParameters->VisibleLightHashHistory = VisibleLightHashHistory != nullptr ? GraphBuilder.CreateSRV(VisibleLightHashHistory) : nullptr;
				PassParameters->VisibleLightMaskHashHistory = VisibleLightMaskHashHistory != nullptr ? GraphBuilder.CreateSRV(VisibleLightMaskHashHistory) : nullptr;
				PassParameters->GuideByHistoryMode = CVarMegaLightsGuideByHistory.GetValueOnRenderThread();
				PassParameters->AreaLightHiddenPDFWeight = CVarMegaLightsGuideByHistoryAreaLightHiddenWeight.GetValueOnRenderThread();
				PassParameters->GuideByHistoryHiddenRatio = MegaLights::GetGuideByHistoryHiddenRatio();
				PassParameters->MegaLightsDepthHistory = SceneDepthHistory;
				PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
				PassParameters->HistoryUVMinMax = HistoryUVMinMax;
				PassParameters->HistoryGatherUVMinMax = HistoryGatherUVMinMax;
				PassParameters->HistoryBufferSizeAndInvSize = HistoryBufferSizeAndInvSize;
				PassParameters->HistoryVisibleLightHashViewMinInTiles = HistoryVisibleLightHashViewMinInTiles;
				PassParameters->HistoryVisibleLightHashViewSizeInTiles = HistoryVisibleLightHashViewSizeInTiles;

				FGenerateLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FGenerateLightSamplesCS::FTileType>(TileType);
				PermutationVector.Set<FGenerateLightSamplesCS::FNumSamplesPerPixel1d>(NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y);
				PermutationVector.Set<FGenerateLightSamplesCS::FGuideByHistory>(VisibleLightHashHistory != nullptr && SceneDepthHistory != nullptr);
				PermutationVector.Set<FGenerateLightSamplesCS::FInputType>(uint32(InputType));
				PermutationVector.Set<FGenerateLightSamplesCS::FDebugMode>(bDebug);
				auto ComputeShader = View.ShaderMap->GetShader<FGenerateLightSamplesCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("GenerateSamples DownsampleFactor:%d SamplesPerPixel:%dx%d TileType:%s", DownsampleFactor, NumSamplesPerPixel2d.X, NumSamplesPerPixel2d.Y, MegaLights::GetTileTypeString((MegaLights::ETileType)TileType)),
					ComputeShader,
					PassParameters,
					DownsampledTileIndirectArgs,
					TileType * sizeof(FRHIDispatchIndirectParameters));
			}
		}

		FRDGTextureRef VolumeLightSamples = nullptr;

		if (MegaLights::UseVolume() && bShouldRenderVolumetricFog)
		{
			VolumeLightSamples = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create3D(VolumeSampleBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
				TEXT("MegaLights.Volume.LightSamples"));

			// Generate new candidate light samples for the volume
			{
				FVolumeGenerateLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeGenerateLightSamplesCS::FParameters>();
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->MegaLightsVolumeParameters = MegaLightsVolumeParameters;
				PassParameters->VolumeVisibleLightHashHistory = VolumeVisibleLightHashHistory != nullptr ? GraphBuilder.CreateSRV(VolumeVisibleLightHashHistory) : nullptr;
				PassParameters->RWVolumeLightSamples = GraphBuilder.CreateUAV(VolumeLightSamples);
				PassParameters->HistoryVolumeVisibleLightHashViewSizeInTiles = HistoryVolumeVisibleLightHashViewSizeInTiles;
				PassParameters->VolumeVisibleLightHashTileSize = VolumeVisibleLightHashTileSize;
				PassParameters->VolumeGuideByHistoryHiddenRatio = MegaLightsVolume::GetGuideByHistoryHiddenRatio();

				FVolumeGenerateLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FVolumeGenerateLightSamplesCS::FTranslucencyLightingVolume>(false);
				PermutationVector.Set<FVolumeGenerateLightSamplesCS::FNumSamplesPerVoxel1d>(NumSamplesPerVoxel3d.X * NumSamplesPerVoxel3d.Y * NumSamplesPerVoxel3d.Z);
				PermutationVector.Set<FVolumeGenerateLightSamplesCS::FLightSoftFading>(GetVolumetricFogLightSoftFading() > 0.0f);
				PermutationVector.Set<FVolumeGenerateLightSamplesCS::FUseLightFunctionAtlas>(bUseLightFunctionAtlas && MegaLightsVolume::UsesLightFunction());
				PermutationVector.Set<FVolumeGenerateLightSamplesCS::FGuideByHistory>(VolumeVisibleLightHashHistory != nullptr);
				PermutationVector.Set<FVolumeGenerateLightSamplesCS::FDebugMode>(bVolumeDebug);

				auto ComputeShader = View.ShaderMap->GetShader<FVolumeGenerateLightSamplesCS>(PermutationVector);

				const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(VolumeDownsampledViewSize, FVolumeGenerateLightSamplesCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("VolumeGenerateSamples SamplesPerVoxel:%dx%dx%d", NumSamplesPerVoxel3d.X, NumSamplesPerVoxel3d.Y, NumSamplesPerVoxel3d.Z),
					ComputeShader,
					PassParameters,
					GroupCount);
			}
		}

		TArray<FRDGTextureRef, TInlineAllocator<TVC_MAX>> TranslucencyVolumeLightSamples;

		if (MegaLights::UseTranslucencyVolume() && bShouldRenderTranslucencyVolume)
		{
			TranslucencyVolumeLightSamples.AddDefaulted(TVC_MAX);

			for (uint32 CascadeIndex = 0; CascadeIndex < TVC_MAX; ++CascadeIndex)
			{
				TranslucencyVolumeLightSamples[CascadeIndex] = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create3D(TranslucencyVolumeSampleBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
					TEXT("MegaLights.TranslucencyVolume.LightSamples"));

				// Generate new candidate light samples for the Translucency Volume
				{
					FVolumeGenerateLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeGenerateLightSamplesCS::FParameters>();
					PassParameters->MegaLightsParameters = MegaLightsParameters;
					PassParameters->MegaLightsVolumeParameters = MegaLightsTranslucencyVolumeParameters;
					PassParameters->MegaLightsVolumeParameters.TranslucencyVolumeCascadeIndex = CascadeIndex;
					PassParameters->VolumeVisibleLightHashHistory = TranslucencyVolumeVisibleLightHashHistory[CascadeIndex] != nullptr ? GraphBuilder.CreateSRV(TranslucencyVolumeVisibleLightHashHistory[CascadeIndex]) : nullptr;
					PassParameters->RWVolumeLightSamples = GraphBuilder.CreateUAV(TranslucencyVolumeLightSamples[CascadeIndex]);
					PassParameters->HistoryVolumeVisibleLightHashViewSizeInTiles = HistoryTranslucencyVolumeVisibleLightHashSizeInTiles;
					PassParameters->VolumeVisibleLightHashTileSize = TranslucencyVolumeVisibleLightHashTileSize;
					PassParameters->VolumeGuideByHistoryHiddenRatio = MegaLightsTranslucencyVolume::GetGuideByHistoryHiddenRatio();

					FVolumeGenerateLightSamplesCS::FPermutationDomain PermutationVector;
					PermutationVector.Set<FVolumeGenerateLightSamplesCS::FTranslucencyLightingVolume>(true);
					PermutationVector.Set<FVolumeGenerateLightSamplesCS::FNumSamplesPerVoxel1d>(NumSamplesPerTranslucencyVoxel3d.X * NumSamplesPerTranslucencyVoxel3d.Y * NumSamplesPerTranslucencyVoxel3d.Z);
					PermutationVector.Set<FVolumeGenerateLightSamplesCS::FLightSoftFading >(false);
					PermutationVector.Set<FVolumeGenerateLightSamplesCS::FUseLightFunctionAtlas>(bUseLightFunctionAtlas && MegaLightsTranslucencyVolume::UsesLightFunction());
					PermutationVector.Set<FVolumeGenerateLightSamplesCS::FGuideByHistory>(TranslucencyVolumeVisibleLightHashHistory[CascadeIndex] != nullptr);
					PermutationVector.Set<FVolumeGenerateLightSamplesCS::FDebugMode>(bTranslucencyVolumeDebug);
					auto ComputeShader = View.ShaderMap->GetShader<FVolumeGenerateLightSamplesCS>(PermutationVector);

					const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(TranslucencyVolumeDownsampledBufferSize, FVolumeGenerateLightSamplesCS::GetGroupSize());

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("TranslucencyVolumeGenerateSamples SamplesPerVoxel:%dx%dx%d", NumSamplesPerTranslucencyVoxel3d.X, NumSamplesPerTranslucencyVoxel3d.Y, NumSamplesPerTranslucencyVoxel3d.Z),
						ComputeShader,
						PassParameters,
						GroupCount);
				}
			}
		}

		MegaLights::RayTraceLightSamples(
			ViewFamily,
			View, ViewIndex,
			GraphBuilder,
			SceneTextures,
			VirtualShadowMapArray,
			SampleBufferSize,
			LightSamples,
			LightSampleRays,
			VolumeSampleBufferSize,
			VolumeLightSamples,
			TranslucencyVolumeSampleBufferSize,
			TranslucencyVolumeLightSamples,
			MegaLightsParameters,
			MegaLightsVolumeParameters,
			MegaLightsTranslucencyVolumeParameters,
			InputType
		);

		// Compute transmittance estimate for hair sample
		FRDGTextureRef HairTransmittanceMaskTexture = nullptr;
		if (InputType == EMegaLightsInput::HairStrands)
		{
			HairTransmittanceMaskTexture = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R32_UINT, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV),
				TEXT("MegaLights.HairTransmittance"));

			FMegaLightHairTransmittanceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMegaLightHairTransmittanceCS::FParameters>();
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->VirtualVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
			PassParameters->LightSamples = LightSamples;
			PassParameters->LightSampleRays = LightSampleRays;
			PassParameters->RWTransmittanceMaskTexture = GraphBuilder.CreateUAV(HairTransmittanceMaskTexture);

			FMegaLightHairTransmittanceCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FMegaLightHairTransmittanceCS::FNumSamplesPerPixel1d>(NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y);
			auto ComputeShader = View.ShaderMap->GetShader<FMegaLightHairTransmittanceCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(DonwnsampledSampleBufferSize, FMegaLightHairTransmittanceCS::GetGroupSize());
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("HairTransmittanceCS"),
				ComputeShader,
				PassParameters,
				GroupCount);
		}

		FRDGTextureRef ResolvedDiffuseLighting = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.ResolvedDiffuseLighting"));

		FRDGTextureRef ResolvedSpecularLighting = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.ResolvedSpecularLighting"));

		FRDGTextureRef ShadingConfidence = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_R8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.ShadingConfidence"));

		FRDGBufferRef VisibleLightHash = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), VisibleLightHashBufferSize), TEXT("MegaLights.VisibleLightHash"));
		FRDGBufferRef VisibleLightMaskHash = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), VisibleLightHashBufferSize), TEXT("MegaLights.VisibleLightMaskHash"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(VisibleLightHash), 0);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(VisibleLightMaskHash), 0);

		FRDGBufferRef VolumeVisibleLightHash = nullptr;
		if (MegaLights::UseVolume() && bShouldRenderVolumetricFog && bVolumeGuideByHistory)
		{
			VolumeVisibleLightHash = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), VolumeVisibleLightHashBufferSize), TEXT("MegaLights.Volume.VisibleLightHash"));
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(VolumeVisibleLightHash), 0);
		}

		FRDGBufferRef TranslucencyVolumeVisibleLightHash[TVC_MAX] = {};
		if (MegaLights::UseTranslucencyVolume() && bShouldRenderTranslucencyVolume && bTranslucencyVolumeGuideByHistory)
		{
			for (uint32 CascadeIndex = 0; CascadeIndex < TVC_MAX; ++CascadeIndex)
			{
				TranslucencyVolumeVisibleLightHash[CascadeIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TranslucencyVolumeVisibleLightHashBufferSize), TEXT("MegaLights.TranslucencyVolume.VisibleLightHash"));
				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TranslucencyVolumeVisibleLightHash[CascadeIndex]), 0);
			}
		}

		// Shade light samples
		{
			FRDGTextureUAVRef ResolvedDiffuseLightingUAV = GraphBuilder.CreateUAV(ResolvedDiffuseLighting, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef ResolvedSpecularLightingUAV = GraphBuilder.CreateUAV(ResolvedSpecularLighting, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef ShadingConfidenceUAV = GraphBuilder.CreateUAV(ShadingConfidence, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGBufferUAVRef VisibleLightHashUAV = GraphBuilder.CreateUAV(VisibleLightHash, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGBufferUAVRef VisibleLightMaskHashUAV = GraphBuilder.CreateUAV(VisibleLightMaskHash, ERDGUnorderedAccessViewFlags::SkipBarrier);

			// Clear tiles which won't be processed by FShadeLightSamplesCS
			{
				FClearResolvedLightingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearResolvedLightingCS::FParameters>();
				PassParameters->IndirectArgs = TileIndirectArgs;
				PassParameters->RWResolvedDiffuseLighting = ResolvedDiffuseLightingUAV;
				PassParameters->RWResolvedSpecularLighting = ResolvedSpecularLightingUAV;
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
				PassParameters->TileData = GraphBuilder.CreateSRV(TileData);

				auto ComputeShader = View.ShaderMap->GetShader<FClearResolvedLightingCS>();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ClearResolvedLighting"),
					ComputeShader,
					PassParameters,
					TileIndirectArgs,
					(int32)MegaLights::ETileType::Empty * sizeof(FRHIDispatchIndirectParameters));
			}

			for (const int32 TileType : ShadingTileTypes)
			{
				if (!View.bLightGridHasRectLights && IsRectLightTileType((MegaLights::ETileType)TileType))
				{
					continue;
				}

				if (!View.bLightGridHasTexturedLights && IsTexturedLightTileType((MegaLights::ETileType)TileType))
				{
					continue;
				}

				FShadeLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadeLightSamplesCS::FParameters>();
				PassParameters->RWResolvedDiffuseLighting = ResolvedDiffuseLightingUAV;
				PassParameters->RWResolvedSpecularLighting = ResolvedSpecularLightingUAV;
				PassParameters->RWShadingConfidence = ShadingConfidenceUAV;
				PassParameters->IndirectArgs = TileIndirectArgs;
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
				PassParameters->TileData = GraphBuilder.CreateSRV(TileData);
				PassParameters->LightSamples = LightSamples;
				PassParameters->UseShadingConfidence = CVarMegaLightsShadingConfidence.GetValueOnRenderThread();
				PassParameters->HairTransmittanceMaskTexture = HairTransmittanceMaskTexture;

				FShadeLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FShadeLightSamplesCS::FTileType>(TileType);
				PermutationVector.Set<FShadeLightSamplesCS::FDownsampleFactor>(DownsampleFactor);
				PermutationVector.Set<FShadeLightSamplesCS::FNumSamplesPerPixel1d>(NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y);
				PermutationVector.Set<FShadeLightSamplesCS::FInputType>(uint32(InputType));
				PermutationVector.Set<FShadeLightSamplesCS::FDebugMode>(bDebug);
				auto ComputeShader = View.ShaderMap->GetShader<FShadeLightSamplesCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ShadeLightSamples TileType:%s", MegaLights::GetTileTypeString((MegaLights::ETileType)TileType)),
					ComputeShader,
					PassParameters,
					TileIndirectArgs,
					TileType * sizeof(FRHIDispatchIndirectParameters));
			}
		}

		// Prepare visible light list hash for the next frame
		if (bGuideByHistory)
		{
			FVisibleLightHashCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVisibleLightHashCS::FParameters>();
			PassParameters->RWVisibleLightHash = GraphBuilder.CreateUAV(VisibleLightHash);
			PassParameters->RWVisibleLightMaskHash = GraphBuilder.CreateUAV(VisibleLightMaskHash);
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->LightSamples = LightSamples;
			PassParameters->LightSampleRays = LightSampleRays;

			FVisibleLightHashCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FVisibleLightHashCS::FNumSamplesPerPixel1d>(NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y);
			PermutationVector.Set<FVisibleLightHashCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FVisibleLightHashCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FVisibleLightHashCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("VisibleLightHash"),
				ComputeShader,
				PassParameters,
				GroupCount);
		}

		if (MegaLights::UseVolume() && bShouldRenderVolumetricFog && bVolumeGuideByHistory)
		{
			FVolumeVisibleLightHashCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeVisibleLightHashCS::FParameters>();
			PassParameters->RWVisibleLightHash = GraphBuilder.CreateUAV(VolumeVisibleLightHash);
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->LightSamples = VolumeLightSamples;
			PassParameters->VolumeVisibleLightHashTileSize = VolumeVisibleLightHashTileSize;
			PassParameters->VolumeVisibleLightHashViewSizeInTiles = VolumeVisibleLightHashViewSizeInTiles;

			FVolumeVisibleLightHashCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FVolumeVisibleLightHashCS::FNumSamplesPerVoxel1d>(NumSamplesPerVoxel3d.X * NumSamplesPerVoxel3d.Y * NumSamplesPerVoxel3d.Z);
			PermutationVector.Set<FVolumeVisibleLightHashCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FVolumeVisibleLightHashCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(VolumeVisibleLightHashViewSizeInTiles, FVolumeVisibleLightHashCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("VolumeVisibleLightHash"),
				ComputeShader,
				PassParameters,
				GroupCount);
		}

		if (MegaLights::UseTranslucencyVolume() && bShouldRenderTranslucencyVolume && bTranslucencyVolumeGuideByHistory)
		{
			for (uint32 CascadeIndex = 0; CascadeIndex < TVC_MAX; ++CascadeIndex)
			{
				FVolumeVisibleLightHashCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeVisibleLightHashCS::FParameters>();
				PassParameters->RWVisibleLightHash = GraphBuilder.CreateUAV(TranslucencyVolumeVisibleLightHash[CascadeIndex]);
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->LightSamples = TranslucencyVolumeLightSamples[CascadeIndex];
				PassParameters->VolumeVisibleLightHashTileSize = TranslucencyVolumeVisibleLightHashTileSize;
				PassParameters->VolumeVisibleLightHashViewSizeInTiles = TranslucencyVolumeVisibleLightHashSizeInTiles;

				FVolumeVisibleLightHashCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FVolumeVisibleLightHashCS::FNumSamplesPerVoxel1d>(NumSamplesPerTranslucencyVoxel3d.X * NumSamplesPerTranslucencyVoxel3d.Y * NumSamplesPerTranslucencyVoxel3d.Z);
				PermutationVector.Set<FVolumeVisibleLightHashCS::FDebugMode>(bDebug);
				auto ComputeShader = View.ShaderMap->GetShader<FVolumeVisibleLightHashCS>(PermutationVector);

				const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(TranslucencyVolumeVisibleLightHashSizeInTiles, FVolumeVisibleLightHashCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("TranslucencyVolumeVisibleLightHash"),
					ComputeShader,
					PassParameters,
					GroupCount);
			}
		}

		if (MegaLights::UseVolume() && bShouldRenderVolumetricFog)
		{
			FRDGTextureRef VolumeResolvedLighting = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create3D(VolumeBufferSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
				TEXT("MegaLights.Volume.ResolvedLighting"));

			FVolumeShadeLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeShadeLightSamplesCS::FParameters>();
			PassParameters->RWVolumeResolvedLighting = GraphBuilder.CreateUAV(VolumeResolvedLighting);
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->MegaLightsVolumeParameters = MegaLightsVolumeParameters;
			PassParameters->VolumeLightSamples = VolumeLightSamples;

			FVolumeShadeLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FVolumeShadeLightSamplesCS::FTranslucencyLightingVolume>(false);
			PermutationVector.Set<FVolumeShadeLightSamplesCS::FDownsampleFactor>(VolumeDownsampleFactor);
			PermutationVector.Set<FVolumeShadeLightSamplesCS::FNumSamplesPerVoxel1d>(NumSamplesPerVoxel3d.X * NumSamplesPerVoxel3d.Y * NumSamplesPerVoxel3d.Z);
			PermutationVector.Set<FVolumeShadeLightSamplesCS::FLightSoftFading>(GetVolumetricFogLightSoftFading() > 0.0f);
			PermutationVector.Set<FVolumeShadeLightSamplesCS::FUseLightFunctionAtlas >(bUseLightFunctionAtlas && MegaLightsVolume::UsesLightFunction());
			PermutationVector.Set<FVolumeShadeLightSamplesCS::FDebugMode>(bVolumeDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FVolumeShadeLightSamplesCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(VolumeViewSize, FVolumeShadeLightSamplesCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("VolumeShadeLightSamples"),
				ComputeShader,
				PassParameters,
				GroupCount);

			if (MegaLightsVolume)
			{
				MegaLightsVolume->Texture = VolumeResolvedLighting;
			}
		}

		if (MegaLights::UseTranslucencyVolume() && bShouldRenderTranslucencyVolume)
		{
			for (uint32 CascadeIndex = 0; CascadeIndex < TVC_MAX; ++CascadeIndex)
			{
				FRDGTextureRef TranslucencyVolumeResolvedLightingAmbient = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create3D(TranslucencyVolumeBufferSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
					TEXT("MegaLights.TranslucencyVolume.ResolvedLightingAmbient"));

				FRDGTextureRef TranslucencyVolumeResolvedLightingDirectional = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create3D(TranslucencyVolumeBufferSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
					TEXT("MegaLights.TranslucencyVolume.ResolvedLightingDirectional"));

				FVolumeShadeLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeShadeLightSamplesCS::FParameters>();
				PassParameters->RWTranslucencyVolumeResolvedLightingAmbient = GraphBuilder.CreateUAV(TranslucencyVolumeResolvedLightingAmbient);
				PassParameters->RWTranslucencyVolumeResolvedLightingDirectional = GraphBuilder.CreateUAV(TranslucencyVolumeResolvedLightingDirectional);
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->MegaLightsVolumeParameters = MegaLightsTranslucencyVolumeParameters;
				PassParameters->MegaLightsVolumeParameters.TranslucencyVolumeCascadeIndex = CascadeIndex;
				PassParameters->VolumeLightSamples = TranslucencyVolumeLightSamples[CascadeIndex];

				FVolumeShadeLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FVolumeShadeLightSamplesCS::FTranslucencyLightingVolume>(true);
				PermutationVector.Set<FVolumeShadeLightSamplesCS::FDownsampleFactor>(TranslucencyVolumeDownsampleFactor);
				PermutationVector.Set<FVolumeShadeLightSamplesCS::FNumSamplesPerVoxel1d>(NumSamplesPerTranslucencyVoxel3d.X * NumSamplesPerTranslucencyVoxel3d.Y * NumSamplesPerTranslucencyVoxel3d.Z);
				PermutationVector.Set<FVolumeShadeLightSamplesCS::FLightSoftFading>(false);
				PermutationVector.Set<FVolumeShadeLightSamplesCS::FUseLightFunctionAtlas >(bUseLightFunctionAtlas && MegaLightsTranslucencyVolume::UsesLightFunction());
				PermutationVector.Set<FVolumeShadeLightSamplesCS::FDebugMode>(bTranslucencyVolumeDebug);
				auto ComputeShader = View.ShaderMap->GetShader<FVolumeShadeLightSamplesCS>(PermutationVector);

				const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(TranslucencyVolumeBufferSize, FVolumeShadeLightSamplesCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("TranslucencyVolumeShadeLightSamples"),
					ComputeShader,
					PassParameters,
					GroupCount);

				if (MegaLightsVolume)
				{
					MegaLightsVolume->TranslucencyAmbient[CascadeIndex] = TranslucencyVolumeResolvedLightingAmbient;
					MegaLightsVolume->TranslucencyDirectional[CascadeIndex] = TranslucencyVolumeResolvedLightingDirectional;
				}
			}
		}

		if (bGuideByHistory && CVarMegaLightsGuideByHistoryFilter.GetValueOnRenderThread())
		{
			FRDGBufferRef FilteredVisibleLightHash = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), VisibleLightHashBufferSize), TEXT("MegaLights.FilteredVisibleLightHash"));
			FRDGBufferRef FilteredVisibleLightMaskHash = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), VisibleLightHashBufferSize), TEXT("MegaLights.FilteredVisibleLightMaskHash"));

			FFilterVisibleLightHashCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilterVisibleLightHashCS::FParameters>();
			PassParameters->RWVisibleLightHash = GraphBuilder.CreateUAV(FilteredVisibleLightHash);
			PassParameters->RWVisibleLightMaskHash = GraphBuilder.CreateUAV(FilteredVisibleLightMaskHash);
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->VisibleLightHashBuffer = GraphBuilder.CreateSRV(VisibleLightHash);
			PassParameters->VisibleLightMaskHashBuffer = GraphBuilder.CreateSRV(VisibleLightMaskHash);

			FFilterVisibleLightHashCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FFilterVisibleLightHashCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FFilterVisibleLightHashCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(VisibleLightHashViewSizeInTiles, FFilterVisibleLightHashCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("FilterVisibleLightHash"),
				ComputeShader,
				PassParameters,
				GroupCount);

			VisibleLightHash = FilteredVisibleLightHash;
			VisibleLightMaskHash = FilteredVisibleLightMaskHash;
		}

		if (MegaLights::UseVolume() && bShouldRenderVolumetricFog && bVolumeGuideByHistory && CVarMegaLightsVolumeGuideByHistoryFilter.GetValueOnRenderThread())
		{
			FRDGBufferRef VolumeFilteredVisibleLightHash = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), VolumeVisibleLightHashBufferSize), TEXT("MegaLights.Volume.FilteredVisibleLightHash"));

			FVolumeFilterVisibleLightHashCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeFilterVisibleLightHashCS::FParameters>();
			PassParameters->RWVisibleLightHash = GraphBuilder.CreateUAV(VolumeFilteredVisibleLightHash);
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->VolumeVisibleLightHashViewSizeInTiles = VolumeVisibleLightHashViewSizeInTiles;
			PassParameters->VisibleLightHashBuffer = GraphBuilder.CreateSRV(VolumeVisibleLightHash);

			FVolumeFilterVisibleLightHashCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FVolumeFilterVisibleLightHashCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FVolumeFilterVisibleLightHashCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(VolumeVisibleLightHashViewSizeInTiles, FVolumeFilterVisibleLightHashCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("VolumeFilterVisibleLightHash"),
				ComputeShader,
				PassParameters,
				GroupCount);

			VolumeVisibleLightHash = VolumeFilteredVisibleLightHash;
		}

		if (MegaLights::UseTranslucencyVolume() && bShouldRenderTranslucencyVolume && bTranslucencyVolumeGuideByHistory && CVarMegaLightsTranslucencyVolumeGuideByHistoryFilter.GetValueOnRenderThread())
		{
			for (uint32 CascadeIndex = 0; CascadeIndex < TVC_MAX; ++CascadeIndex)
			{
				FRDGBufferRef TranslucencyVolumeFilteredVisibleLightHash = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TranslucencyVolumeVisibleLightHashBufferSize), TEXT("MegaLights.TranslucencyVolume.FilteredVisibleLightHash"));

				FVolumeFilterVisibleLightHashCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeFilterVisibleLightHashCS::FParameters>();
				PassParameters->RWVisibleLightHash = GraphBuilder.CreateUAV(TranslucencyVolumeFilteredVisibleLightHash);
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->VolumeVisibleLightHashViewSizeInTiles = TranslucencyVolumeVisibleLightHashSizeInTiles;
				PassParameters->VisibleLightHashBuffer = GraphBuilder.CreateSRV(TranslucencyVolumeVisibleLightHash[CascadeIndex]);

				FVolumeFilterVisibleLightHashCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FVolumeFilterVisibleLightHashCS::FDebugMode>(bDebug);
				auto ComputeShader = View.ShaderMap->GetShader<FVolumeFilterVisibleLightHashCS>(PermutationVector);

				const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(TranslucencyVolumeVisibleLightHashSizeInTiles, FVolumeFilterVisibleLightHashCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("TranslucencyVolumeFilterVisibleLightHash"),
					ComputeShader,
					PassParameters,
					GroupCount);

				TranslucencyVolumeVisibleLightHash[CascadeIndex] = TranslucencyVolumeFilteredVisibleLightHash;
			}
		}

		// Demodulated lighting components with second luminance moments stored in alpha channel for temporal variance tracking
		// This will be passed to the next frame
		FRDGTextureRef DiffuseLightingAndSecondMoment = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.DiffuseLightingAndSecondMoment"));

		FRDGTextureRef SpecularLightingAndSecondMoment = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.SpecularLightingAndSecondMoment"));

		FRDGTextureRef NumFramesAccumulated = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_G8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.NumFramesAccumulated"));

		// Temporal accumulation
		{
			FDenoiserTemporalCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDenoiserTemporalCS::FParameters>();
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->ResolvedDiffuseLighting = ResolvedDiffuseLighting;
			PassParameters->ResolvedSpecularLighting = ResolvedSpecularLighting;
			PassParameters->ShadingConfidenceTexture = ShadingConfidence;
			PassParameters->DiffuseLightingAndSecondMomentHistoryTexture = DiffuseLightingAndSecondMomentHistory;
			PassParameters->SpecularLightingAndSecondMomentHistoryTexture = SpecularLightingAndSecondMomentHistory;
			PassParameters->NumFramesAccumulatedHistoryTexture = NumFramesAccumulatedHistory;
			PassParameters->MegaLightsDepthHistory = SceneDepthHistory;
			PassParameters->MegaLightsNormalAndShading = SceneNormalAndShadingHistory;
			PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
			PassParameters->MinFramesAccumulatedForHistoryMiss = FMath::Clamp(CVarMegaLightsTemporalMinFramesAccumulatedForHistoryMiss.GetValueOnRenderThread(), 1.0f, MegaLights::GetTemporalMaxFramesAccumulated());
			PassParameters->MinFramesAccumulatedForHighConfidence = FMath::Clamp(CVarMegaLightsTemporalMinFramesAccumulatedForHighConfidence.GetValueOnRenderThread(), 1.0f, MegaLights::GetTemporalMaxFramesAccumulated());
			PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
			PassParameters->HistoryUVMinMax = HistoryUVMinMax;
			PassParameters->HistoryGatherUVMinMax = HistoryGatherUVMinMax;
			PassParameters->HistoryBufferSizeAndInvSize = HistoryBufferSizeAndInvSize;
			PassParameters->RWDiffuseLightingAndSecondMoment = GraphBuilder.CreateUAV(DiffuseLightingAndSecondMoment);
			PassParameters->RWSpecularLightingAndSecondMoment = GraphBuilder.CreateUAV(SpecularLightingAndSecondMoment);
			PassParameters->RWNumFramesAccumulated = GraphBuilder.CreateUAV(NumFramesAccumulated);

			FDenoiserTemporalCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FDenoiserTemporalCS::FValidHistory>(DiffuseLightingAndSecondMomentHistory && SceneDepthHistory && SceneNormalAndShadingHistory && bTemporal);
			PermutationVector.Set<FDenoiserTemporalCS::FDebugMode>(bDebug);
			PermutationVector.Set<FDenoiserTemporalCS::FInputType>(uint32(InputType));
			auto ComputeShader = View.ShaderMap->GetShader<FDenoiserTemporalCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FDenoiserTemporalCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TemporalAccumulation"),
				ComputeShader,
				PassParameters,
				GroupCount);
		}

		// Spatial filter
		{
			FDenoiserSpatialCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDenoiserSpatialCS::FParameters>();
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->RWSceneColor = GraphBuilder.CreateUAV(OutputColorTarget);
			PassParameters->DiffuseLightingAndSecondMomentTexture = DiffuseLightingAndSecondMoment;
			PassParameters->SpecularLightingAndSecondMomentTexture = SpecularLightingAndSecondMoment;
			PassParameters->ShadingConfidenceTexture = ShadingConfidence;
			PassParameters->NumFramesAccumulatedTexture = NumFramesAccumulated;
			PassParameters->SpatialFilterDepthWeightScale = CVarMegaLightsSpatialDepthWeightScale.GetValueOnRenderThread();
			PassParameters->SpatialFilterKernelRadius = CVarMegaLightsSpatialKernelRadius.GetValueOnRenderThread();
			PassParameters->SpatialFilterNumSamples = FMath::Clamp(CVarMegaLightsSpatialNumSamples.GetValueOnRenderThread(), 0, 1024);
			PassParameters->SpatialFilterMaxDisocclusionFrames = MegaLights::GetSpatialFilterMaxDisocclusionFrames();

			FDenoiserSpatialCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FDenoiserSpatialCS::FSpatialFilter>(bSpatial);
			PermutationVector.Set<FDenoiserSpatialCS::FDebugMode>(bDebug);
			PermutationVector.Set<FDenoiserSpatialCS::FInputType>(uint32(InputType));
			auto ComputeShader = View.ShaderMap->GetShader<FDenoiserSpatialCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FDenoiserSpatialCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Spatial"),
				ComputeShader,
				PassParameters,
				GroupCount);
		}

		if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
		{
			FMegaLightsViewState& MegaLightsViewState = View.ViewState->MegaLights;

			MegaLightsViewState.HistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(View.GetSceneTexturesConfig().Extent, View.ViewRect);

			const FVector2f InvBufferSize(1.0f / SceneTextures.Config.Extent.X, 1.0f / SceneTextures.Config.Extent.Y);

			MegaLightsViewState.HistoryUVMinMax = FVector4f(
				View.ViewRect.Min.X * InvBufferSize.X,
				View.ViewRect.Min.Y * InvBufferSize.Y,
				View.ViewRect.Max.X * InvBufferSize.X,
				View.ViewRect.Max.Y * InvBufferSize.Y);

			// Clamp gather4 to a valid bilinear footprint in order to avoid sampling outside of valid bounds
			MegaLightsViewState.HistoryGatherUVMinMax = FVector4f(
				(View.ViewRect.Min.X + 0.51f) * InvBufferSize.X,
				(View.ViewRect.Min.Y + 0.51f) * InvBufferSize.Y,
				(View.ViewRect.Max.X - 0.51f) * InvBufferSize.X,
				(View.ViewRect.Max.Y - 0.51f) * InvBufferSize.Y);

			MegaLightsViewState.HistoryBufferSizeAndInvSize = FVector4f(
				SceneTextures.Config.Extent.X,
				SceneTextures.Config.Extent.Y,
				1.0f / SceneTextures.Config.Extent.X,
				1.0f / SceneTextures.Config.Extent.Y);

			MegaLightsViewState.HistoryVisibleLightHashViewMinInTiles = VisibleLightHashViewMinInTiles;
			MegaLightsViewState.HistoryVisibleLightHashViewSizeInTiles = VisibleLightHashViewSizeInTiles;

			MegaLightsViewState.HistoryVolumeVisibleLightHashViewSizeInTiles = VolumeVisibleLightHashViewSizeInTiles;
			MegaLightsViewState.HistoryTranslucencyVolumeVisibleLightHashSizeInTiles = TranslucencyVolumeVisibleLightHashSizeInTiles;

			if (DiffuseLightingAndSecondMoment && SpecularLightingAndSecondMoment && NumFramesAccumulated && bTemporal)
			{
				GraphBuilder.QueueTextureExtraction(DiffuseLightingAndSecondMoment, &MegaLightsViewState.DiffuseLightingAndSecondMomentHistory);
				GraphBuilder.QueueTextureExtraction(SpecularLightingAndSecondMoment, &MegaLightsViewState.SpecularLightingAndSecondMomentHistory);
				GraphBuilder.QueueTextureExtraction(NumFramesAccumulated, &MegaLightsViewState.NumFramesAccumulatedHistory);
			}
			else
			{
				MegaLightsViewState.DiffuseLightingAndSecondMomentHistory = nullptr;
				MegaLightsViewState.SpecularLightingAndSecondMomentHistory = nullptr;
				MegaLightsViewState.NumFramesAccumulatedHistory = nullptr;
			}

			if (bGuideByHistory)
			{
				GraphBuilder.QueueBufferExtraction(VisibleLightHash, &MegaLightsViewState.VisibleLightHashHistory);
				GraphBuilder.QueueBufferExtraction(VisibleLightMaskHash, &MegaLightsViewState.VisibleLightMaskHashHistory);
			}
			else
			{
				MegaLightsViewState.VisibleLightHashHistory = nullptr;
				MegaLightsViewState.VisibleLightMaskHashHistory = nullptr;
			}

			if (bVolumeGuideByHistory && VolumeVisibleLightHash != nullptr)
			{
				GraphBuilder.QueueBufferExtraction(VolumeVisibleLightHash, &MegaLightsViewState.VolumeVisibleLightHashHistory);
			}
			else
			{
				MegaLightsViewState.VolumeVisibleLightHashHistory = nullptr;
			}

			if (bTranslucencyVolumeGuideByHistory && TranslucencyVolumeVisibleLightHash[0] != nullptr && TranslucencyVolumeVisibleLightHash[1] != nullptr)
			{
				GraphBuilder.QueueBufferExtraction(TranslucencyVolumeVisibleLightHash[0], &MegaLightsViewState.TranslucencyVolume0VisibleLightHashHistory);
				GraphBuilder.QueueBufferExtraction(TranslucencyVolumeVisibleLightHash[1], &MegaLightsViewState.TranslucencyVolume1VisibleLightHashHistory);
			}
			else
			{
				MegaLightsViewState.TranslucencyVolume0VisibleLightHashHistory = nullptr;
				MegaLightsViewState.TranslucencyVolume1VisibleLightHashHistory = nullptr;
			}
		}
	}
}

void FDeferredShadingSceneRenderer::RenderMegaLights(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures, FRDGTextureRef LightingChannelsTexture, const FSortedLightSetSceneInfo& SortedLightSet)
{
	if (!MegaLights::IsEnabled(ViewFamily) || !ViewFamily.EngineShowFlags.DirectLighting)
	{
		return;
	}

	check(AreLightsInLightGrid());
	RDG_EVENT_SCOPE_STAT(GraphBuilder, MegaLights, "MegaLights");
	RDG_GPU_STAT_SCOPE(GraphBuilder, MegaLights);

	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	TUniformBufferRef<FBlueNoise> BlueNoiseUniformBuffer = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];

		// Enable support for VSMs if there are any relevant local lights and the feature is enabled
		FShadowSceneRenderer& ShadowSceneRenderer = GetSceneExtensionsRenderers().GetRenderer<FShadowSceneRenderer>();
		const bool bUseVSM = VirtualShadowMapArray.IsAllocated() && ShadowSceneRenderer.AreAnyLightsUsingMegaLightsVSM();
		const bool bHairStrands = HairStrands::HasViewHairStrandsData(View) && CVarMegaLightsEnableHairStrands.GetValueOnRenderThread() > 0;

		{
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, bHairStrands, "GBuffer");

			InternalRenderMegaLights(
				GraphBuilder, 
				ViewIndex, 
				View, 
				ViewFamily, 
				Scene, 
				SceneTextures, 
				LightingChannelsTexture,  
				SortedLightSet, 
				bUseVSM ? &VirtualShadowMapArray : nullptr, 
				&View.GetOwnMegaLightsVolume(),
				ShouldRenderVolumetricFog(),
				MegaLights::UseTranslucencyVolume(),
				BlueNoiseUniformBuffer, 
				EMegaLightsInput::GBuffer,
				SceneTextures.Color.Target);
		}

		if (bHairStrands)
		{
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, bHairStrands, "HairStrands");

			InternalRenderMegaLights(
				GraphBuilder, 
				ViewIndex, 
				View, 
				ViewFamily, 
				Scene, 
				SceneTextures, 
				LightingChannelsTexture, 
				SortedLightSet, 
				bUseVSM ? &VirtualShadowMapArray : nullptr, 
				nullptr /*MegaLightsVolume*/, 
				false /*bShouldRenderVolumetricFog*/,
				false /*bShouldRenderTranslucencyVolume*/,
				BlueNoiseUniformBuffer, 
				EMegaLightsInput::HairStrands,
				View.HairStrandsViewData.VisibilityData.SampleLightingTexture);
		}
	}
}

namespace MegaLights
{
	bool IsMissingDirectionalLightData(const FSceneViewFamily& ViewFamily)
	{
		static auto LightBufferModeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Forward.LightBuffer.Mode"));
		
		return CVarMegaLightsDirectionalLights.GetValueOnRenderThread() && LightBufferModeCVar->GetInt() == 0;
	}

	bool HasWarning(const FSceneViewFamily& ViewFamily)
	{
		return IsRequested(ViewFamily) && (!HasRequiredTracingData(ViewFamily) || IsMissingDirectionalLightData(ViewFamily));
	}

	void WriteWarnings(const FSceneViewFamily& ViewFamily, FScreenMessageWriter& Writer)
	{
		if (!HasWarning(ViewFamily))
		{
			return;
		}

		if (!HasRequiredTracingData(ViewFamily))
		{
			static const FText MainMessage = NSLOCTEXT("Renderer", "MegaLightsCantDisplay", "MegaLights is enabled, but has no ray tracing data and won't operate correctly.");
			Writer.DrawLine(MainMessage);

#if RHI_RAYTRACING
			if (!IsRayTracingAllowed())
			{
				static const FText Message = NSLOCTEXT("Renderer", "MegaLightsCantDisplayDueToHWRTNotAllowed", "- Hardware Ray Tracing is not allowed. Check log for more info.");
				Writer.DrawLine(Message);
			}
			else if (!IsRayTracingEnabled())
			{
				static const FText Message = NSLOCTEXT("Renderer", "MegaLightsCantDisplayDueToHWRTDisabled", "- Enable 'r.RayTracing.Enable'.");
				Writer.DrawLine(Message);
			}

			static auto CVarMegaLightsHardwareRayTracing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.HardwareRayTracing"));
			if (CVarMegaLightsHardwareRayTracing->GetInt() == 0)
			{
				static const FText Message = NSLOCTEXT("Renderer", "MegaLightsCantDisplayDueToCvar", "- Enable 'r.MegaLights.HardwareRayTracing'.");
				Writer.DrawLine(Message);
			}

			static auto CVarMegaLightsHardwareRayTracingInline = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.HardwareRayTracing.Inline"));
			if (!(GRHISupportsRayTracingShaders || (GRHISupportsInlineRayTracing && CVarMegaLightsHardwareRayTracingInline->GetInt() != 0)))
			{
				static const FText Message = NSLOCTEXT("Renderer", "MegaLightsCantDisplayDueToPlatformSettings", "- Enable Full Ray Tracing in platform platform settings or r.MegaLights.HardwareRayTracing.Inline.");
				Writer.DrawLine(Message);
			}

			if (!(ViewFamily.Views.Num() == 1 || (ViewFamily.Views.Num() == 2 && IStereoRendering::IsStereoEyeView(*ViewFamily.Views[0]))))
			{
				static const FText Message = NSLOCTEXT("Renderer", "MegaLightsCantDisplayDueToMultipleViews", "- Multiple views are not supported.");
				Writer.DrawLine(Message);
			}

			if (!ViewFamily.Views[0]->IsRayTracingAllowedForView())
			{
				static const FText Message = NSLOCTEXT("Renderer", "MegaLightsCantDisplayDueToView", "- Ray Tracing not allowed on the View.");
				Writer.DrawLine(Message);
			}
#else
			static const FText Message = NSLOCTEXT("Renderer", "MegaLightsCantDisplayDueToBuild", "- Unreal Engine was built without Hardware Ray Tracing support.");
			Writer.DrawLine(Message);
#endif
		}

		if (IsMissingDirectionalLightData(ViewFamily))
		{
			static const FText MainMessage = NSLOCTEXT("Renderer", "MegaLightsCantDisplayDirectionalLights", "MegaLights requires r.Forward.LightBuffer.Mode > 0 when using r.MegaLights.DirectionalLights=1.");
			Writer.DrawLine(MainMessage);
		}
	}
}