// Copyright Epic Games, Inc. All Rights Reserved.

#include "MegaLights.h"
#include "MegaLightsInternal.h"
#include "Lumen/LumenTracingUtils.h"
#include "Lumen/LumenHardwareRayTracingCommon.h"
#include "VirtualShadowMaps/VirtualShadowMapArray.h"
#include "Nanite/NaniteRayTracing.h"
#include "BasePassRendering.h"

static TAutoConsoleVariable<int32> CVarMegaLightsScreenTraces(
	TEXT("r.MegaLights.ScreenTraces"),
	1,
	TEXT("Whether to use screen space tracing for shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsScreenTracesMaxIterations(
	TEXT("r.MegaLights.ScreenTraces.MaxIterations"),
	50,
	TEXT("Max iterations for HZB tracing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsScreenTracesMaxDistance(
	TEXT("r.MegaLights.ScreenTraces.MaxDistance"),
	100,
	TEXT("Max distance in world space for screen space tracing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsScreenTracesMinimumOccupancy(
	TEXT("r.MegaLights.ScreenTraces.MinimumOccupancy"),
	0,
	TEXT("Minimum number of threads still tracing before aborting the trace. Can be used for scalability to abandon traces that have a disproportionate cost."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsScreenTraceRelativeDepthThreshold(
	TEXT("r.MegaLights.ScreenTraces.RelativeDepthThickness"),
	0.005f,
	TEXT("Determines depth thickness of objects hit by HZB tracing, as a relative depth threshold."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsWorldSpaceTraces(
	TEXT("r.MegaLights.WorldSpaceTraces"),
	1,
	TEXT("Whether to trace world space shadow rays for samples. Useful for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsSoftwareRayTracingAllow(
	TEXT("r.MegaLights.SoftwareRayTracing.Allow"),
	0,
	TEXT("Whether to allow using software ray tracing when hardware ray tracing is not supported."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsHardwareRayTracing(
	TEXT("r.MegaLights.HardwareRayTracing"),
	1,
	TEXT("Whether to use hardware ray tracing for shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsHardwareRayTracingInline(
	TEXT("r.MegaLights.HardwareRayTracing.Inline"),
	1,
	TEXT("Uses hardware inline ray tracing for ray traced lighting, when available."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarMegaLightsHardwareRayTracingEvaluateMaterialMode(
	TEXT("r.MegaLights.HardwareRayTracing.EvaluateMaterialMode"),
	0,
	TEXT("Which mode to use for material evaluation to support alpha masked materials.\n")
	TEXT("0 - Don't evaluate materials (default)\n")
	TEXT("1 - Evaluate materials\n")
	TEXT("2 - Evaluate materials in a separate pass (may be faster on certain platforms without dedicated ray tracing hardware)"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<float> CVarMegaLightsHardwareRayTracingBias(
	TEXT("r.MegaLights.HardwareRayTracing.Bias"),
	1.0f,
	TEXT("Constant bias for hardware ray traced shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsHardwareRayTracingEndBias(
	TEXT("r.MegaLights.HardwareRayTracing.EndBias"),
	1.0f,
	TEXT("Constant bias for hardware ray traced shadow rays to prevent proxy geo self-occlusion near the lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsHardwareRayTracingNormalBias(
	TEXT("r.MegaLights.HardwareRayTracing.NormalBias"),
	0.1f,
	TEXT("Normal bias for hardware ray traced shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsHardwareRayTracingPullbackBias(
	TEXT("r.MegaLights.HardwareRayTracing.PullbackBias"),
	1.0f,
	TEXT("Determines the pull-back bias when resuming a screen-trace ray."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsHardwareRayTracingMaxIterations(
	TEXT("r.MegaLights.HardwareRayTracing.MaxIterations"),
	8192,
	TEXT("Limit number of ray tracing traversal iterations on supported platfoms. Improves performance, but may add over-occlusion."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsHardwareRayTracingMeshSectionVisibilityTest(
	TEXT("r.MegaLights.HardwareRayTracing.MeshSectionVisibilityTest"),
	0,
	TEXT("Whether to test mesh section visibility at runtime.\n")
	TEXT("When enabled translucent mesh sections are automatically hidden based on the material, but it slows down performance due to extra visibility tests per intersection.\n")
	TEXT("When disabled translucent meshes can be hidden only if they are fully translucent. Individual mesh sections need to be hidden upfront inside the static mesh editor."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

// #ml_todo: Separate config cvars from Lumen once we support multiple SBT with same RayTracingPipeline or Global Uniform Buffers in Ray Tracing
static TAutoConsoleVariable<int32> CVarMegaLightsHardwareRayTracingAvoidSelfIntersections(
	TEXT("r.MegaLights.HardwareRayTracing.AvoidSelfIntersections"),
	1,
	TEXT("Whether to skip back face hits for a small distance in order to avoid self-intersections when BLAS mismatches rasterized geometry.\n")
	TEXT("Currently shares config with Lumen:\n")
	TEXT("0 - Disabled. May have extra leaking, but it's the fastest mode.\n")
	TEXT("1 - Enabled. This mode retraces to skip first backface hit up to r.Lumen.HardwareRayTracing.SkipBackFaceHitDistance. Good default on most platforms.\n")
	TEXT("2 - Enabled. This mode uses AHS to skip any backface hits up to r.Lumen.HardwareRayTracing.SkipBackFaceHitDistance. Faster on platforms with inline AHS support."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMegaLightsHardwareRayTracingFarField(
	TEXT("r.MegaLights.HardwareRayTracing.FarField"),
	0,
	TEXT("Determines whether a second trace will be fired for far-field shadowing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsHardwareRayTracingFarFieldMaxDistance(
	TEXT("r.MegaLights.HardwareRayTracing.FarField.MaxDistance"),
	1.0e8f,
	TEXT("Maximum distance in world space for far-field ray tracing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsHardwareRayTracingFarFieldBias(
	TEXT("r.MegaLights.HardwareRayTracing.FarField.Bias"),
	200.0f,
	TEXT("Determines bias for the far field traces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsHairVoxelTraces(
	TEXT("r.MegaLights.HairVoxelTraces"),
	1,
	TEXT("Whether to trace hair voxels."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMegaLightsVolumeWorldSpaceTraces(
	TEXT("r.MegaLights.Volume.WorldSpaceTraces"),
	1,
	TEXT("Whether to trace world space shadow rays for volume samples. Useful for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMegaLightsScreenTracesHairStrands(
	TEXT("r.MegaLights.HairStrands.ScreenTraces"),
	0,
	TEXT("Whether to use screen space tracing for shadow rays with hair strands."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarMegaLightsDebugTraceStats(
	TEXT("r.MegaLights.Debug.TraceStats"),
	0,
	TEXT("Whether to print ray tracing stats on screen."),
	ECVF_RenderThreadSafe);

namespace MegaLights
{
	bool IsSoftwareRayTracingSupported(const FSceneViewFamily& ViewFamily)
	{
		return DoesProjectSupportDistanceFields() && CVarMegaLightsSoftwareRayTracingAllow.GetValueOnRenderThread() != 0;
	}

	bool IsHardwareRayTracingSupported(const FSceneViewFamily& ViewFamily)
	{
#if RHI_RAYTRACING
		{
			// Update MegaLights::WriteWarnings(...) when conditions below are changed
			if (IsRayTracingEnabled()
				&& CVarMegaLightsHardwareRayTracing.GetValueOnRenderThread() != 0
				&& (GRHISupportsRayTracingShaders || (GRHISupportsInlineRayTracing && CVarMegaLightsHardwareRayTracingInline.GetValueOnRenderThread() != 0))
				// HWRT does not support multiple views yet due to TLAS, but stereo views can be allowed as they reuse TLAS for View[0]
				&& (ViewFamily.Views.Num() == 1 || (ViewFamily.Views.Num() == 2 && IStereoRendering::IsStereoEyeView(*ViewFamily.Views[0])))
				&& ViewFamily.Views[0]->IsRayTracingAllowedForView())
			{
				return true;
			}
		}
#endif

		return false;
	}

	bool UseHardwareRayTracing(const FSceneViewFamily& ViewFamily)
	{
		return MegaLights::IsEnabled(ViewFamily) && IsHardwareRayTracingSupported(ViewFamily);
	}

	bool UseInlineHardwareRayTracing(const FSceneViewFamily& ViewFamily)
	{
		#if RHI_RAYTRACING
		{
			if (UseHardwareRayTracing(ViewFamily)
				&& GRHISupportsInlineRayTracing
				&& CVarMegaLightsHardwareRayTracingInline.GetValueOnRenderThread() != 0)
			{
				return true;
			}
		}
		#endif

		return false;
	}

	bool UseFarField(const FSceneViewFamily& ViewFamily)
	{
		return UseHardwareRayTracing(ViewFamily) && CVarMegaLightsHardwareRayTracingFarField.GetValueOnRenderThread() != 0;
	}

	bool UseScreenTraces(EMegaLightsInput InputType)
	{
		const bool bValidMaxDistance = CVarMegaLightsScreenTracesMaxDistance.GetValueOnRenderThread() > 0.0f;
		switch (InputType)
		{
			case EMegaLightsInput::GBuffer: return bValidMaxDistance && CVarMegaLightsScreenTraces.GetValueOnRenderThread() != 0;
			case EMegaLightsInput::HairStrands: return bValidMaxDistance && CVarMegaLightsScreenTracesHairStrands.GetValueOnRenderThread() != 0;
			default: checkf(false, TEXT("MegaLight::UseScreenTraces not implemented")); return false;
		}
	}

	bool IsUsingClosestHZB(const FSceneViewFamily& ViewFamily)
	{
		return IsEnabled(ViewFamily)
			&& (UseScreenTraces(EMegaLightsInput::GBuffer) || UseScreenTraces(EMegaLightsInput::HairStrands));
	}

	bool IsUsingGlobalSDF(const FSceneViewFamily& ViewFamily)
	{
		return IsEnabled(ViewFamily)
			&& CVarMegaLightsWorldSpaceTraces.GetValueOnRenderThread() != 0
			&& IsSoftwareRayTracingSupported(ViewFamily)
			&& !UseHardwareRayTracing(ViewFamily);
	}

#if RHI_RAYTRACING
	bool IsUsingLightingChannels(const FRayTracingScene& RayTracingScene)
	{
		return IsUsingLightingChannels() && RayTracingScene.bUsesLightingChannels;
	}
#endif

	LumenHardwareRayTracing::EAvoidSelfIntersectionsMode GetAvoidSelfIntersectionsMode()
	{
		return (LumenHardwareRayTracing::EAvoidSelfIntersectionsMode)
			FMath::Clamp(CVarMegaLightsHardwareRayTracingAvoidSelfIntersections.GetValueOnRenderThread(), 0, (uint32)LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::MAX - 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FHairVoxelTraceParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, VirtualVoxel)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FCompactedTraceParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs | ERHIAccess::SRVCompute)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CompactedTraceTexelData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CompactedTraceTexelAllocator)
	END_SHADER_PARAMETER_STRUCT()

	enum class ECompactedTraceIndirectArgs
	{
		NumTracesDiv64 = 0 * sizeof(FRHIDispatchIndirectParameters),
		NumTracesDiv32 = 1 * sizeof(FRHIDispatchIndirectParameters),
		NumTraces = 2 * sizeof(FRHIDispatchIndirectParameters),
		MAX = 3
	};

	FCompactedTraceParameters CompactMegaLightsTraces(
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		const FIntPoint SampleBufferSize,
		FRDGTextureRef LightSamples,
		const FMegaLightsParameters& MegaLightsParameters,
		bool bCompactForScreenSpaceTraces);

	FCompactedTraceParameters CompactMegaLightsVolumeTraces(
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		const FIntVector VolumeSampleBufferSize,
		FRDGTextureRef VolumeLightSamples,
		const FMegaLightsParameters& MegaLightsParameters,
		const FMegaLightsVolumeParameters& MegaLightsVolumeParameters);

	enum class EMaterialMode : uint8
	{
		Disabled,
		AHS,
		RetraceAHS,

		MAX
	};

	EMaterialMode GetMaterialMode()
	{
		EMaterialMode MaterialMode = (EMaterialMode)FMath::Clamp(CVarMegaLightsHardwareRayTracingEvaluateMaterialMode.GetValueOnRenderThread(), 0, 2);

		if (!GRHISupportsRayTracingShaders)
		{
			static bool bWarnOnce = true;

			if (bWarnOnce && MaterialMode != EMaterialMode::Disabled)
			{
				UE_LOG(LogRenderer, Warning, TEXT("Ignoring r.MegaLights.HardwareRayTracing.EvaluateMaterialMode because RHI doesn't support ray tracing shaders. Check platform settings."));
				bWarnOnce = false;
			}

			return EMaterialMode::Disabled;
		}

		return MaterialMode;
	}

	struct FTraceStats
	{
		FRDGBufferRef VSM = nullptr;
		FRDGBufferRef Screen = nullptr;
		FRDGBufferRef World = nullptr;
		FRDGBufferRef WorldMaterialRetrace = nullptr;
		FRDGBufferRef Volume = nullptr;
		FRDGBufferRef TranslucencyVolume0 = nullptr;
		FRDGBufferRef TranslucencyVolume1 = nullptr;
	};
};

class FCompactLightSampleTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompactLightSampleTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FCompactLightSampleTracesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCompactedTraceTexelData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCompactedTraceTexelAllocator)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSampleRays)
		SHADER_PARAMETER(uint32, CompactForScreenSpaceTraces)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 16;
	}

	class FWaveOps : SHADER_PERMUTATION_BOOL("WAVE_OPS");
	using FPermutationDomain = TShaderPermutationDomain<FWaveOps>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FWaveOps>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FCompactLightSampleTracesCS, "/Engine/Private/MegaLights/MegaLightsRayTracing.usf", "CompactLightSampleTracesCS", SF_Compute);

class FVolumeCompactLightSampleTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVolumeCompactLightSampleTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FVolumeCompactLightSampleTracesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsVolumeParameters, MegaLightsVolumeParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCompactedTraceTexelData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCompactedTraceTexelAllocator)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, VolumeLightSamples)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FWaveOps : SHADER_PERMUTATION_BOOL("WAVE_OPS");
	using FPermutationDomain = TShaderPermutationDomain<FWaveOps>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FWaveOps>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FVolumeCompactLightSampleTracesCS, "/Engine/Private/MegaLights/MegaLightsVolumeRayTracing.usf", "VolumeCompactLightSampleTracesCS", SF_Compute);

class FInitCompactedTraceTexelIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCompactedTraceTexelIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitCompactedTraceTexelIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CompactedTraceTexelAllocator)
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

IMPLEMENT_GLOBAL_SHADER(FInitCompactedTraceTexelIndirectArgsCS, "/Engine/Private/MegaLights/MegaLightsRayTracing.usf", "InitCompactedTraceTexelIndirectArgsCS", SF_Compute);

class FPrintTraceStatsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPrintTraceStatsCS)
	SHADER_USE_PARAMETER_STRUCT(FPrintTraceStatsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, VSMIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ScreenIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, WorldIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, WorldMaterialRetraceIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, VolumeIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TranslucencyVolume0IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TranslucencyVolume1IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ShaderPrint::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.SetDefine(TEXT("DEBUG_MODE"), 1);
	}

	static int32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FPrintTraceStatsCS, "/Engine/Private/MegaLights/MegaLightsRayTracing.usf", "PrintTraceStatsCS", SF_Compute);

#if RHI_RAYTRACING

class FHardwareRayTraceLightSamples : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FHardwareRayTraceLightSamples)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(MegaLights::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(MegaLights::FHairVoxelTraceParameters, HairVoxelTraceParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSampleRays)
		SHADER_PARAMETER(float, RayTracingBias)
		SHADER_PARAMETER(float, RayTracingEndBias)
		SHADER_PARAMETER(float, RayTracingNormalBias)
		SHADER_PARAMETER(float, RayTracingPullbackBias)
		// Ray Tracing
		SHADER_PARAMETER(float, NearFieldSceneRadius)
		SHADER_PARAMETER(float, NearFieldMaxTraceDistance)
		SHADER_PARAMETER(float, NearFieldMaxTraceDistanceDitherScale)
		SHADER_PARAMETER(float, FarFieldBias)
		SHADER_PARAMETER(float, FarFieldMaxTraceDistance)
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER(uint32, MeshSectionVisibilityTest)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, FarFieldTLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, RayTracingSceneMetadata)
		// Ray tracing feedback buffer
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWInstanceHitCountBuffer)

		// Inline Ray Tracing
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<Lumen::FHitGroupRootConstants>, HitGroupData)
		SHADER_PARAMETER_STRUCT_REF(FLumenHardwareRayTracingUniformBufferParameters, LumenHardwareRayTracingUniformBuffer)

		// Nanite Ray Tracing
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRayTracingUniformParameters, NaniteRayTracing)
	END_SHADER_PARAMETER_STRUCT()

	class FEvaluateMaterials : SHADER_PERMUTATION_BOOL("MEGA_LIGHTS_EVALUATE_MATERIALS");
	class FLightingChannels : SHADER_PERMUTATION_BOOL("MEGA_LIGHTS_LIGHTING_CHANNELS");
	class FSupportContinuation : SHADER_PERMUTATION_BOOL("SUPPORT_CONTINUATION");
	class FEnableFarFieldTracing : SHADER_PERMUTATION_BOOL("ENABLE_FAR_FIELD_TRACING");
	class FAvoidSelfIntersectionsMode : SHADER_PERMUTATION_ENUM_CLASS("AVOID_SELF_INTERSECTIONS_MODE", LumenHardwareRayTracing::EAvoidSelfIntersectionsMode);
	class FHairVoxelTraces : SHADER_PERMUTATION_BOOL("HAIR_VOXEL_TRACES");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FEvaluateMaterials, FLightingChannels, FSupportContinuation, FEnableFarFieldTracing, FAvoidSelfIntersectionsMode, FHairVoxelTraces, FDebugMode>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (PermutationVector.Get<FEvaluateMaterials>())
		{
			PermutationVector.Set<FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::Disabled);
			PermutationVector.Set<FLightingChannels>(false);
		}

		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (RemapPermutation(PermutationVector) != PermutationVector)
		{
			return false;
		}

		if (ShaderDispatchType == Lumen::ERayTracingShaderDispatchType::Inline && PermutationVector.Get<FEvaluateMaterials>())
		{
			return false;
		}

		return MegaLights::ShouldCompileShaders(Parameters.Platform)  
			&& FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(Parameters, ShaderDispatchType);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::AlwaysResidentPagesWithoutFeedback, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		FPermutationDomain PermutationVector(PermutationId);
		if (PermutationVector.Get<FEvaluateMaterials>())
		{
			return ERayTracingPayloadType::RayTracingMaterial;
		}
		else
		{
			return ERayTracingPayloadType::LumenMinimal;
		}
	}
};

IMPLEMENT_MEGALIGHT_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FHardwareRayTraceLightSamples)

IMPLEMENT_GLOBAL_SHADER(FHardwareRayTraceLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsHardwareRayTracing.usf", "HardwareRayTraceLightSamplesCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FHardwareRayTraceLightSamplesRGS, "/Engine/Private/MegaLights/MegaLightsHardwareRayTracing.usf", "HardwareRayTraceLightSamplesRGS", SF_RayGen);

class FVolumeHardwareRayTraceLightSamples : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FVolumeHardwareRayTraceLightSamples)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(MegaLights::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsVolumeParameters, MegaLightsVolumeParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, RWVolumeLightSamples)
		SHADER_PARAMETER(float, RayTracingBias)
		SHADER_PARAMETER(float, RayTracingEndBias)
		SHADER_PARAMETER(float, RayTracingNormalBias)
		// Ray Tracing
		SHADER_PARAMETER(float, NearFieldSceneRadius)
		SHADER_PARAMETER(float, NearFieldMaxTraceDistance)
		SHADER_PARAMETER(float, NearFieldMaxTraceDistanceDitherScale)
		SHADER_PARAMETER(float, FarFieldBias)
		SHADER_PARAMETER(float, FarFieldMaxTraceDistance)
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER(uint32, MeshSectionVisibilityTest)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, FarFieldTLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, RayTracingSceneMetadata)
		// Ray tracing feedback buffer
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWInstanceHitCountBuffer)

		// Inline Ray Tracing
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<Lumen::FHitGroupRootConstants>, HitGroupData)
		SHADER_PARAMETER_STRUCT_REF(FLumenHardwareRayTracingUniformBufferParameters, LumenHardwareRayTracingUniformBuffer)

		// Nanite Ray Tracing
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRayTracingUniformParameters, NaniteRayTracing)
	END_SHADER_PARAMETER_STRUCT()

	class FTranslucencyLightingVolume : SHADER_PERMUTATION_BOOL("TRANSLUCENCY_LIGHTING_VOLUME");
	class FLightingChannels : SHADER_PERMUTATION_BOOL("MEGA_LIGHTS_LIGHTING_CHANNELS");
	class FEnableFarFieldTracing : SHADER_PERMUTATION_BOOL("ENABLE_FAR_FIELD_TRACING");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FTranslucencyLightingVolume, FLightingChannels, FEnableFarFieldTracing, FDebugMode>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (RemapPermutation(PermutationVector) != PermutationVector)
		{
			return false;
		}

		return MegaLights::ShouldCompileShaders(Parameters.Platform)
			&& FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(Parameters, ShaderDispatchType);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::AlwaysResidentPagesWithoutFeedback, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}
};

IMPLEMENT_MEGALIGHT_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FVolumeHardwareRayTraceLightSamples)

IMPLEMENT_GLOBAL_SHADER(FVolumeHardwareRayTraceLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsVolumeHardwareRayTracing.usf", "VolumeHardwareRayTraceLightSamplesCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FVolumeHardwareRayTraceLightSamplesRGS, "/Engine/Private/MegaLights/MegaLightsVolumeHardwareRayTracing.usf", "VolumeHardwareRayTraceLightSamplesRGS", SF_RayGen);

#endif // RHI_RAYTRACING

class FSoftwareRayTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSoftwareRayTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FSoftwareRayTraceLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(MegaLights::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(MegaLights::FHairVoxelTraceParameters, HairVoxelTraceParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSampleRays)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 64;
	}

	class FHairVoxelTraces : SHADER_PERMUTATION_BOOL("HAIR_VOXEL_TRACES");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FHairVoxelTraces, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		// GPU Scene definitions
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
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

IMPLEMENT_GLOBAL_SHADER(FSoftwareRayTraceLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsRayTracing.usf", "SoftwareRayTraceLightSamplesCS", SF_Compute);

class FVolumeSoftwareRayTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVolumeSoftwareRayTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FVolumeSoftwareRayTraceLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(MegaLights::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsVolumeParameters, MegaLightsVolumeParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, RWVolumeLightSamples)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 64;
	}

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		// GPU Scene definitions
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
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

IMPLEMENT_GLOBAL_SHADER(FVolumeSoftwareRayTraceLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsVolumeRayTracing.usf", "VolumeSoftwareRayTraceLightSamplesCS", SF_Compute);

class FScreenSpaceRayTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenSpaceRayTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenSpaceRayTraceLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(MegaLights::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSampleRays)
		SHADER_PARAMETER(float, MaxHierarchicalScreenTraceIterations)
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, RelativeDepthThickness)
		SHADER_PARAMETER(float, HistoryDepthTestRelativeThickness)
		SHADER_PARAMETER(uint32, MinimumTracingThreadOccupancy)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 64;
	}

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	class FLightingChannels : SHADER_PERMUTATION_BOOL("MEGA_LIGHTS_LIGHTING_CHANNELS");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode, FLightingChannels>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
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

IMPLEMENT_GLOBAL_SHADER(FScreenSpaceRayTraceLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsRayTracing.usf", "ScreenSpaceRayTraceLightSamplesCS", SF_Compute);

class FVirtualShadowMapTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVirtualShadowMapTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FVirtualShadowMapTraceLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(MegaLights::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, VirtualShadowMapSamplingParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSampleRays)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 64;
	}

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters.Platform);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
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

IMPLEMENT_GLOBAL_SHADER(FVirtualShadowMapTraceLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsVSMTracing.usf", "VirtualShadowMapTraceLightSamplesCS", SF_Compute);

#if RHI_RAYTRACING
void FDeferredShadingSceneRenderer::PrepareMegaLightsHardwareRayTracing(const FViewInfo& View, const FScene& Scene, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	const MegaLights::EMaterialMode MaterialMode = MegaLights::GetMaterialMode();
	const bool bUseFarField = MegaLights::UseFarField(*View.Family); // #ml_todo: check if far field has any instances

	if (MegaLights::UseHardwareRayTracing(*View.Family) && MaterialMode != MegaLights::EMaterialMode::Disabled)
	{
		for (int32 DebugModeIt = 0; DebugModeIt < 2; ++DebugModeIt)
		for (int32 HairVoxelTraces = 0; HairVoxelTraces < 2; ++HairVoxelTraces)
		{
			// Check if any pass needs the debug permutation
			const bool bValid = 
				(DebugModeIt == 0 && (MegaLights::GetDebugMode(EMegaLightsInput::GBuffer) == 0 || MegaLights::GetDebugMode(EMegaLightsInput::HairStrands) == 0)) || 
				(DebugModeIt >  0 && (MegaLights::GetDebugMode(EMegaLightsInput::GBuffer) >  0 || MegaLights::GetDebugMode(EMegaLightsInput::HairStrands) >  0));			
			if (!bValid)
			{
				continue;
			}

			FHardwareRayTraceLightSamplesRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FEvaluateMaterials>(true);
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FLightingChannels>(MegaLights::IsUsingLightingChannels(Scene.RayTracingScene));
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FSupportContinuation>(false);
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FEnableFarFieldTracing>(bUseFarField);
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FAvoidSelfIntersectionsMode>(MegaLights::GetAvoidSelfIntersectionsMode());
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FHairVoxelTraces>(HairVoxelTraces != 0);
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FDebugMode>(DebugModeIt != 0);
			PermutationVector = FHardwareRayTraceLightSamplesRGS::RemapPermutation(PermutationVector);

			TShaderRef<FHardwareRayTraceLightSamplesRGS> RayGenerationShader = View.ShaderMap->GetShader<FHardwareRayTraceLightSamplesRGS>(PermutationVector);

			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

void FDeferredShadingSceneRenderer::PrepareMegaLightsHardwareRayTracingLumenMaterial(const FViewInfo& View, const FScene& Scene, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	const MegaLights::EMaterialMode MaterialMode = MegaLights::GetMaterialMode();
	const bool bUseFarField = MegaLights::UseFarField(*View.Family); // #ml_todo: check if far field has any instances

	if (MegaLights::UseHardwareRayTracing(*View.Family) && !MegaLights::UseInlineHardwareRayTracing(*View.Family))
	{
		// Opaque
		for (int32 DebugModeIt = 0; DebugModeIt < 2; ++DebugModeIt)
		for (int32 HairVoxelTraces = 0; HairVoxelTraces < 2; ++HairVoxelTraces)
		{
			// Check if any pass needs the debug permutation
			const bool bValid = 
				(DebugModeIt == 0 && (MegaLights::GetDebugMode(EMegaLightsInput::GBuffer) == 0 || MegaLights::GetDebugMode(EMegaLightsInput::HairStrands) == 0)) || 
				(DebugModeIt >  0 && (MegaLights::GetDebugMode(EMegaLightsInput::GBuffer) >  0 || MegaLights::GetDebugMode(EMegaLightsInput::HairStrands) >  0));			
			if (!bValid)
			{
				continue;
			}

			FHardwareRayTraceLightSamplesRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FEvaluateMaterials>(false);
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FLightingChannels>(MegaLights::IsUsingLightingChannels(Scene.RayTracingScene));
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FSupportContinuation>(MaterialMode == MegaLights::EMaterialMode::RetraceAHS);
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FEnableFarFieldTracing>(bUseFarField);
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FAvoidSelfIntersectionsMode>(MegaLights::GetAvoidSelfIntersectionsMode());
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FHairVoxelTraces>(HairVoxelTraces != 0);
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FDebugMode>(DebugModeIt != 0);
			PermutationVector = FHardwareRayTraceLightSamplesRGS::RemapPermutation(PermutationVector);

			TShaderRef<FHardwareRayTraceLightSamplesRGS> RayGenerationShader = View.ShaderMap->GetShader<FHardwareRayTraceLightSamplesRGS>(PermutationVector);

			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}

		// Volume
		{
			FVolumeHardwareRayTraceLightSamplesRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FVolumeHardwareRayTraceLightSamplesRGS::FTranslucencyLightingVolume>(false);
			PermutationVector.Set<FVolumeHardwareRayTraceLightSamplesRGS::FLightingChannels>(MegaLights::IsUsingLightingChannels(Scene.RayTracingScene));
			PermutationVector.Set<FVolumeHardwareRayTraceLightSamplesRGS::FEnableFarFieldTracing>(bUseFarField);
			PermutationVector.Set<FVolumeHardwareRayTraceLightSamplesRGS::FDebugMode>(MegaLightsVolume::GetDebugMode() != 0);
			PermutationVector = FVolumeHardwareRayTraceLightSamplesRGS::RemapPermutation(PermutationVector);

			TShaderRef<FVolumeHardwareRayTraceLightSamplesRGS> RayGenerationShader = View.ShaderMap->GetShader<FVolumeHardwareRayTraceLightSamplesRGS>(PermutationVector);

			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}

		// Translucency Volume
		{
			FVolumeHardwareRayTraceLightSamplesRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FVolumeHardwareRayTraceLightSamplesRGS::FTranslucencyLightingVolume>(true);
			PermutationVector.Set<FVolumeHardwareRayTraceLightSamplesRGS::FLightingChannels>(MegaLights::IsUsingLightingChannels(Scene.RayTracingScene));
			PermutationVector.Set<FVolumeHardwareRayTraceLightSamplesRGS::FEnableFarFieldTracing>(bUseFarField);
			PermutationVector.Set<FVolumeHardwareRayTraceLightSamplesRGS::FDebugMode>(MegaLightsTranslucencyVolume::GetDebugMode() != 0);
			PermutationVector = FVolumeHardwareRayTraceLightSamplesRGS::RemapPermutation(PermutationVector);

			TShaderRef<FVolumeHardwareRayTraceLightSamplesRGS> RayGenerationShader = View.ShaderMap->GetShader<FVolumeHardwareRayTraceLightSamplesRGS>(PermutationVector);

			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

namespace MegaLights
{
	void SetHardwareRayTracingPassParameters(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const MegaLights::FCompactedTraceParameters& CompactedTraceParameters,
		const FMegaLightsParameters& MegaLightsParameters,
		const FHairVoxelTraceParameters& HairVoxelTraceParameters,
		FRDGTextureRef LightSamples,
		FRDGTextureRef LightSampleRays,
		FHardwareRayTraceLightSamples::FParameters* PassParameters)
	{
		PassParameters->CompactedTraceParameters = CompactedTraceParameters;
		PassParameters->MegaLightsParameters = MegaLightsParameters;
		PassParameters->HairVoxelTraceParameters = HairVoxelTraceParameters;
		PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
		PassParameters->RWLightSampleRays = GraphBuilder.CreateUAV(LightSampleRays);
		PassParameters->RayTracingBias = CVarMegaLightsHardwareRayTracingBias.GetValueOnRenderThread();
		PassParameters->RayTracingEndBias = CVarMegaLightsHardwareRayTracingEndBias.GetValueOnRenderThread();
		PassParameters->RayTracingNormalBias = CVarMegaLightsHardwareRayTracingNormalBias.GetValueOnRenderThread();
		PassParameters->RayTracingPullbackBias = CVarMegaLightsHardwareRayTracingPullbackBias.GetValueOnRenderThread();

		const bool bUseFarField = MegaLights::UseFarField(*View.Family); // #ml_todo: check if far field has any instances

		checkf(View.HasRayTracingScene(), TEXT("TLAS does not exist. Verify that the current pass is represented in Lumen::AnyLumenHardwareRayTracingPassEnabled()."));
		PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
		PassParameters->FarFieldTLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::FarField);
		PassParameters->MaxTraversalIterations = FMath::Max(CVarMegaLightsHardwareRayTracingMaxIterations.GetValueOnRenderThread(), 1);
		PassParameters->MeshSectionVisibilityTest = CVarMegaLightsHardwareRayTracingMeshSectionVisibilityTest.GetValueOnRenderThread();

		// #ml_todo: should use MegaLights specific far field tracing configuration instead of sharing Lumen config?
		PassParameters->NearFieldSceneRadius = Lumen::GetNearFieldSceneRadius(View, bUseFarField);
		PassParameters->NearFieldMaxTraceDistance = Lumen::MaxTraceDistance;
		PassParameters->NearFieldMaxTraceDistanceDitherScale = Lumen::GetNearFieldMaxTraceDistanceDitherScale(bUseFarField);
		PassParameters->FarFieldBias = CVarMegaLightsHardwareRayTracingFarFieldBias.GetValueOnRenderThread();
		PassParameters->FarFieldMaxTraceDistance = CVarMegaLightsHardwareRayTracingFarFieldMaxDistance.GetValueOnRenderThread();

		// Inline
		// TODO: Do we still need to use View.GetPrimaryView()? And if so should it also be done when initializing RayTracingSceneMetadata?
		checkf(View.VisibleRayTracingShaderBindingsFinalizeTask.IsCompleted(), TEXT("VisibleRayTracingShaderBindingsFinalizeTask must be completed before accessing MegaLights HWRT data."));
		PassParameters->HitGroupData = View.GetPrimaryView()->LumenHardwareRayTracingHitDataBuffer ? GraphBuilder.CreateSRV(View.GetPrimaryView()->LumenHardwareRayTracingHitDataBuffer) : nullptr;
		PassParameters->LumenHardwareRayTracingUniformBuffer = View.GetPrimaryView()->LumenHardwareRayTracingUniformBuffer;
		PassParameters->RayTracingSceneMetadata = View.InlineRayTracingBindingDataBuffer ? GraphBuilder.CreateSRV(View.InlineRayTracingBindingDataBuffer) : nullptr;
		PassParameters->NaniteRayTracing = Nanite::GRayTracingManager.GetUniformBuffer();

		// Feedback Buffer
		PassParameters->RWInstanceHitCountBuffer = View.GetRayTracingInstanceHitCountUAV(GraphBuilder);
	}

	void SetHardwareRayTracingPassParameters(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const MegaLights::FCompactedTraceParameters& CompactedTraceParameters,
		const FMegaLightsParameters& MegaLightsParameters,
		FRDGTextureRef VolumeLightSamples,
		FVolumeHardwareRayTraceLightSamples::FParameters* PassParameters)
	{
		PassParameters->CompactedTraceParameters = CompactedTraceParameters;
		PassParameters->MegaLightsParameters = MegaLightsParameters;
		PassParameters->RWVolumeLightSamples = GraphBuilder.CreateUAV(VolumeLightSamples);
		PassParameters->RayTracingBias = CVarMegaLightsHardwareRayTracingBias.GetValueOnRenderThread();
		PassParameters->RayTracingEndBias = CVarMegaLightsHardwareRayTracingEndBias.GetValueOnRenderThread();
		PassParameters->RayTracingNormalBias = CVarMegaLightsHardwareRayTracingNormalBias.GetValueOnRenderThread();

		const bool bUseFarField = MegaLights::UseFarField(*View.Family); // #ml_todo: check if far field has any instances

		checkf(View.HasRayTracingScene(), TEXT("TLAS does not exist. Verify that the current pass is represented in Lumen::AnyLumenHardwareRayTracingPassEnabled()."));
		PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
		PassParameters->FarFieldTLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::FarField);
		PassParameters->MaxTraversalIterations = FMath::Max(CVarMegaLightsHardwareRayTracingMaxIterations.GetValueOnRenderThread(), 1);
		PassParameters->MeshSectionVisibilityTest = CVarMegaLightsHardwareRayTracingMeshSectionVisibilityTest.GetValueOnRenderThread();

		// #ml_todo: should use MegaLights specific far field tracing configuration instead of sharing Lumen config?
		PassParameters->NearFieldSceneRadius = Lumen::GetNearFieldSceneRadius(View, bUseFarField);
		PassParameters->NearFieldMaxTraceDistance = Lumen::MaxTraceDistance;
		PassParameters->NearFieldMaxTraceDistanceDitherScale = Lumen::GetNearFieldMaxTraceDistanceDitherScale(bUseFarField);
		PassParameters->FarFieldBias = CVarMegaLightsHardwareRayTracingFarFieldBias.GetValueOnRenderThread();
		PassParameters->FarFieldMaxTraceDistance = CVarMegaLightsHardwareRayTracingFarFieldMaxDistance.GetValueOnRenderThread();

		// Inline
		// TODO: Do we still need to use View.GetPrimaryView()? And if so should it also be done when initializing RayTracingSceneMetadata?
		checkf(View.VisibleRayTracingShaderBindingsFinalizeTask.IsCompleted(), TEXT("VisibleRayTracingShaderBindingsFinalizeTask must be completed before accessing MegaLights HWRT data."));
		PassParameters->HitGroupData = View.GetPrimaryView()->LumenHardwareRayTracingHitDataBuffer ? GraphBuilder.CreateSRV(View.GetPrimaryView()->LumenHardwareRayTracingHitDataBuffer) : nullptr;
		PassParameters->LumenHardwareRayTracingUniformBuffer = View.GetPrimaryView()->LumenHardwareRayTracingUniformBuffer;
		PassParameters->RayTracingSceneMetadata = View.InlineRayTracingBindingDataBuffer ? GraphBuilder.CreateSRV(View.InlineRayTracingBindingDataBuffer) : nullptr;
		PassParameters->NaniteRayTracing = Nanite::GRayTracingManager.GetUniformBuffer();

		// Feedback Buffer
		PassParameters->RWInstanceHitCountBuffer = View.GetRayTracingInstanceHitCountUAV(GraphBuilder);
	}
}; // namespace MegaLights

#endif

MegaLights::FCompactedTraceParameters MegaLights::CompactMegaLightsTraces(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const FIntPoint SampleBufferSize,
	FRDGTextureRef LightSampleRays,
	const FMegaLightsParameters& MegaLightsParameters,
	bool bCompactForScreenSpaceTraces)
{
	FRDGBufferRef CompactedTraceTexelData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), SampleBufferSize.X * SampleBufferSize.Y),
		TEXT("MegaLightsParameters.CompactedTraceTexelData"));

	FRDGBufferRef CompactedTraceTexelAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1),
		TEXT("MegaLightsParameters.CompactedTraceTexelAllocator"));

	FRDGBufferRef CompactedTraceTexelIndirectArgs = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)ECompactedTraceIndirectArgs::MAX),
		TEXT("MegaLights.CompactedTraceTexelIndirectArgs"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompactedTraceTexelAllocator, PF_R32_UINT), 0);

	// Compact light sample traces before tracing
	{
		FCompactLightSampleTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompactLightSampleTracesCS::FParameters>();
		PassParameters->RWCompactedTraceTexelData = GraphBuilder.CreateUAV(CompactedTraceTexelData, PF_R32_UINT);
		PassParameters->RWCompactedTraceTexelAllocator = GraphBuilder.CreateUAV(CompactedTraceTexelAllocator, PF_R32_UINT);
		PassParameters->MegaLightsParameters = MegaLightsParameters;
		PassParameters->LightSampleRays = LightSampleRays;
		PassParameters->CompactForScreenSpaceTraces = bCompactForScreenSpaceTraces ? 1 : 0;

		const bool bWaveOps = MegaLights::UseWaveOps(View.GetShaderPlatform())
			&& GRHIMinimumWaveSize <= 32
			&& GRHIMaximumWaveSize >= 32;

		FCompactLightSampleTracesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCompactLightSampleTracesCS::FWaveOps>(bWaveOps);
		auto ComputeShader = View.ShaderMap->GetShader<FCompactLightSampleTracesCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(MegaLightsParameters.SampleViewSize, FCompactLightSampleTracesCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompactLightSampleTraces"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	// Setup indirect args for tracing
	{
		FInitCompactedTraceTexelIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompactedTraceTexelIndirectArgsCS::FParameters>();
		PassParameters->MegaLightsParameters = MegaLightsParameters;
		PassParameters->RWIndirectArgs = GraphBuilder.CreateUAV(CompactedTraceTexelIndirectArgs);
		PassParameters->CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(CompactedTraceTexelAllocator, PF_R32_UINT);

		auto ComputeShader = View.ShaderMap->GetShader<FInitCompactedTraceTexelIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitCompactedTraceTexelIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FCompactedTraceParameters Parameters;
	Parameters.CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(CompactedTraceTexelAllocator, PF_R32_UINT);
	Parameters.CompactedTraceTexelData = GraphBuilder.CreateSRV(CompactedTraceTexelData, PF_R32_UINT);
	Parameters.IndirectArgs = CompactedTraceTexelIndirectArgs;
	return Parameters;
}

MegaLights::FCompactedTraceParameters MegaLights::CompactMegaLightsVolumeTraces(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const FIntVector VolumeSampleBufferSize,
	FRDGTextureRef VolumeLightSamples,
	const FMegaLightsParameters& MegaLightsParameters,
	const FMegaLightsVolumeParameters& MegaLightsVolumeParameters)
{
	FRDGBufferRef CompactedTraceTexelData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), VolumeSampleBufferSize.X * VolumeSampleBufferSize.Y * VolumeSampleBufferSize.Z),
		TEXT("MegaLightsParameters.CompactedVolumeTraceTexelData"));

	FRDGBufferRef CompactedTraceTexelAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1),
		TEXT("MegaLightsParameters.CompactedVolumeTraceTexelAllocator"));

	FRDGBufferRef CompactedTraceTexelIndirectArgs = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)ECompactedTraceIndirectArgs::MAX),
		TEXT("MegaLights.CompactedVolumeTraceTexelIndirectArgs"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompactedTraceTexelAllocator, PF_R32_UINT), 0);

	// Compact light sample traces before tracing
	{
		FVolumeCompactLightSampleTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeCompactLightSampleTracesCS::FParameters>();
		PassParameters->RWCompactedTraceTexelData = GraphBuilder.CreateUAV(CompactedTraceTexelData, PF_R32_UINT);
		PassParameters->RWCompactedTraceTexelAllocator = GraphBuilder.CreateUAV(CompactedTraceTexelAllocator, PF_R32_UINT);
		PassParameters->MegaLightsParameters = MegaLightsParameters;
		PassParameters->MegaLightsVolumeParameters = MegaLightsVolumeParameters;
		PassParameters->VolumeLightSamples = VolumeLightSamples;

		const bool bWaveOps = MegaLights::UseWaveOps(View.GetShaderPlatform())
			&& GRHIMinimumWaveSize <= 32
			&& GRHIMaximumWaveSize >= 32;

		FVolumeCompactLightSampleTracesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FVolumeCompactLightSampleTracesCS::FWaveOps>(bWaveOps);
		auto ComputeShader = View.ShaderMap->GetShader<FVolumeCompactLightSampleTracesCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(MegaLightsVolumeParameters.VolumeSampleViewSize, FVolumeCompactLightSampleTracesCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompactVolumeLightSampleTraces"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	// Setup indirect args for tracing
	{
		FInitCompactedTraceTexelIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompactedTraceTexelIndirectArgsCS::FParameters>();
		PassParameters->MegaLightsParameters = MegaLightsParameters;
		PassParameters->RWIndirectArgs = GraphBuilder.CreateUAV(CompactedTraceTexelIndirectArgs);
		PassParameters->CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(CompactedTraceTexelAllocator, PF_R32_UINT);

		auto ComputeShader = View.ShaderMap->GetShader<FInitCompactedTraceTexelIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitCompactedVolumeTraceTexelIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FCompactedTraceParameters Parameters;
	Parameters.CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(CompactedTraceTexelAllocator, PF_R32_UINT);
	Parameters.CompactedTraceTexelData = GraphBuilder.CreateSRV(CompactedTraceTexelData, PF_R32_UINT);
	Parameters.IndirectArgs = CompactedTraceTexelIndirectArgs;
	return Parameters;
}

/**
 * Ray trace light samples using a variety of tracing methods depending on the feature configuration.
 */
void MegaLights::RayTraceLightSamples(
	const FSceneViewFamily& ViewFamily,
	const FViewInfo& View, int32 ViewIndex,
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FVirtualShadowMapArray* VirtualShadowMapArray,
	const FIntPoint SampleBufferSize,
	FRDGTextureRef LightSamples,
	FRDGTextureRef LightSampleRays,
	FIntVector VolumeSampleBufferSize,
	FRDGTextureRef VolumeLightSamples,
	FIntVector TranslucencyVolumeSampleBufferSize,
	TArrayView<FRDGTextureRef> TranslucencyVolumeLightSamples,
	const FMegaLightsParameters& MegaLightsParameters,
	const FMegaLightsVolumeParameters& MegaLightsVolumeParameters,
	const FMegaLightsVolumeParameters& MegaLightsTranslucencyVolumeParameters,
	EMegaLightsInput InputType)
{
	const bool bDebug = MegaLights::GetDebugMode(InputType) != 0;
	const bool bVolumeDebug = MegaLightsVolume::GetDebugMode() != 0;
	const bool bTranslucencyVolumeDebug = MegaLightsTranslucencyVolume::GetDebugMode() != 0;
	const bool bTraceStats = CVarMegaLightsDebugTraceStats.GetValueOnRenderThread();

	const FScene* Scene = static_cast<const FScene*>(ViewFamily.Scene);
#if RHI_RAYTRACING
	const FRayTracingScene& RayTracingScene = Scene->RayTracingScene;
#endif

	FTraceStats TraceStats;

	if (VirtualShadowMapArray)
	{
		FCompactedTraceParameters CompactedTraceParameters = MegaLights::CompactMegaLightsTraces(
			View,
			GraphBuilder,
			SampleBufferSize,
			LightSampleRays,
			MegaLightsParameters,
			/*bCompactForScreenSpaceTraces*/ false);

		TraceStats.VSM = CompactedTraceParameters.IndirectArgs;

		FVirtualShadowMapTraceLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualShadowMapTraceLightSamplesCS::FParameters>();
		PassParameters->CompactedTraceParameters = CompactedTraceParameters;
		PassParameters->MegaLightsParameters = MegaLightsParameters;
		PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
		PassParameters->RWLightSampleRays = GraphBuilder.CreateUAV(LightSampleRays);
		PassParameters->VirtualShadowMapSamplingParameters = VirtualShadowMapArray->GetSamplingParameters(GraphBuilder, ViewIndex);

		FVirtualShadowMapTraceLightSamplesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FVirtualShadowMapTraceLightSamplesCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FVirtualShadowMapTraceLightSamplesCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("VirtualShadowMapTraceLightSamples"),
			ComputeShader,
			PassParameters,
			CompactedTraceParameters.IndirectArgs,
			(int32)MegaLights::ECompactedTraceIndirectArgs::NumTracesDiv64);
	}

	if (MegaLights::UseScreenTraces(InputType))
	{
		FCompactedTraceParameters CompactedTraceParameters = MegaLights::CompactMegaLightsTraces(
			View,
			GraphBuilder,
			SampleBufferSize,
			LightSampleRays,
			MegaLightsParameters,
			/*bCompactForScreenSpaceTraces*/ true);

		TraceStats.Screen = CompactedTraceParameters.IndirectArgs;

		FScreenSpaceRayTraceLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenSpaceRayTraceLightSamplesCS::FParameters>();
		PassParameters->CompactedTraceParameters = CompactedTraceParameters;
		PassParameters->MegaLightsParameters = MegaLightsParameters;
		PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
		PassParameters->RWLightSampleRays = GraphBuilder.CreateUAV(LightSampleRays);
		PassParameters->MaxHierarchicalScreenTraceIterations = CVarMegaLightsScreenTracesMaxIterations.GetValueOnRenderThread();
		PassParameters->MaxTraceDistance = CVarMegaLightsScreenTracesMaxDistance.GetValueOnRenderThread();
		PassParameters->RelativeDepthThickness = CVarMegaLightsScreenTraceRelativeDepthThreshold.GetValueOnRenderThread() * View.ViewMatrices.GetPerProjectionDepthThicknessScale();
		PassParameters->HistoryDepthTestRelativeThickness = 0.0f;
		PassParameters->MinimumTracingThreadOccupancy = CVarMegaLightsScreenTracesMinimumOccupancy.GetValueOnRenderThread();

		FScreenSpaceRayTraceLightSamplesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FScreenSpaceRayTraceLightSamplesCS::FDebugMode>(bDebug);
		PermutationVector.Set<FScreenSpaceRayTraceLightSamplesCS::FLightingChannels>(MegaLights::IsUsingLightingChannels() && View.bUsesLightingChannels);
		auto ComputeShader = View.ShaderMap->GetShader<FScreenSpaceRayTraceLightSamplesCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ScreenSpaceRayTraceLightSamples"),
			ComputeShader,
			PassParameters,
			CompactedTraceParameters.IndirectArgs,
			(int32)MegaLights::ECompactedTraceIndirectArgs::NumTracesDiv64);
	}

	const bool bHairVoxelTraces = HairStrands::HasViewHairStrandsData(View)
		&& InputType != EMegaLightsInput::HairStrands
		&& HairStrands::HasViewHairStrandsVoxelData(View)
		&& CVarMegaLightsHairVoxelTraces.GetValueOnRenderThread() != 0;

	FHairVoxelTraceParameters HairVoxelTraceParameters;
	if (bHairVoxelTraces)
	{
		HairVoxelTraceParameters.HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
		HairVoxelTraceParameters.VirtualVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
	}

	if (CVarMegaLightsWorldSpaceTraces.GetValueOnRenderThread() != 0)
	{
		FCompactedTraceParameters CompactedTraceParameters = MegaLights::CompactMegaLightsTraces(
			View,
			GraphBuilder,
			SampleBufferSize,
			LightSampleRays,
			MegaLightsParameters,
			/*bCompactForScreenSpaceTraces*/ false);

		TraceStats.World = CompactedTraceParameters.IndirectArgs;

		FCompactedTraceParameters CompactedVolumeTraceParameters;
		if (VolumeLightSamples && CVarMegaLightsVolumeWorldSpaceTraces.GetValueOnRenderThread() != 0)
		{
			CompactedVolumeTraceParameters = MegaLights::CompactMegaLightsVolumeTraces(
				View,
				GraphBuilder,
				VolumeSampleBufferSize,
				VolumeLightSamples,
				MegaLightsParameters,
				MegaLightsVolumeParameters);
		}

		TraceStats.Volume = CompactedVolumeTraceParameters.IndirectArgs;

		FCompactedTraceParameters CompactedTranslucencyVolumeTraceParameters[TVC_MAX];
		if (!TranslucencyVolumeLightSamples.IsEmpty() && CVarMegaLightsVolumeWorldSpaceTraces.GetValueOnRenderThread() != 0)
		{
			FMegaLightsVolumeParameters CascadeMegaLightsParameters = MegaLightsTranslucencyVolumeParameters;

			CascadeMegaLightsParameters.TranslucencyVolumeCascadeIndex = 0;
			CompactedTranslucencyVolumeTraceParameters[0] = MegaLights::CompactMegaLightsVolumeTraces(
				View,
				GraphBuilder,
				TranslucencyVolumeSampleBufferSize,
				TranslucencyVolumeLightSamples[0],
				MegaLightsParameters,
				CascadeMegaLightsParameters);

			CascadeMegaLightsParameters.TranslucencyVolumeCascadeIndex = 1;
			CompactedTranslucencyVolumeTraceParameters[1] = MegaLights::CompactMegaLightsVolumeTraces(
				View,
				GraphBuilder,
				TranslucencyVolumeSampleBufferSize,
				TranslucencyVolumeLightSamples[1],
				MegaLightsParameters,
				CascadeMegaLightsParameters);

			TraceStats.TranslucencyVolume0 = CompactedTranslucencyVolumeTraceParameters[0].IndirectArgs;
			TraceStats.TranslucencyVolume1 = CompactedTranslucencyVolumeTraceParameters[1].IndirectArgs;
		}

		if (MegaLights::UseHardwareRayTracing(ViewFamily))
		{
#if RHI_RAYTRACING
			const EMaterialMode MaterialMode = MegaLights::GetMaterialMode();
			const bool bUseFarField = MegaLights::UseFarField(*View.Family); // #ml_todo: check if far field has any instances

			{
				FHardwareRayTraceLightSamples::FParameters* PassParameters = GraphBuilder.AllocParameters<FHardwareRayTraceLightSamples::FParameters>();
				MegaLights::SetHardwareRayTracingPassParameters(
					GraphBuilder,
					View,
					CompactedTraceParameters,
					MegaLightsParameters,
					HairVoxelTraceParameters,
					LightSamples,
					LightSampleRays,
					PassParameters);

				FHardwareRayTraceLightSamples::FPermutationDomain PermutationVector;
				PermutationVector.Set<FHardwareRayTraceLightSamples::FEvaluateMaterials>(MaterialMode == EMaterialMode::AHS);
				PermutationVector.Set<FHardwareRayTraceLightSamples::FSupportContinuation>(MaterialMode == EMaterialMode::RetraceAHS);
				PermutationVector.Set<FHardwareRayTraceLightSamples::FLightingChannels>(MegaLights::IsUsingLightingChannels(RayTracingScene));
				PermutationVector.Set<FHardwareRayTraceLightSamples::FEnableFarFieldTracing>(bUseFarField);
				PermutationVector.Set<FHardwareRayTraceLightSamples::FAvoidSelfIntersectionsMode>(MegaLights::GetAvoidSelfIntersectionsMode());
				PermutationVector.Set<FHardwareRayTraceLightSamples::FHairVoxelTraces>(bHairVoxelTraces);
				PermutationVector.Set<FHardwareRayTraceLightSamples::FDebugMode>(bDebug);
				PermutationVector = FHardwareRayTraceLightSamples::RemapPermutation(PermutationVector);

				if (MegaLights::UseInlineHardwareRayTracing(ViewFamily) && !PermutationVector.Get<FHardwareRayTraceLightSamples::FEvaluateMaterials>())
				{
					FHardwareRayTraceLightSamplesCS::AddMegaLightRayTracingDispatchIndirect(
						GraphBuilder,
						RDG_EVENT_NAME("HardwareRayTraceLightSamples Inline"),
						View,
						PermutationVector,
						PassParameters,
						CompactedTraceParameters.IndirectArgs,
						(int32)MegaLights::ECompactedTraceIndirectArgs::NumTracesDiv32,
						ERDGPassFlags::Compute);
				}
				else
				{
					FHardwareRayTraceLightSamplesRGS::AddMegaLightRayTracingDispatchIndirect(
						GraphBuilder,
						RDG_EVENT_NAME("HardwareRayTraceLightSamples RayGen"),
						View,
						PermutationVector,
						PassParameters,
						PassParameters->CompactedTraceParameters.IndirectArgs,
						(int32)MegaLights::ECompactedTraceIndirectArgs::NumTraces,
						/*bUseMinimalPayload*/ MaterialMode != EMaterialMode::AHS,
						ERDGPassFlags::Compute);
				}
			}

			// Volume
			if (VolumeLightSamples && CVarMegaLightsVolumeWorldSpaceTraces.GetValueOnRenderThread() != 0)
			{
				FVolumeHardwareRayTraceLightSamples::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeHardwareRayTraceLightSamples::FParameters>();
				MegaLights::SetHardwareRayTracingPassParameters(
					GraphBuilder,
					View,
					CompactedVolumeTraceParameters,
					MegaLightsParameters,
					VolumeLightSamples,
					PassParameters);
				PassParameters->MegaLightsVolumeParameters = MegaLightsVolumeParameters;

				FVolumeHardwareRayTraceLightSamples::FPermutationDomain PermutationVector;
				PermutationVector.Set<FVolumeHardwareRayTraceLightSamples::FTranslucencyLightingVolume>(false);
				PermutationVector.Set<FVolumeHardwareRayTraceLightSamples::FLightingChannels>(MegaLights::IsUsingLightingChannels(RayTracingScene));
				PermutationVector.Set<FVolumeHardwareRayTraceLightSamples::FEnableFarFieldTracing>(bUseFarField);
				PermutationVector.Set<FVolumeHardwareRayTraceLightSamples::FDebugMode>(bVolumeDebug);
				PermutationVector = FVolumeHardwareRayTraceLightSamples::RemapPermutation(PermutationVector);

				if (MegaLights::UseInlineHardwareRayTracing(ViewFamily))
				{
					FVolumeHardwareRayTraceLightSamplesCS::AddMegaLightRayTracingDispatchIndirect(
						GraphBuilder,
						RDG_EVENT_NAME("VolumeHardwareRayTraceLightSamples Inline"),
						View,
						PermutationVector,
						PassParameters,
						CompactedVolumeTraceParameters.IndirectArgs,
						(int32)MegaLights::ECompactedTraceIndirectArgs::NumTracesDiv32,
						ERDGPassFlags::Compute);
				}
				else
				{
					FVolumeHardwareRayTraceLightSamplesRGS::AddMegaLightRayTracingDispatchIndirect(
						GraphBuilder,
						RDG_EVENT_NAME("VolumeHardwareRayTraceLightSamples RayGen"),
						View,
						PermutationVector,
						PassParameters,
						PassParameters->CompactedTraceParameters.IndirectArgs,
						(int32)MegaLights::ECompactedTraceIndirectArgs::NumTraces,
						/*bUseMinimalPayload*/ true,
						ERDGPassFlags::Compute);
				}
			}

			// Translucency Volume
			if (!TranslucencyVolumeLightSamples.IsEmpty() && CVarMegaLightsVolumeWorldSpaceTraces.GetValueOnRenderThread() != 0)
			{
				for (uint32 CascadeIndex = 0; CascadeIndex < TVC_MAX; ++CascadeIndex)
				{
					FMegaLightsVolumeParameters CascadeMegaLightsParameters = MegaLightsTranslucencyVolumeParameters;
					CascadeMegaLightsParameters.TranslucencyVolumeCascadeIndex = CascadeIndex;

					FVolumeHardwareRayTraceLightSamples::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeHardwareRayTraceLightSamples::FParameters>();
					MegaLights::SetHardwareRayTracingPassParameters(
						GraphBuilder,
						View,
						CompactedTranslucencyVolumeTraceParameters[CascadeIndex],
						MegaLightsParameters,
						TranslucencyVolumeLightSamples[CascadeIndex],
						PassParameters);
					PassParameters->MegaLightsVolumeParameters = CascadeMegaLightsParameters;

					FVolumeHardwareRayTraceLightSamples::FPermutationDomain PermutationVector;
					PermutationVector.Set<FVolumeHardwareRayTraceLightSamples::FTranslucencyLightingVolume>(true);
					PermutationVector.Set<FVolumeHardwareRayTraceLightSamples::FLightingChannels>(MegaLights::IsUsingLightingChannels(RayTracingScene));
					PermutationVector.Set<FVolumeHardwareRayTraceLightSamples::FEnableFarFieldTracing>(bUseFarField);
					PermutationVector.Set<FVolumeHardwareRayTraceLightSamples::FDebugMode>(bTranslucencyVolumeDebug);
					PermutationVector = FVolumeHardwareRayTraceLightSamples::RemapPermutation(PermutationVector);

					if (MegaLights::UseInlineHardwareRayTracing(ViewFamily))
					{
						FVolumeHardwareRayTraceLightSamplesCS::AddMegaLightRayTracingDispatchIndirect(
							GraphBuilder,
							RDG_EVENT_NAME("TranslucencyVolumeHardwareRayTraceLightSamples Inline"),
							View,
							PermutationVector,
							PassParameters,
							CompactedTranslucencyVolumeTraceParameters[CascadeIndex].IndirectArgs,
							(int32)MegaLights::ECompactedTraceIndirectArgs::NumTracesDiv32,
							ERDGPassFlags::Compute);
					}
					else
					{
						FVolumeHardwareRayTraceLightSamplesRGS::AddMegaLightRayTracingDispatchIndirect(
							GraphBuilder,
							RDG_EVENT_NAME("TranslucencyVolumeHardwareRayTraceLightSamples RayGen"),
							View,
							PermutationVector,
							PassParameters,
							PassParameters->CompactedTraceParameters.IndirectArgs,
							(int32)MegaLights::ECompactedTraceIndirectArgs::NumTraces,
							/*bUseMinimalPayload*/ true,
							ERDGPassFlags::Compute);
					}
				}
			}

			if (MaterialMode == EMaterialMode::RetraceAHS)
			{
				FCompactedTraceParameters RetraceCompactedTraceParameters = MegaLights::CompactMegaLightsTraces(
					View,
					GraphBuilder,
					SampleBufferSize,
					LightSampleRays,
					MegaLightsParameters,
					/*bCompactForScreenSpaceTraces*/ false);

				TraceStats.WorldMaterialRetrace = RetraceCompactedTraceParameters.IndirectArgs;

				FHardwareRayTraceLightSamples::FParameters* PassParameters = GraphBuilder.AllocParameters<FHardwareRayTraceLightSamples::FParameters>();
				MegaLights::SetHardwareRayTracingPassParameters(
					GraphBuilder,
					View,
					RetraceCompactedTraceParameters,
					MegaLightsParameters,
					HairVoxelTraceParameters,
					LightSamples,
					LightSampleRays,
					PassParameters);

				FHardwareRayTraceLightSamples::FPermutationDomain PermutationVector;
				PermutationVector.Set<FHardwareRayTraceLightSamples::FEvaluateMaterials>(true);
				PermutationVector.Set<FHardwareRayTraceLightSamples::FSupportContinuation>(false);
				PermutationVector.Set<FHardwareRayTraceLightSamples::FEnableFarFieldTracing>(bUseFarField);
				PermutationVector.Set<FHardwareRayTraceLightSamples::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::Disabled);
				PermutationVector.Set<FHardwareRayTraceLightSamples::FHairVoxelTraces>(bHairVoxelTraces);
				PermutationVector.Set<FHardwareRayTraceLightSamples::FDebugMode>(bDebug);
				PermutationVector = FHardwareRayTraceLightSamples::RemapPermutation(PermutationVector);

				FHardwareRayTraceLightSamplesRGS::AddMegaLightRayTracingDispatchIndirect(
					GraphBuilder,
					RDG_EVENT_NAME("HardwareRayTraceLightSamples RayGen (material retrace)"),
					View,
					PermutationVector,
					PassParameters,
					PassParameters->CompactedTraceParameters.IndirectArgs,
					(int32)MegaLights::ECompactedTraceIndirectArgs::NumTraces,
					/*bUseMinimalPayload*/ false,
					ERDGPassFlags::Compute);
			}
			#endif
		}
		else
		{
			ensure(MegaLights::IsUsingGlobalSDF(ViewFamily));

			// GBuffer
			{
				FSoftwareRayTraceLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSoftwareRayTraceLightSamplesCS::FParameters>();
				PassParameters->CompactedTraceParameters = CompactedTraceParameters;
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->HairVoxelTraceParameters = HairVoxelTraceParameters;
				PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
				PassParameters->LightSampleRays = LightSampleRays;

				FSoftwareRayTraceLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FSoftwareRayTraceLightSamplesCS::FHairVoxelTraces>(bHairVoxelTraces);
				PermutationVector.Set<FSoftwareRayTraceLightSamplesCS::FDebugMode>(bDebug);
				auto ComputeShader = View.ShaderMap->GetShader<FSoftwareRayTraceLightSamplesCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("SoftwareRayTraceLightSamples"),
					ComputeShader,
					PassParameters,
					CompactedTraceParameters.IndirectArgs,
					0);
			}

			// Volume
			if (VolumeLightSamples && CVarMegaLightsVolumeWorldSpaceTraces.GetValueOnRenderThread() != 0)
			{
				FVolumeSoftwareRayTraceLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeSoftwareRayTraceLightSamplesCS::FParameters>();
				PassParameters->CompactedTraceParameters = CompactedVolumeTraceParameters;
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->MegaLightsVolumeParameters = MegaLightsVolumeParameters;
				PassParameters->RWVolumeLightSamples = GraphBuilder.CreateUAV(VolumeLightSamples);

				FVolumeSoftwareRayTraceLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FVolumeSoftwareRayTraceLightSamplesCS::FDebugMode>(bVolumeDebug);
				auto ComputeShader = View.ShaderMap->GetShader<FVolumeSoftwareRayTraceLightSamplesCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("VolumeSoftwareRayTraceLightSamples"),
					ComputeShader,
					PassParameters,
					CompactedVolumeTraceParameters.IndirectArgs,
					0);
			}

			// TODO: Translucency Volume
		}
	}

	if (bTraceStats)
	{
		FRDGBufferRef NullIndirectArgs = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)ECompactedTraceIndirectArgs::MAX),
			TEXT("MegaLights.NullIndirectArgs"));

		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(NullIndirectArgs, PF_R32_UINT), 0);

		FPrintTraceStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPrintTraceStatsCS::FParameters>();
		PassParameters->VSMIndirectArgs = GraphBuilder.CreateSRV(TraceStats.VSM ? TraceStats.VSM : NullIndirectArgs, PF_R32_UINT);
		PassParameters->ScreenIndirectArgs = GraphBuilder.CreateSRV(TraceStats.Screen ? TraceStats.Screen : NullIndirectArgs, PF_R32_UINT);
		PassParameters->WorldIndirectArgs = GraphBuilder.CreateSRV(TraceStats.World ? TraceStats.World : NullIndirectArgs, PF_R32_UINT);
		PassParameters->WorldMaterialRetraceIndirectArgs = GraphBuilder.CreateSRV(TraceStats.WorldMaterialRetrace ? TraceStats.WorldMaterialRetrace : NullIndirectArgs, PF_R32_UINT);
		PassParameters->VolumeIndirectArgs = GraphBuilder.CreateSRV(TraceStats.Volume ? TraceStats.Volume : NullIndirectArgs, PF_R32_UINT);
		PassParameters->TranslucencyVolume0IndirectArgs = GraphBuilder.CreateSRV(TraceStats.TranslucencyVolume0 ? TraceStats.TranslucencyVolume0 : NullIndirectArgs, PF_R32_UINT);
		PassParameters->TranslucencyVolume1IndirectArgs = GraphBuilder.CreateSRV(TraceStats.TranslucencyVolume1 ? TraceStats.TranslucencyVolume1 : NullIndirectArgs, PF_R32_UINT);

		ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);

		auto ComputeShader = View.ShaderMap->GetShader<FPrintTraceStatsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("PrintTraceStats"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}
}
