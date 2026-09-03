// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteCullRaster.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "NaniteVisualizationData.h"
#include "NaniteDefinitions.h"
#include "NaniteSceneProxy.h"
#include "NaniteVertexFactory.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "SceneTextureParameters.h"
#include "GPUScene.h"
#include "RendererModule.h"
#include "Rendering/NaniteStreamingManager.h"
#include "SystemTextures.h"
#include "ComponentRecreateRenderStateContext.h"
#include "VirtualShadowMaps/VirtualShadowMapCacheManager.h"
#include "SceneTextureReductions.h"
#include "Engine/Engine.h"
#include "RenderGraphUtils.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "DynamicResolutionState.h"
#include "Lumen/Lumen.h"
#include "TessellationTable.h"
#include "SceneCulling/SceneCullingRenderer.h"
#include "PSOPrecacheValidation.h"
#include "UnrealEngine.h"
#include "MaterialCache/MaterialCache.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("CullingContexts"), STAT_NaniteCullingContexts, STATGROUP_Nanite);

#define CULLING_PASS_NO_OCCLUSION		0
#define CULLING_PASS_OCCLUSION_MAIN		1
#define CULLING_PASS_OCCLUSION_POST		2
#define CULLING_PASS_EXPLICIT_LIST		3

#if NANITE_CANDIDATE_CLUSTER_SIZE_DWORDS == 3
static_assert(	NANITE_NUM_CULLING_FLAG_BITS + NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS_BITS + NANITE_MAX_INSTANCES_BITS +
				NANITE_ASSEMBLY_TRANSFORM_INDEX_BITS + NANITE_POOL_CLUSTER_REF_BITS + NANITE_NUM_DEPTH_BUCKETS_PER_BLOCK_BITS <= 96,
				"FVisibleCluster fields don't fit in 96bits");
#else
static_assert(NANITE_CANDIDATE_CLUSTER_SIZE_DWORDS == 2);
static_assert(NANITE_NUM_CULLING_FLAG_BITS + NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS_BITS + NANITE_MAX_INSTANCES_BITS + NANITE_POOL_CLUSTER_REF_BITS <= 64, "FVisibleCluster fields don't fit in 64bits");
#endif
static_assert(1 + NANITE_NUM_CULLING_FLAG_BITS + NANITE_MAX_INSTANCES_BITS <= 32, "FCandidateNode.x fields don't fit in 32bits");
static_assert(1 + NANITE_MAX_NODES_PER_PRIMITIVE_BITS + NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS_BITS <= 32, "FCandidateNode.y fields don't fit in 32bits");
static_assert(1 + NANITE_MAX_BVH_NODES_PER_GROUP <= 32, "FCandidateNode.z fields don't fit in 32bits");
static_assert(NANITE_MAX_INSTANCES <= MAX_INSTANCE_ID, "Nanite must be able to represent the full scene instance ID range");

static TAutoConsoleVariable<int32> CVarNaniteEnableAsyncRasterization(
	TEXT("r.Nanite.AsyncRasterization"),
	1,
	TEXT("If available, run Nanite compute rasterization as asynchronous compute."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteAsyncRasterizeShadowDepths(
	TEXT("r.Nanite.AsyncRasterization.ShadowDepths"),
	0,
	TEXT("If available, run Nanite compute rasterization of shadows as asynchronous compute."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteAsyncRasterizeCustomPass(
	TEXT("r.Nanite.AsyncRasterization.CustomPass"),
	1,
	TEXT("If available, run Nanite compute rasterization of custom passes as asynchronous compute."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteAsyncRasterizeLumenMeshCards(
	TEXT("r.Nanite.AsyncRasterization.LumenMeshCards"),
	0,
	TEXT("If available, run Nanite compute rasterization of Lumen mesh cards as asynchronous compute."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteComputeRasterization(
	TEXT("r.Nanite.ComputeRasterization"),
	1,
	TEXT("Whether to allow compute rasterization. When disabled all rasterization will go through the hardware path."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteProgrammableRaster(
	TEXT("r.Nanite.ProgrammableRaster"),
	1,
	TEXT("Whether to allow programmable raster. When disabled all rasterization will go through the fixed function path."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteTessellation(
	TEXT("r.Nanite.Tessellation"),
	1,
	TEXT("Whether to enable runtime tessellation."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteFilterPrimitives(
	TEXT("r.Nanite.FilterPrimitives"),
	1,
	TEXT("Whether per-view filtering of primitive is enabled."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteMeshShaderRasterization(
	TEXT("r.Nanite.MeshShaderRasterization"),
	1,
	TEXT("If available, use mesh shaders for hardware rasterization."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNanitePrimShaderRasterization(
	TEXT("r.Nanite.PrimShaderRasterization"),
	1,
	TEXT("If available, use primitive shaders for hardware rasterization."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteVSMInvalidateOnLODDelta(
	TEXT("r.Nanite.VSMInvalidateOnLODDelta"),
	0,
	TEXT("Experimental: Clusters that are not streamed in to LOD matching the computed Nanite LOD estimate will trigger VSM invalidation such that they are re-rendered when streaming completes.\n")
	TEXT("  NOTE: May cause a large increase in invalidations in cases where the streamer has difficulty keeping up (a future version will need to throttle the invalidations and/or add a threshold)."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteRasterSetupTask(
	TEXT("r.Nanite.RasterSetupTask"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteRasterSetupCache(
	TEXT("r.Nanite.RasterSetupCache"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarNaniteMaxPixelsPerEdge(
	TEXT("r.Nanite.MaxPixelsPerEdge"),
	1.0f,
	TEXT("The triangle edge length that the Nanite runtime targets, measured in pixels."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarNaniteImposterMaxPixels(
	TEXT("r.Nanite.ImposterMaxPixels"),
	5,
	TEXT("The maximum size of imposters measured in pixels."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarNaniteMinPixelsPerEdgeHW(
	TEXT("r.Nanite.MinPixelsPerEdgeHW"),
	32.0f,
	TEXT("The triangle edge length in pixels at which Nanite starts using the hardware rasterizer."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarNaniteDicingRate(
	TEXT("r.Nanite.DicingRate"),
	2.0f,
	TEXT("Size of the micropolygons that Nanite tessellation will dice to, measured in pixels."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarNaniteMaxPatchesPerGroup(
	TEXT("r.Nanite.MaxPatchesPerGroup"),
	5,
	TEXT("Maximum number of patches to process per rasterizer group."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarNaniteDepthBucketsMinZ(
	TEXT("r.Nanite.DepthBucketsMinZ"),
	1000.0f,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarNaniteDepthBucketsMaxZ(
	TEXT("r.Nanite.DepthBucketsMaxZ"),
	100000.0f,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteDepthBucketing(
	TEXT("r.Nanite.DepthBucketing"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteDepthBucketPixelProgrammable(
	TEXT("r.Nanite.DepthBucketPixelProgrammable"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe
);

// 0 : Disabled
// 1 : Pixel Clear
// 2 : Tile Clear
static TAutoConsoleVariable<int32> CVarNaniteFastVisBufferClear(
	TEXT("r.Nanite.FastVisBufferClear"),
	1,
	TEXT("Whether the fast clear optimization is enabled. Set to 2 for tile clear."),
	ECVF_RenderThreadSafe
);

// Support a max of 3 unique materials per visible cluster (i.e. if all clusters are fast path and use full range, never run out of space).
static TAutoConsoleVariable<float> CVarNaniteRasterIndirectionMultiplier(
	TEXT("r.Nanite.RasterIndirectionMultiplier"),
	3.0f,
	TEXT(""),
	ECVF_RenderThreadSafe
);

// TODO: Heavy work in progress, do not use
static TAutoConsoleVariable<int32> CVarNaniteBundleRaster(
	TEXT("r.Nanite.Bundle.Raster"),
	0,
	TEXT("Whether to enable Nanite shader bundle dispatch for raster"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		// We need to recreate scene proxies so that raster state can be re-evaluated.
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteBundleRasterSW(
	TEXT("r.Nanite.Bundle.RasterSW"),
	1,
	TEXT("Whether to enable Nanite shader bundle dispatch for Software raster"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		// We need to recreate scene proxies so that raster state can be re-evaluated.
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteBundleRasterHW(
	TEXT("r.Nanite.Bundle.RasterHW"),
	1,
	TEXT("Whether to enable Nanite shader bundle dispatch for Hardware raster"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		// We need to recreate scene proxies so that raster state can be re-evaluated.
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteRasterSort(
	TEXT("r.Nanite.RasterSort"),
	1,
	TEXT("Whether to enable sorting of rasterizer dispatches and draws"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteCullingHZB(
	TEXT("r.Nanite.Culling.HZB"),
	1,
	TEXT("Set to 0 to test disabling Nanite culling due to occlusion by the hierarchical depth buffer."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteCullingFrustum(
	TEXT("r.Nanite.Culling.Frustum"),
	1,
	TEXT("Set to 0 to test disabling Nanite culling due to being outside of the view frustum."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteCullingGlobalClipPlane(
	TEXT("r.Nanite.Culling.GlobalClipPlane"),
	1,
	TEXT("Set to 0 to test disabling Nanite culling due to being beyond the global clip plane.\n")
	TEXT("NOTE: Has no effect if r.AllowGlobalClipPlane=0."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteCullingDrawDistance(
	TEXT("r.Nanite.Culling.DrawDistance"),
	1,
	TEXT("Set to 0 to test disabling Nanite culling due to instance draw distance."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteCullingWPODisableDistance(
	TEXT("r.Nanite.Culling.WPODisableDistance"),
	1,
	TEXT("Set to 0 to test disabling 'World Position Offset Disable Distance' for Nanite instances."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteCullingShowAssemblyParts(
	TEXT("r.Nanite.Culling.ShowAssemblyParts"),
	1,
	TEXT("Set to 0 to test disabling all Nanite Assembly parts."),
	ECVF_RenderThreadSafe
);

int32 GNaniteCullingTwoPass = 1;
static FAutoConsoleVariableRef CVarNaniteCullingTwoPass(
	TEXT("r.Nanite.Culling.TwoPass"),
	GNaniteCullingTwoPass,
	TEXT("Set to 0 to test disabling two pass occlusion culling."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLargePageRectThreshold(
	TEXT("r.Nanite.LargePageRectThreshold"),
	128,
	TEXT("Threshold for the size in number of virtual pages overlapped of a candidate cluster to be recorded as large in the stats."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNanitePersistentThreadsCulling(
	TEXT("r.Nanite.PersistentThreadsCulling"),
	0,
	TEXT("Perform node and cluster culling in one combined kernel using persistent threads.")
	TEXT("It doesn't scale threads with GPU size and relies on scheduler behavior, so it is not recommended for non-fixed hardware platforms."),
	ECVF_RenderThreadSafe
);

// i.e. if r.Nanite.MaxPixelsPerEdge is 1.0 and r.Nanite.PrimaryRaster.PixelsPerEdgeScaling is 20%, when heavily over budget r.Nanite.MaxPixelsPerEdge will be scaled to to 5.0
static TAutoConsoleVariable<float> CVarNanitePrimaryPixelsPerEdgeScalingPercentage(
	TEXT("r.Nanite.PrimaryRaster.PixelsPerEdgeScaling"),
	30.0f, // 100% - no scaling - set to < 100% to scale pixel error when over budget
	TEXT("Lower limit percentage to scale the Nanite primary raster MaxPixelsPerEdge value when over budget."),
	ECVF_RenderThreadSafe | ECVF_Default);

// i.e. if r.Nanite.MaxPixelsPerEdge is 1.0 and r.Nanite.ShadowRaster.PixelsPerEdgeScaling is 20%, when heavily over budget r.Nanite.MaxPixelsPerEdge will be scaled to to 5.0
static TAutoConsoleVariable<float> CVarNaniteShadowPixelsPerEdgeScalingPercentage(
	TEXT("r.Nanite.ShadowRaster.PixelsPerEdgeScaling"),
	100.0f, // 100% - no scaling - set to < 100% to scale pixel error when over budget
	TEXT("Lower limit percentage to scale the Nanite shadow raster MaxPixelsPerEdge value when over budget."),
	ECVF_RenderThreadSafe | ECVF_Default);

static TAutoConsoleVariable<float> CVarNanitePrimaryTimeBudgetMs(
	TEXT("r.Nanite.PrimaryRaster.TimeBudgetMs"),
	DynamicRenderScaling::FHeuristicSettings::kBudgetMsDisabled,
	TEXT("Frame's time budget for Nanite primary raster in milliseconds."),
	ECVF_RenderThreadSafe | ECVF_Default);

static TAutoConsoleVariable<float> CVarNaniteShadowTimeBudgetMs(
	TEXT("r.Nanite.ShadowRaster.TimeBudgetMs"),
	DynamicRenderScaling::FHeuristicSettings::kBudgetMsDisabled,
	TEXT("Frame's time budget for Nanite shadow raster in milliseconds."),
	ECVF_RenderThreadSafe | ECVF_Default);

static TAutoConsoleVariable<float> CVarNaniteOccludedInstancesBufferSizeMultiplier(
	TEXT("r.Nanite.OccludedInstancesBufferSizeMultiplier"),
	1.0f,
	TEXT("DEBUG"),
	ECVF_RenderThreadSafe | ECVF_Default);

static TAutoConsoleVariable<int32> CVarNaniteInstanceHierarchyArgsMaxWorkGroups(
	TEXT("r.Nanite.InstanceHierarchyArgsMaxWorkGroups"),
	4*1024*1024,
	TEXT("Sanitize instance hierarchy arguments to prevent dispatching more workgroups than there are items to consume.\n")
	TEXT("  Sets the dispatch work group size to the minimum of the group work buffer size and the value provided in this cvar.\n")
	TEXT("  The minimum is 32 (anything lower is ignored).\n")
	TEXT("	NOTE: This cvar is only for testing/hot fixing purposes."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNaniteAllowStaticGeometryPath(
	TEXT("r.Nanite.StaticGeometryInstanceCull"),
	false,
	TEXT("If enabled (default: off) static instances are use a specialized instance culling permutation which doesn't need to use the previous transform, reducing register pressure significantly."),
	ECVF_RenderThreadSafe
);

extern TAutoConsoleVariable<int32> CVarNaniteBundleEmulation;

extern bool CanUseShaderBundleWorkGraph(EShaderPlatform Platform);

static bool CanUseShaderBundleWorkGraphSW(EShaderPlatform Platform)
{
	return CanUseShaderBundleWorkGraph(Platform);
}

static bool CanUseShaderBundleWorkGraphHW(EShaderPlatform Platform)
{
	return CanUseShaderBundleWorkGraph(Platform) && !!GRHIGlobals.ShaderBundles.SupportsWorkGraphGraphicsDispatch && RHISupportsWorkGraphsTier1_1(Platform);
}

static bool UseWorkGraphForRasterBundles(EShaderPlatform Platform)
{
	return CVarNaniteBundleRaster.GetValueOnRenderThread() != 0 && CVarNaniteBundleEmulation.GetValueOnRenderThread() == 0 && CanUseShaderBundleWorkGraph(Platform);
}

static DynamicRenderScaling::FHeuristicSettings GetDynamicNaniteScalingPrimarySettings()
{
	const float PixelsPerEdgeScalingPercentage = FMath::Clamp(CVarNanitePrimaryPixelsPerEdgeScalingPercentage.GetValueOnAnyThread(), 1.0f, 100.0f);

	DynamicRenderScaling::FHeuristicSettings BucketSetting;
	BucketSetting.Model = DynamicRenderScaling::EHeuristicModel::Linear;
	BucketSetting.bModelScalesWithPrimaryScreenPercentage = false; // r.Nanite.MaxPixelsPerEdge is not scaled by dynamic resolution of the primary view
	BucketSetting.MinResolutionFraction = DynamicRenderScaling::PercentageToFraction(PixelsPerEdgeScalingPercentage);
	BucketSetting.MaxResolutionFraction = DynamicRenderScaling::PercentageToFraction(100.0f);
	BucketSetting.BudgetMs = CVarNanitePrimaryTimeBudgetMs.GetValueOnAnyThread();
	BucketSetting.ChangeThreshold = DynamicRenderScaling::PercentageToFraction(1.0f);
	BucketSetting.TargetedHeadRoom = DynamicRenderScaling::PercentageToFraction(5.0f); // 5% headroom
	BucketSetting.UpperBoundQuantization = DynamicRenderScaling::FHeuristicSettings::kDefaultUpperBoundQuantization;
	return BucketSetting;
}

static DynamicRenderScaling::FHeuristicSettings GetDynamicNaniteScalingShadowSettings()
{
	const float PixelsPerEdgeScalingPercentage = FMath::Clamp(CVarNaniteShadowPixelsPerEdgeScalingPercentage.GetValueOnAnyThread(), 1.0f, 100.0f);

	DynamicRenderScaling::FHeuristicSettings BucketSetting;
	BucketSetting.Model = DynamicRenderScaling::EHeuristicModel::Linear;
	BucketSetting.bModelScalesWithPrimaryScreenPercentage = false; // r.Nanite.MaxPixelsPerEdge is not scaled by dynamic resolution of the primary view
	BucketSetting.MinResolutionFraction = DynamicRenderScaling::PercentageToFraction(PixelsPerEdgeScalingPercentage);
	BucketSetting.MaxResolutionFraction = DynamicRenderScaling::PercentageToFraction(100.0f);
	BucketSetting.BudgetMs = CVarNaniteShadowTimeBudgetMs.GetValueOnAnyThread();
	BucketSetting.ChangeThreshold = DynamicRenderScaling::PercentageToFraction(1.0f);
	BucketSetting.TargetedHeadRoom = DynamicRenderScaling::PercentageToFraction(5.0f); // 5% headroom
	BucketSetting.UpperBoundQuantization = DynamicRenderScaling::FHeuristicSettings::kDefaultUpperBoundQuantization;
	return BucketSetting;
}

DynamicRenderScaling::FBudget GDynamicNaniteScalingPrimary(TEXT("DynamicNaniteScalingPrimary"), &GetDynamicNaniteScalingPrimarySettings);
DynamicRenderScaling::FBudget GDynamicNaniteScalingShadow( TEXT("DynamicNaniteScalingShadow"),  &GetDynamicNaniteScalingShadowSettings);

extern int32 GNaniteShowStats;
extern int32 GSkipDrawOnPSOPrecaching;

// Set to 1 to pretend all programmable raster draws are not precached yet
TAutoConsoleVariable<int32> CVarNaniteTestPrecacheDrawSkipping(
	TEXT("r.Nanite.TestPrecacheDrawSkipping"),
	0,
	TEXT("Set to 1 to pretend all programmable raster draws are not precached yet."),
	ECVF_RenderThreadSafe
);

static bool UseRasterSetupCache()
{
	// The raster setup cache is disabled in the editor due to shader map invalidations.
#if WITH_EDITOR
	return false;
#else
	return CVarNaniteRasterSetupCache.GetValueOnRenderThread() > 0;
#endif
}

static bool UseMeshShader(EShaderPlatform ShaderPlatform, Nanite::EPipeline Pipeline)
{
	if (!FDataDrivenShaderPlatformInfo::GetSupportsMeshShadersTier1(ShaderPlatform))
	{
		return false;
	}

	// Disable mesh shaders if global clip planes are enabled and the platform cannot support MS with clip distance output
	static const auto AllowGlobalClipPlaneVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowGlobalClipPlane"));
	static const bool bAllowGlobalClipPlane = (AllowGlobalClipPlaneVar && AllowGlobalClipPlaneVar->GetValueOnAnyThread() != 0);
	const bool bMSSupportsClipDistance = FDataDrivenShaderPlatformInfo::GetSupportsMeshShadersWithClipDistance(ShaderPlatform);

	// We require tier1 support to utilize primitive attributes
	const bool bSupported = CVarNaniteMeshShaderRasterization.GetValueOnAnyThread() != 0 && GRHISupportsMeshShadersTier1 && (!bAllowGlobalClipPlane || bMSSupportsClipDistance);
	return bSupported;
}

static bool UsePrimitiveShader()
{
	return CVarNanitePrimShaderRasterization.GetValueOnAnyThread() != 0 && GRHISupportsPrimitiveShaders;
}

static bool ShouldCompileSvBarycentricPermutation(EShaderPlatform ShaderPlatform, bool bPixelProgrammable, bool bMeshShaderRasterPath, bool bAllowSvBarycentrics)
{
	if (!bPixelProgrammable || !bMeshShaderRasterPath || FDataDrivenShaderPlatformInfo::GetSupportsBarycentricsIntrinsics(ShaderPlatform))
	{
		return bAllowSvBarycentrics == false;
	}

	const ERHIFeatureSupport BarycentricsSemanticSupport = FDataDrivenShaderPlatformInfo::GetSupportsBarycentricsSemantic(ShaderPlatform);

	if (BarycentricsSemanticSupport == ERHIFeatureSupport::RuntimeGuaranteed)
	{
		// We don't want disabled permutations when support is guaranteed
		return bAllowSvBarycentrics == true;
	}

	if (BarycentricsSemanticSupport == ERHIFeatureSupport::Unsupported)
	{
		return bAllowSvBarycentrics == false;
	}

	// BarycentricsSemanticSupport == ERHIFeatureSupport::RuntimeDependent
	return true;
}

static bool ShouldUseSvBarycentricPermutation(EShaderPlatform ShaderPlatform, bool bPixelProgrammable, bool bMeshShaderRasterPath)
{
	// Only used with pixel programmable shaders with the Mesh shaders raster path when intrinsics are not supported
	if (!bPixelProgrammable || !bMeshShaderRasterPath || FDataDrivenShaderPlatformInfo::GetSupportsBarycentricsIntrinsics(ShaderPlatform))
	{
		return false;
	}

	const ERHIFeatureSupport BarycentricsSemanticSupport = FDataDrivenShaderPlatformInfo::GetSupportsBarycentricsSemantic(ShaderPlatform);

	// Only use the barycentric permutation when support is runtime guaranteed or if we're dependent and the global cap flag is set.
	if (BarycentricsSemanticSupport == ERHIFeatureSupport::RuntimeGuaranteed ||
		(BarycentricsSemanticSupport == ERHIFeatureSupport::RuntimeDependent && GRHIGlobals.SupportsBarycentricsSemantic))
	{
		return true;
	}

	return false;
}

enum class ERasterHardwarePath : uint8
{
	VertexShader,
	PrimitiveShader,
	MeshShaderWrapped,
	MeshShaderNV,
	MeshShader,
};

static ERasterHardwarePath GetRasterHardwarePath(EShaderPlatform ShaderPlatform, Nanite::EPipeline Pipeline)
{
	ERasterHardwarePath HardwarePath = ERasterHardwarePath::VertexShader;
	
	if (UseMeshShader(ShaderPlatform, Pipeline))
	{
		// TODO: Cleaner detection later
		const bool bNVExtension = FDataDrivenShaderPlatformInfo::GetMaxMeshShaderThreadGroupSize(ShaderPlatform) == 32u;

		if (bNVExtension)
		{
			HardwarePath = ERasterHardwarePath::MeshShaderNV;
		}
		else if (FDataDrivenShaderPlatformInfo::GetRequiresUnwrappedMeshShaderArgs(ShaderPlatform))
		{
			HardwarePath = ERasterHardwarePath::MeshShader;
		}
		else
		{
			HardwarePath = ERasterHardwarePath::MeshShaderWrapped;
		}
	}
	else if (UsePrimitiveShader())
	{
		HardwarePath = ERasterHardwarePath::PrimitiveShader;
	}

	return HardwarePath;
}

static bool IsMeshShaderRasterPath(const ERasterHardwarePath HardwarePath)
{
	return
	(
		HardwarePath == ERasterHardwarePath::MeshShader ||
		HardwarePath == ERasterHardwarePath::MeshShaderNV ||
		HardwarePath == ERasterHardwarePath::MeshShaderWrapped
	);
}

static uint32 GetMaxPatchesPerGroup()
{
	return (uint32)FMath::Max(1, FMath::Min(CVarNaniteMaxPatchesPerGroup.GetValueOnRenderThread(), GRHIMinimumWaveSize / 3));
}

static bool UseAsyncComputeForShadowMaps(const FViewFamilyInfo& ViewFamily)
{
	// Automatically disabled when Lumen async is enabled, as it then delays graphics pipe too much and regresses overall frame performance
	return CVarNaniteAsyncRasterizeShadowDepths.GetValueOnRenderThread() != 0 && !Lumen::UseAsyncCompute(ViewFamily);
}

static bool UseAsyncComputeForCustomPass(const FViewFamilyInfo& ViewFamily)
{
	// Automatically disabled when Lumen async is enabled, as it then delays graphics pipe too much and regresses overall frame performance
	return CVarNaniteAsyncRasterizeCustomPass.GetValueOnRenderThread() != 0 && !Lumen::UseAsyncCompute(ViewFamily);
}

struct FCompactedViewInfo
{
	uint32 StartOffset;
	uint32 NumValidViews;
};

BEGIN_SHADER_PARAMETER_STRUCT( FCullingParameters, )
	SHADER_PARAMETER( FIntVector4,	PageConstants )
	SHADER_PARAMETER( uint32,		MaxCandidateClusters )
	SHADER_PARAMETER( uint32,		MaxVisibleClusters )
	SHADER_PARAMETER( uint32,		RenderFlags )
	SHADER_PARAMETER( uint32,		DebugFlags )
	SHADER_PARAMETER( uint32,		NumViews )

	SHADER_PARAMETER( FVector2f,	HZBSize )

	SHADER_PARAMETER_RDG_TEXTURE( Texture2DArray,	HZBTextureArray )
	SHADER_PARAMETER_RDG_TEXTURE( Texture2D,	HZBTexture )
	SHADER_PARAMETER_SAMPLER( SamplerState,		HZBSampler )
	
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FPackedView >, InViews)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT( FVirtualTargetParameters, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FVirtualShadowMapUniformParameters, VirtualShadowMap )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, HZBPageTable )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint4 >,	HZBPageRectBounds )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, HZBPageFlags )
	SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >, OutDirtyPageFlags )
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT( FInstanceWorkGroupParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer< uint >, InInstanceWorkArgs)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FInstanceCullingGroupWork >, InInstanceWorkGroups)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FViewDrawGroup >, InViewDrawRanges)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, InstanceIds)
END_SHADER_PARAMETER_STRUCT()

inline bool IsValid(const FInstanceWorkGroupParameters &InstanceWorkGroupParameters)
{
	return InstanceWorkGroupParameters.InInstanceWorkArgs != nullptr;
}

class FRasterClearCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterClearCS);
	SHADER_USE_PARAMETER_STRUCT(FRasterClearCS, FNaniteGlobalShader);

	class FClearDepthDim : SHADER_PERMUTATION_BOOL("RASTER_CLEAR_DEPTH");
	class FClearDebugDim : SHADER_PERMUTATION_BOOL("RASTER_CLEAR_DEBUG");
	class FClearTiledDim : SHADER_PERMUTATION_BOOL("RASTER_CLEAR_TILED");
	using FPermutationDomain = TShaderPermutationDomain<FClearDepthDim, FClearDebugDim, FClearTiledDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FRasterParameters, RasterParameters)
		SHADER_PARAMETER(FUint32Vector4, ClearRect)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FRasterClearCS, "/Engine/Private/Nanite/NaniteRasterClear.usf", "RasterClear", SF_Compute);

class FPrimitiveFilter_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPrimitiveFilter_CS);
	SHADER_USE_PARAMETER_STRUCT(FPrimitiveFilter_CS, FNaniteGlobalShader);

	class FHiddenPrimitivesListDim : SHADER_PERMUTATION_BOOL("HAS_HIDDEN_PRIMITIVES_LIST");
	class FShowOnlyPrimitivesListDim : SHADER_PERMUTATION_BOOL("HAS_SHOW_ONLY_PRIMITIVES_LIST");

	using FPermutationDomain = TShaderPermutationDomain<FHiddenPrimitivesListDim, FShowOnlyPrimitivesListDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumPrimitives)
		SHADER_PARAMETER(uint32, HiddenFilterFlags)
		SHADER_PARAMETER(uint32, NumHiddenPrimitives)
		SHADER_PARAMETER(uint32, NumShowOnlyPrimitives)

		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, PrimitiveFilterBuffer)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, HiddenPrimitivesList)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ShowOnlyPrimitivesList)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FPrimitiveFilter_CS, "/Engine/Private/Nanite/NanitePrimitiveFilter.usf", "PrimitiveFilter", SF_Compute);

class FInstanceHierarchyCullShader : public FNaniteGlobalShader
{
public:
	FInstanceHierarchyCullShader() = default;
	FInstanceHierarchyCullShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FNaniteGlobalShader(Initializer)
	{
	}

	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FDebugFlagsDim : SHADER_PERMUTATION_BOOL( "DEBUG_FLAGS" );
	class FCullingPassDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_PASS", CULLING_PASS_NO_OCCLUSION, CULLING_PASS_OCCLUSION_MAIN, CULLING_PASS_OCCLUSION_POST);

	using FPermutationDomain = TShaderPermutationDomain<FDebugFlagsDim, FCullingPassDim, FVirtualTextureTargetDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );

		FVirtualShadowMapArray::SetShaderDefines( OutEnvironment );
		// The material cache does not use the hierarchy path (only instance and below) so fine to set to 0 here
		OutEnvironment.SetDefine( TEXT( "MATERIAL_CACHE" ), 0 );
		OutEnvironment.SetDefine( TEXT( "NANITE_MULTI_VIEW" ), 1 );
		OutEnvironment.SetDefine( TEXT( "DEPTH_ONLY" ), 1 );
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FCommonParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE( FInstanceHierarchyParameters, InstanceHierarchyParameters )
		SHADER_PARAMETER_STRUCT_INCLUDE( FCullingParameters, CullingParameters )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FSceneUniformParameters, Scene)
		SHADER_PARAMETER_STRUCT_INCLUDE( FVirtualTargetParameters, VirtualShadowMap )

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FViewDrawGroup >, InViewDrawRanges)

		SHADER_PARAMETER(uint32, MaxInstanceWorkGroups)
		SHADER_PARAMETER(uint32, bAllowStaticGeometryPath)
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FInstanceCullingGroupWork >, OutInstanceWorkGroups )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutInstanceWorkArgs )

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer<FNaniteStats>, OutStatsBuffer )

		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >, InOccludedChunkArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >, OutOccludedChunkArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FOccludedChunkDraw>, OutOccludedChunkDraws)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)

	END_SHADER_PARAMETER_STRUCT()
};

class FInstanceHierarchyCellChunkCull_CS : public FInstanceHierarchyCullShader
{
	DECLARE_GLOBAL_SHADER( FInstanceHierarchyCellChunkCull_CS );
	SHADER_USE_PARAMETER_STRUCT( FInstanceHierarchyCellChunkCull_CS, FInstanceHierarchyCullShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		// All post passes use the chunk cull
		if (PermutationVector.Get<FCullingPassDim>() == CULLING_PASS_OCCLUSION_POST)
		{
			return false;
		}

		return FInstanceHierarchyCullShader::ShouldCompilePermutation( Parameters );
	}

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FInstanceHierarchyCullShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		FGPUWorkGroupLoadBalancer::SetShaderDefines( OutEnvironment );
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE( FInstanceHierarchyCullShader::FCommonParameters, CommonParameters )
		SHADER_PARAMETER_STRUCT_INCLUDE( FSceneInstanceCullResult::FCellChunkDraws::FShaderParameters, CellChunkDraws )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInstanceHierarchyCellChunkCull_CS, "/Engine/Private/Nanite/NaniteInstanceHierarchyCulling.usf", "InstanceHierarchyCellChunkCull_CS", SF_Compute );

class FInstanceHierarchyChunkCull_CS : public FInstanceHierarchyCullShader
{
	DECLARE_GLOBAL_SHADER( FInstanceHierarchyChunkCull_CS );
	SHADER_USE_PARAMETER_STRUCT( FInstanceHierarchyChunkCull_CS, FInstanceHierarchyCullShader);

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE( FInstanceHierarchyCullShader::FCommonParameters, CommonParameters )
		SHADER_PARAMETER(uint32, NumGroupIds)
		SHADER_PARAMETER(uint32, NumAllocatedChunks)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, InGroupIds)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FOccludedChunkDraw>, InOccludedChunkDraws)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInstanceHierarchyChunkCull_CS, "/Engine/Private/Nanite/NaniteInstanceHierarchyCulling.usf", "InstanceHierarchyChunkCull_CS", SF_Compute );

class FInstanceHierarchyAppendUncullable_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInstanceHierarchyAppendUncullable_CS );
	SHADER_USE_PARAMETER_STRUCT( FInstanceHierarchyAppendUncullable_CS, FNaniteGlobalShader);

	class FDebugFlagsDim : SHADER_PERMUTATION_BOOL( "DEBUG_FLAGS" );
	using FPermutationDomain = TShaderPermutationDomain<FDebugFlagsDim>;

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );

		FVirtualShadowMapArray::SetShaderDefines( OutEnvironment );

		// These defines might be needed to make sure it compiles.
		OutEnvironment.SetDefine( TEXT( "NANITE_MULTI_VIEW" ), 1 );
		OutEnvironment.SetDefine( TEXT( "DEPTH_ONLY" ), 1 );
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE( FInstanceHierarchyParameters, InstanceHierarchyParameters )

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FViewDrawGroup >, InViewDrawRanges)
		SHADER_PARAMETER(uint32, NumViewDrawGroups)
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FInstanceCullingGroupWork >, OutInstanceWorkGroups )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutInstanceWorkArgs )
		SHADER_PARAMETER(uint32, MaxInstanceWorkGroups)
		SHADER_PARAMETER(uint32, bAllowStaticGeometryPath)
		SHADER_PARAMETER(uint32, UncullableItemChunksOffset)
		SHADER_PARAMETER(uint32, UncullableNumItemChunks)

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer<FNaniteStats>, OutStatsBuffer )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInstanceHierarchyAppendUncullable_CS, "/Engine/Private/Nanite/NaniteInstanceHierarchyCulling.usf", "AppendUncullableInstanceWork", SF_Compute );

class FInstanceHierarchySanitizeInstanceArgs_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInstanceHierarchySanitizeInstanceArgs_CS );
	SHADER_USE_PARAMETER_STRUCT( FInstanceHierarchySanitizeInstanceArgs_CS, FNaniteGlobalShader);

	class FDebugFlagsDim : SHADER_PERMUTATION_BOOL( "DEBUG_FLAGS" );
	using FPermutationDomain = TShaderPermutationDomain<FDebugFlagsDim>;

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		FVirtualShadowMapArray::SetShaderDefines( OutEnvironment );
		// These defines might be needed to make sure it compiles.
		OutEnvironment.SetDefine( TEXT( "DEPTH_ONLY" ), 1 );
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, InOutInstanceWorkArgs )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer<FNaniteStats>, OutStatsBuffer )
		SHADER_PARAMETER(uint32, MaxInstanceWorkGroups)
		SHADER_PARAMETER(uint32, GroupWorkArgsMaxCount)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInstanceHierarchySanitizeInstanceArgs_CS, "/Engine/Private/Nanite/NaniteInstanceHierarchyCulling.usf", "InstanceHierarchySanitizeInstanceArgsCS", SF_Compute );

class FInitInstanceHierarchyArgs_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInitInstanceHierarchyArgs_CS );
	SHADER_USE_PARAMETER_STRUCT( FInitInstanceHierarchyArgs_CS, FNaniteGlobalShader);

	class FOcclusionCullingDim : SHADER_PERMUTATION_BOOL( "OCCLUSION_CULLING" );
	using FPermutationDomain = TShaderPermutationDomain<FOcclusionCullingDim>;

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER(uint32, RenderFlags)

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FQueueState >,		OutQueueState )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FUintVector2 >,	InOutTotalPrevDrawClusters )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >,						InOutMainPassRasterizeArgsSWHW )
		
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutOccludedInstancesArgs )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutInstanceWorkArgs0)
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutInstanceWorkArgs1)
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, InOutPostPassRasterizeArgsSWHW )

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutOccludedChunkArgs)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitInstanceHierarchyArgs_CS, "/Engine/Private/Nanite/NaniteInstanceHierarchyCulling.usf", "InitArgs", SF_Compute);



class FInstanceCull_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInstanceCull_CS );
	SHADER_USE_PARAMETER_STRUCT( FInstanceCull_CS, FNaniteGlobalShader);

	class FCullingPassDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_PASS", CULLING_PASS_NO_OCCLUSION, CULLING_PASS_OCCLUSION_MAIN, CULLING_PASS_OCCLUSION_POST, CULLING_PASS_EXPLICIT_LIST);
	class FMultiViewDim : SHADER_PERMUTATION_BOOL("NANITE_MULTI_VIEW");
	class FPrimitiveFilterDim : SHADER_PERMUTATION_BOOL("PRIMITIVE_FILTER");
	class FDebugFlagsDim : SHADER_PERMUTATION_BOOL("DEBUG_FLAGS");
	class FDepthOnlyDim : SHADER_PERMUTATION_BOOL("DEPTH_ONLY");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FMaterialCacheDim : SHADER_PERMUTATION_BOOL("MATERIAL_CACHE");
	class FStaticGeoDim : SHADER_PERMUTATION_BOOL("STATIC_GEOMETRY_ONLY");
	class FUseGroupWorkBufferDim : SHADER_PERMUTATION_BOOL("INSTANCE_CULL_USE_WORK_GROUP_BUFFER"); // TODO: this permutation is mutually exclusive with NANITE_MULTI_VIEW, but need to be careful around what defines are set. )
	using FPermutationDomain = TShaderPermutationDomain<FCullingPassDim, FMultiViewDim, FPrimitiveFilterDim, FDebugFlagsDim, FDepthOnlyDim, FVirtualTextureTargetDim, FMaterialCacheDim, FStaticGeoDim, FUseGroupWorkBufferDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Only some platforms support native 64-bit atomics.
		if (!FDataDrivenShaderPlatformInfo::GetSupportsUInt64ImageAtomics(Parameters.Platform))
		{
			return false;
		}

		// Skip permutations targeting other culling passes, as they are covered in the specialized VSM instance cull, disable when FUseGroupWorkBufferDim, since that needs all choices 
		if (PermutationVector.Get<FVirtualTextureTargetDim>() 
			&& PermutationVector.Get<FCullingPassDim>() != CULLING_PASS_OCCLUSION_POST 
			&& !PermutationVector.Get<FUseGroupWorkBufferDim>())
		{
			return false;
		}

		// These are mutually exclusive
		if (PermutationVector.Get<FCullingPassDim>() == CULLING_PASS_EXPLICIT_LIST
			&& (PermutationVector.Get<FVirtualTextureTargetDim>() || PermutationVector.Get<FUseGroupWorkBufferDim>()))
		{
			return false;
		}

		// Only used together
		if (PermutationVector.Get<FStaticGeoDim>() && !PermutationVector.Get<FUseGroupWorkBufferDim>())
		{
			return false;
		}

		return FNaniteGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FDebugFlagsDim>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FVirtualShadowMapArray::SetShaderDefines( OutEnvironment );	// Still needed for shader to compile
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER( uint32, NumInstances )
		SHADER_PARAMETER( uint32, MaxNodes )
		SHADER_PARAMETER( int32,  ImposterMaxPixels )
		SHADER_PARAMETER( uint32, MaxInstanceWorkGroups )
		
		SHADER_PARAMETER_STRUCT_INCLUDE( FCullingParameters, CullingParameters )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FSceneUniformParameters, Scene )
		SHADER_PARAMETER_STRUCT_INCLUDE( FRasterParameters, RasterParameters )
		SHADER_PARAMETER_STRUCT_INCLUDE( FInstanceWorkGroupParameters, InstanceWorkGroupParameters )

		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, ImposterAtlas )
		
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FInstanceDraw >, InInstanceDraws )

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer, OutMainAndPostNodesAndClusterBatches )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FInstanceDraw >, OutOccludedInstances )

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FQueueState >, OutQueueState )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutOccludedInstancesArgs )

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteStats>, OutStatsBuffer)

		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >, InOccludedInstancesArgs )
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >, InPrimitiveFilterBuffer )

		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualTargetParameters, VirtualShadowMap)

		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)

	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInstanceCull_CS, "/Engine/Private/Nanite/NaniteInstanceCulling.usf", "InstanceCull", SF_Compute);


BEGIN_SHADER_PARAMETER_STRUCT(FNodeAndClusterCullSharedParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FCullingParameters, CullingParameters)
	SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualTargetParameters, VirtualShadowMap)

	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)

	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, HierarchyBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FUintVector2 >, InTotalPrevDrawClusters)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer< uint >, OffsetClustersArgsSWHW)

	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FQueueState >, QueueState)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, MainAndPostNodesAndClusterBatches)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, MainAndPostCandidateClusters)

	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutVisibleClustersSWHW)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FStreamingRequest>, OutStreamingRequests)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >, VisibleClustersArgsSWHW)

	SHADER_PARAMETER(uint32, MaxNodes)
	SHADER_PARAMETER(uint32, LargePageRectThreshold)
	SHADER_PARAMETER(uint32, StreamingRequestsBufferVersion)
	SHADER_PARAMETER(uint32, StreamingRequestsBufferSize)
	SHADER_PARAMETER(float, DepthBucketsMinZ)
	SHADER_PARAMETER(float, DepthBucketsMaxZ)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteStats>, OutStatsBuffer)
END_SHADER_PARAMETER_STRUCT()

class FNodeAndClusterCull_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FNodeAndClusterCull_CS );
	SHADER_USE_PARAMETER_STRUCT( FNodeAndClusterCull_CS, FNaniteGlobalShader );

	class FCullingPassDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_PASS", CULLING_PASS_NO_OCCLUSION, CULLING_PASS_OCCLUSION_MAIN, CULLING_PASS_OCCLUSION_POST);
	class FCullingTypeDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_TYPE", NANITE_CULLING_TYPE_NODES, NANITE_CULLING_TYPE_CLUSTERS, NANITE_CULLING_TYPE_PERSISTENT_NODES_AND_CLUSTERS);
	class FMultiViewDim : SHADER_PERMUTATION_BOOL("NANITE_MULTI_VIEW");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FMaterialCacheDim : SHADER_PERMUTATION_BOOL("MATERIAL_CACHE");
	class FDebugFlagsDim : SHADER_PERMUTATION_BOOL("DEBUG_FLAGS");
	class FSplineDeformDim : SHADER_PERMUTATION_BOOL("USE_SPLINEDEFORM");
	
	using FPermutationDomain = TShaderPermutationDomain<FCullingPassDim, FCullingTypeDim, FMultiViewDim, FVirtualTextureTargetDim, FMaterialCacheDim, FDebugFlagsDim, FSplineDeformDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_INCLUDE(FNodeAndClusterCullSharedParameters, SharedParameters)
		
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>,	CurrentNodeIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>,	NextNodeIndirectArgs)

		SHADER_PARAMETER(uint32,		NodeLevel)
		RDG_BUFFER_ACCESS(IndirectArgs,	ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!DoesPlatformSupportNanite(Parameters.Platform))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if( PermutationVector.Get<FVirtualTextureTargetDim>() &&
			!PermutationVector.Get<FMultiViewDim>() )
		{
			return false;
		}

		if (PermutationVector.Get<FSplineDeformDim>() && !NaniteSplineMeshesSupported())
		{
			return false;
		}

		return FNaniteGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FDebugFlagsDim>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		int CullingType = PermutationVector.Get<FCullingTypeDim>();
		int PersistentThreadsCulling = CVarNanitePersistentThreadsCulling.GetValueOnAnyThread();
		if (PersistentThreadsCulling > 0)
		{
			if (CullingType != NANITE_CULLING_TYPE_PERSISTENT_NODES_AND_CLUSTERS)
			{
				return EShaderPermutationPrecacheRequest::NotUsed;
			}
		}
		else if (CullingType == NANITE_CULLING_TYPE_PERSISTENT_NODES_AND_CLUSTERS)
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("NANITE_HIERARCHY_TRAVERSAL"), 1);

		// The routing requires access to page table data structures, only for 'VIRTUAL_TEXTURE_TARGET' really...
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FNodeAndClusterCull_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "NodeAndClusterCull", SF_Compute);

class FInitClusterBatches_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInitClusterBatches_CS );
	SHADER_USE_PARAMETER_STRUCT( FInitClusterBatches_CS, FNaniteGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer,	OutMainAndPostNodesAndClusterBatches )
		SHADER_PARAMETER( uint32,								MaxCandidateClusters )
		SHADER_PARAMETER( uint32,								MaxNodes )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitClusterBatches_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "InitClusterBatches", SF_Compute);

class FInitCandidateNodes_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInitCandidateNodes_CS );
	SHADER_USE_PARAMETER_STRUCT( FInitCandidateNodes_CS, FNaniteGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer,	OutMainAndPostNodesAndClusterBatches )
		SHADER_PARAMETER( uint32,								MaxCandidateClusters )
		SHADER_PARAMETER( uint32,								MaxNodes )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitCandidateNodes_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "InitCandidateNodes", SF_Compute);

class FInitArgs_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInitArgs_CS );
	SHADER_USE_PARAMETER_STRUCT( FInitArgs_CS, FNaniteGlobalShader);

	class FOcclusionCullingDim : SHADER_PERMUTATION_BOOL( "OCCLUSION_CULLING" );
	class FDrawPassIndexDim : SHADER_PERMUTATION_INT( "DRAW_PASS_INDEX", 3 );	// 0: no, 1: set, 2: add
	using FPermutationDomain = TShaderPermutationDomain<FOcclusionCullingDim, FDrawPassIndexDim>;

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER(uint32, RenderFlags)

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FQueueState >,		OutQueueState )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FUintVector2 >,	InOutTotalPrevDrawClusters )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >,						InOutMainPassRasterizeArgsSWHW )
		
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutOccludedInstancesArgs )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, InOutPostPassRasterizeArgsSWHW )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitArgs_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "InitArgs", SF_Compute);

class FInitClusterCullArgs_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitClusterCullArgs_CS);
	SHADER_USE_PARAMETER_STRUCT(FInitClusterCullArgs_CS, FNaniteGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FQueueState >,	OutQueueState)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >,					OutClusterCullArgs)
		SHADER_PARAMETER(uint32,											MaxCandidateClusters)
		SHADER_PARAMETER(uint32,											InitIsPostPass)
		END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitClusterCullArgs_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "InitClusterCullArgs", SF_Compute);

class FInitNodeCullArgs_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitNodeCullArgs_CS);
	SHADER_USE_PARAMETER_STRUCT(FInitNodeCullArgs_CS, FNaniteGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FQueueState >,	OutQueueState)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >,					OutNodeCullArgs0)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >,					OutNodeCullArgs1)
		SHADER_PARAMETER(uint32,											MaxNodes)
		SHADER_PARAMETER(uint32,											InitIsPostPass)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitNodeCullArgs_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "InitNodeCullArgs", SF_Compute);


class FCalculateSafeRasterizerArgs_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateSafeRasterizerArgs_CS);
	SHADER_USE_PARAMETER_STRUCT(FCalculateSafeRasterizerArgs_CS, FNaniteGlobalShader);

	class FIsPostPass : SHADER_PERMUTATION_BOOL("IS_POST_PASS");
	using FPermutationDomain = TShaderPermutationDomain<FIsPostPass>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FUintVector2 >,	InTotalPrevDrawClusters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer< uint >,						OffsetClustersArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer< uint >,						InRasterizerArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >,					OutSafeRasterizerArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FUintVector2 >,	OutClusterCountSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >,					OutClusterClassifyArgs)

		SHADER_PARAMETER(uint32,											MaxVisibleClusters)
		SHADER_PARAMETER(uint32,											RenderFlags)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCalculateSafeRasterizerArgs_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "CalculateSafeRasterizerArgs", SF_Compute);

BEGIN_SHADER_PARAMETER_STRUCT(FGlobalWorkQueueParameters,)
	SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer, DataBuffer )
	SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FWorkQueueState >, StateBuffer )
END_SHADER_PARAMETER_STRUCT()

class FInitVisiblePatchesArgsCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInitVisiblePatchesArgsCS );
	SHADER_USE_PARAMETER_STRUCT( FInitVisiblePatchesArgsCS, FNaniteGlobalShader );

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, RWVisiblePatchesArgs )
		SHADER_PARAMETER( uint32, MaxVisiblePatches )
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FNaniteGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NANITE_TESSELLATION"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FInitVisiblePatchesArgsCS, "/Engine/Private/Nanite/NaniteRasterBinning.usf", "InitVisiblePatchesArgs", SF_Compute);

class FRasterBinBuild_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterBinBuild_CS);
	SHADER_USE_PARAMETER_STRUCT(FRasterBinBuild_CS, FNaniteGlobalShader);

	class FIsPostPass : SHADER_PERMUTATION_BOOL("IS_POST_PASS");
	class FPatches : SHADER_PERMUTATION_BOOL("PATCHES");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FMaterialCacheDim : SHADER_PERMUTATION_BOOL("MATERIAL_CACHE");
	class FBuildPassDim : SHADER_PERMUTATION_SPARSE_INT("RASTER_BIN_PASS", NANITE_RASTER_BIN_COUNT, NANITE_RASTER_BIN_SCATTER);
	class FDepthBucketingDim : SHADER_PERMUTATION_BOOL("DEPTH_BUCKETING");

	using FPermutationDomain = TShaderPermutationDomain<FIsPostPass, FPatches, FVirtualTextureTargetDim, FMaterialCacheDim, FBuildPassDim, FDepthBucketingDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FSceneUniformParameters, Scene )

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteRasterBinMeta>,	OutRasterBinMeta)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>,								OutRasterBinArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUintVector2>,			OutRasterBinData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>,					OutDepthBuckets)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FUintVector2>,	InTotalPrevDrawClusters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FUintVector2>,	InClusterCountSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>,					InClusterOffsetSWHW)

		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)

		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,	VisiblePatches )
		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >,	VisiblePatchesArgs )
		SHADER_PARAMETER_STRUCT( FGlobalWorkQueueParameters, SplitWorkQueue )

		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)

		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER(uint32, RenderFlags)
		SHADER_PARAMETER(uint32, MaxVisibleClusters)
		SHADER_PARAMETER(uint32, RegularMaterialRasterBinCount)
		SHADER_PARAMETER(uint32, bUsePrimOrMeshShader)
		SHADER_PARAMETER(uint32, MaxPatchesPerGroup)
		SHADER_PARAMETER(uint32, MeshPassIndex)
		SHADER_PARAMETER(uint32, MinSupportedWaveSize)
		SHADER_PARAMETER(uint32, MaxVisiblePatches)
		SHADER_PARAMETER(uint32, MaxClusterIndirections)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
	#if !NANITE_DEPTH_BUCKETING
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FDepthBucketingDim>() )
		{
			return false;
		}
	#endif

		return FNaniteGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const bool bForceBatching = FDataDrivenShaderPlatformInfo::GetMaxMeshShaderThreadGroupSize(Parameters.Platform) == 32u;
		OutEnvironment.SetDefine(TEXT("FORCE_BATCHING"), bForceBatching ? 1 : 0);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRasterBinBuild_CS, "/Engine/Private/Nanite/NaniteRasterBinning.usf", "RasterBinBuild", SF_Compute);

class FRasterBinInit_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterBinInit_CS);
	SHADER_USE_PARAMETER_STRUCT(FRasterBinInit_CS, FNaniteGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteRasterBinMeta>, OutRasterBinMeta)

		SHADER_PARAMETER(uint32, RasterBinCount)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RASTER_BIN_PASS"), NANITE_RASTER_BIN_INIT);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRasterBinInit_CS, "/Engine/Private/Nanite/NaniteRasterBinning.usf", "RasterBinInit", SF_Compute);

class FRasterBinReserve_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterBinReserve_CS);
	SHADER_USE_PARAMETER_STRUCT(FRasterBinReserve_CS, FNaniteGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutRangeAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutRasterBinArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteRasterBinMeta>, OutRasterBinMeta)

		SHADER_PARAMETER(uint32, RasterBinCount)
		SHADER_PARAMETER(uint32, RenderFlags)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RASTER_BIN_PASS"), NANITE_RASTER_BIN_RESERVE);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRasterBinReserve_CS, "/Engine/Private/Nanite/NaniteRasterBinning.usf", "RasterBinReserve", SF_Compute);

class FRasterBinDepthBlock_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterBinDepthBlock_CS);
	SHADER_USE_PARAMETER_STRUCT(FRasterBinDepthBlock_CS, FNaniteGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutDepthBuckets)
	END_SHADER_PARAMETER_STRUCT()

#if !NANITE_DEPTH_BUCKETING
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return false;
	}
#endif

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RASTER_BIN_PASS"), NANITE_RASTER_BIN_DEPTHBLOCK);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRasterBinDepthBlock_CS, "/Engine/Private/Nanite/NaniteRasterBinning.usf", "RasterBinDepthBlock", SF_Compute);

class FRasterBinFinalize_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterBinFinalize_CS);
	SHADER_USE_PARAMETER_STRUCT(FRasterBinFinalize_CS, FNaniteGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutRasterBinArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteRasterBinMeta>, OutRasterBinMeta)

		SHADER_PARAMETER(uint32, RasterBinCount)
		SHADER_PARAMETER(uint32, FinalizeMode)
		SHADER_PARAMETER(uint32, RenderFlags)
		SHADER_PARAMETER(uint32, MaxClusterIndirections)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RASTER_BIN_PASS"), NANITE_RASTER_BIN_FINALIZE);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRasterBinFinalize_CS, "/Engine/Private/Nanite/NaniteRasterBinning.usf", "RasterBinFinalize", SF_Compute);

class FInitPatchSplitArgs_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInitPatchSplitArgs_CS );
	SHADER_USE_PARAMETER_STRUCT( FInitPatchSplitArgs_CS, FNaniteGlobalShader );

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FNaniteRasterUniformParameters, NaniteRaster )
		SHADER_PARAMETER_STRUCT( FGlobalWorkQueueParameters, SplitWorkQueue )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutPatchSplitArgs0 )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutPatchSplitArgs1 )
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("NANITE_TESSELLATION"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FInitPatchSplitArgs_CS, "/Engine/Private/Nanite/NaniteSplit.usf", "InitPatchSplitArgs", SF_Compute);

class FPatchSplitCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FPatchSplitCS );
	SHADER_USE_PARAMETER_STRUCT( FPatchSplitCS, FNaniteGlobalShader);

	class FCullingPassDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_PASS", CULLING_PASS_NO_OCCLUSION, CULLING_PASS_OCCLUSION_MAIN, CULLING_PASS_OCCLUSION_POST);
	class FMultiViewDim : SHADER_PERMUTATION_BOOL("NANITE_MULTI_VIEW");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FMaterialCacheDim : SHADER_PERMUTATION_BOOL("MATERIAL_CACHE");
	class FSplineDeformDim : SHADER_PERMUTATION_BOOL("USE_SPLINEDEFORM");
	class FSkinningDim : SHADER_PERMUTATION_BOOL("USE_SKINNING");
	class FWriteStatsDim : SHADER_PERMUTATION_BOOL("WRITE_STATS");
	using FPermutationDomain = TShaderPermutationDomain< FCullingPassDim, FMultiViewDim, FVirtualTextureTargetDim, FMaterialCacheDim, FSplineDeformDim, FSkinningDim, FWriteStatsDim>;

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FSceneUniformParameters, Scene )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRasterUniformParameters, NaniteRaster)

		SHADER_PARAMETER_STRUCT( FGlobalWorkQueueParameters, SplitWorkQueue )
		SHADER_PARAMETER_STRUCT( FGlobalWorkQueueParameters, OccludedPatches )

		SHADER_PARAMETER_STRUCT_INCLUDE( FCullingParameters, CullingParameters )
		SHADER_PARAMETER_STRUCT_INCLUDE( FVirtualTargetParameters, VirtualShadowMap )

		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, ClusterPageData )
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, HierarchyBuffer)

		SHADER_PARAMETER_SRV( ByteAddressBuffer,	TessellationTable_Offsets )
		SHADER_PARAMETER_SRV( ByteAddressBuffer,	TessellationTable_VertsAndIndexes )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,		VisibleClustersSWHW )

		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >,		InClusterOffsetSWHW )

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteStats>, OutStatsBuffer)

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer,	RWVisiblePatches )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >,		RWVisiblePatchesArgs )
		SHADER_PARAMETER( uint32,								VisiblePatchesSize )

		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >,	CurrentIndirectArgs )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >,	NextIndirectArgs )

		SHADER_PARAMETER( uint32,			Level )
		RDG_BUFFER_ACCESS( IndirectArgs,	ERHIAccess::IndirectArgs )
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FVirtualTextureTargetDim>() && !PermutationVector.Get<FMultiViewDim>())
		{
			return false;
		}

		if (PermutationVector.Get<FSplineDeformDim>() && !NaniteSplineMeshesSupported())
		{
			return false;
		}

		if (PermutationVector.Get<FSkinningDim>() && !NaniteSkinnedMeshesSupported())
		{
			return false;
		}
		
		return FNaniteGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("NANITE_TESSELLATION"), 1);
		OutEnvironment.SetDefine(TEXT("NANITE_USE_RASTER_UNIFORM_BUFFER"), 1);
		OutEnvironment.SetDefine(TEXT("PATCHSPLIT_PASS"), 1);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);

		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FPatchSplitCS, "/Engine/Private/Nanite/NaniteSplit.usf", "PatchSplit", SF_Compute);

BEGIN_SHADER_PARAMETER_STRUCT( FRasterizePassParameters, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FSceneUniformParameters, Scene )
	SHADER_PARAMETER_STRUCT_INCLUDE( FRasterParameters, RasterParameters )

	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRasterUniformParameters, NaniteRaster)

	SHADER_PARAMETER(FUintVector4, PassData)

	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, ClusterPageData )
	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, HierarchyBuffer )

	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FPackedView >,	InViews )
	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,					VisibleClustersSWHW )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FUintVector2 >,	InTotalPrevDrawClusters )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer<uint>,			RasterBinData )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer<FNaniteRasterBinMeta>,	RasterBinMeta )

	SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >, InClusterOffsetSWHW )
	
	SHADER_PARAMETER_SRV( ByteAddressBuffer,	TessellationTable_Offsets )
	SHADER_PARAMETER_SRV( ByteAddressBuffer,	TessellationTable_VertsAndIndexes )

	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,	VisiblePatches )
	SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >,	VisiblePatchesArgs )
	
	SHADER_PARAMETER_STRUCT( FGlobalWorkQueueParameters, SplitWorkQueue )

	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteStats>, OutStatsBuffer)

	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)

	SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualTargetParameters, VirtualShadowMap)
END_SHADER_PARAMETER_STRUCT()

static uint32 PackMaterialBitFlags(
	const FMaterial& RasterMaterial,
	const FNaniteRasterPipeline& RasterPipeline,
	bool bMaterialHasProgrammableVertexUVs,
	bool bMaterialUsesWorldPositionOffset,
	bool bMaterialUsesPixelDepthOffset,
	bool bMaterialUsesDisplacement)
{
	FNaniteMaterialFlags Flags = {0};
	Flags.bPixelDiscard			= RasterPipeline.bPerPixelEval && RasterMaterial.IsMasked();
	Flags.bPixelDepthOffset		= RasterPipeline.bPerPixelEval && bMaterialUsesPixelDepthOffset;
	Flags.bWorldPositionOffset	= RasterPipeline.bWPOEnabled && bMaterialUsesWorldPositionOffset;
	Flags.bDisplacement			= UseNaniteTessellation() && RasterPipeline.bDisplacementEnabled && bMaterialUsesDisplacement;
	Flags.bSplineMesh			= RasterPipeline.bSplineMesh;
	Flags.bSkinnedMesh			= RasterPipeline.bSkinnedMesh;
	Flags.bTwoSided				= RasterPipeline.bIsTwoSided;
	Flags.bCastShadow			= RasterPipeline.bCastShadow;

	const bool bPixelProgrammable = IsNaniteMaterialPixelProgrammable(Flags);
	Flags.bVertexUVs = bMaterialHasProgrammableVertexUVs && bPixelProgrammable;

	return PackNaniteMaterialBitFlags(Flags);
}

static uint32 PackMaterialBitFlags_GameThread(const FMaterial& RasterMaterial, const FNaniteRasterPipeline& RasterPipeline)
{
	const bool bProgrammableVertexUVs = RasterMaterial.HasVertexInterpolator() || RasterMaterial.GetNumCustomizedUVs() > 0;

	return PackMaterialBitFlags(
		RasterMaterial,
		RasterPipeline,
		bProgrammableVertexUVs,
		RasterMaterial.MaterialUsesWorldPositionOffset_GameThread(),
		RasterMaterial.MaterialUsesPixelDepthOffset_GameThread(),
		RasterMaterial.MaterialUsesDisplacement_GameThread());
}

static uint32 PackMaterialBitFlags_RenderThread(const FMaterial& RasterMaterial, const FNaniteRasterPipeline& RasterPipeline)
{
	const bool bProgrammableVertexUVs = RasterMaterial.HasVertexInterpolator() || RasterMaterial.GetNumCustomizedUVs() > 0;

	return PackMaterialBitFlags(
		RasterMaterial,
		RasterPipeline,
		bProgrammableVertexUVs,
		RasterMaterial.MaterialUsesWorldPositionOffset_RenderThread(),
		RasterMaterial.MaterialUsesPixelDepthOffset_RenderThread(),
		RasterMaterial.MaterialUsesDisplacement_RenderThread());
}

class FMicropolyRasterizeCS : public FNaniteMaterialShader
{
	DECLARE_SHADER_TYPE(FMicropolyRasterizeCS, Material);
	SHADER_USE_PARAMETER_STRUCT_MIXED(FMicropolyRasterizeCS, FNaniteMaterialShader);

	class FDepthOnlyDim : SHADER_PERMUTATION_BOOL("DEPTH_ONLY");
	class FTwoSidedDim : SHADER_PERMUTATION_BOOL("NANITE_TWO_SIDED");
	class FVisualizeDim : SHADER_PERMUTATION_BOOL("VISUALIZE");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FMaterialCacheDim : SHADER_PERMUTATION_BOOL("MATERIAL_CACHE");
	class FVertexProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_VERTEX_PROGRAMMABLE");
	class FPixelProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_PIXEL_PROGRAMMABLE");
	class FTessellationDim : SHADER_PERMUTATION_BOOL("NANITE_TESSELLATION");
	class FPatchesDim : SHADER_PERMUTATION_BOOL("PATCHES");
	class FVoxelsDim : SHADER_PERMUTATION_BOOL("NANITE_VOXELS");
	class FSplineDeformDim : SHADER_PERMUTATION_BOOL("USE_SPLINEDEFORM");
	class FSkinningDim : SHADER_PERMUTATION_BOOL("USE_SKINNING");
	class FFixedDisplacementFallbackDim : SHADER_PERMUTATION_BOOL("FIXED_DISPLACEMENT_FALLBACK");
	
	using FPermutationDomain = TShaderPermutationDomain<
		FDepthOnlyDim,
		FTwoSidedDim,
		FVisualizeDim,
		FVirtualTextureTargetDim,
		FMaterialCacheDim,
		FVertexProgrammableDim,
		FPixelProgrammableDim,
		FTessellationDim,
		FPatchesDim,
		FVoxelsDim,
		FSplineDeformDim,
		FSkinningDim,
		FFixedDisplacementFallbackDim
	>;

	using FParameters = FRasterizePassParameters;

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		if (!DoesPlatformSupportNanite(Parameters.Platform))
		{
			return false;
		}
		
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		
		// Only some platforms support native 64-bit atomics.
		if (!FDataDrivenShaderPlatformInfo::GetSupportsUInt64ImageAtomics(Parameters.Platform))
		{
			return false;
		}

		if (PermutationVector.Get<FVisualizeDim>() &&
			(PermutationVector.Get<FDepthOnlyDim>() && !PermutationVector.Get<FVirtualTextureTargetDim>()))
		{
			// Visualization not supported with standard depth only, but is with VSM
			return false;
		}

		if (!Parameters.MaterialParameters.bIsDefaultMaterial && PermutationVector.Get<FTwoSidedDim>() != Parameters.MaterialParameters.bIsTwoSided)
		{
			return false;
		}

		if (PermutationVector.Get<FVirtualTextureTargetDim>() && !PermutationVector.Get<FDepthOnlyDim>())
		{
			return false;
		}

		if (!ShouldCompileProgrammablePermutation(
				Parameters.MaterialParameters,
				PermutationVector.Get<FVertexProgrammableDim>(),
				PermutationVector.Get<FPixelProgrammableDim>(),
				/* bHWRasterShader */ false
			))
		{
			return false;
		}

		if (PermutationVector.Get<FTessellationDim>() || PermutationVector.Get<FPatchesDim>())
		{
			// TODO Don't compile useless shaders for default material
			if (!Parameters.MaterialParameters.bIsDefaultMaterial && !Parameters.MaterialParameters.bIsTessellationEnabled)
			{
				return false;
			}
		}

		if (PermutationVector.Get<FTessellationDim>() && !PermutationVector.Get<FVertexProgrammableDim>())
		{
			// Tessellation implies vertex programmable (see FNaniteMaterialShader::IsVertexProgrammable)
			return false;
		}

		if (PermutationVector.Get<FVoxelsDim>())
		{
		#if NANITE_VOXEL_DATA
			if (!Parameters.MaterialParameters.bIsDefaultMaterial || PermutationVector.Get<FTwoSidedDim>() || PermutationVector.Get<FSplineDeformDim>() )
			{
				return false;
			}
		#else
			return false;
		#endif
		}

		if (PermutationVector.Get<FSplineDeformDim>())
		{
			if (!NaniteSplineMeshesSupported() || (!Parameters.MaterialParameters.bIsDefaultMaterial && !Parameters.MaterialParameters.bIsUsedWithSplineMeshes))
			{
				return false;
			}
		}

		if (PermutationVector.Get<FSkinningDim>())
		{
			if (!NaniteSkinnedMeshesSupported() || (!Parameters.MaterialParameters.bIsDefaultMaterial && !Parameters.MaterialParameters.bIsUsedWithSkeletalMesh))
			{
				return false;
			}

			if (PermutationVector.Get<FSplineDeformDim>())
			{
				// Mutually exclusive
				return false;
			}
		}

		if (PermutationVector.Get<FFixedDisplacementFallbackDim>())
		{
			// This permutation is ONLY applicable to the default material with no programmable features
			if (!Parameters.MaterialParameters.bIsDefaultMaterial ||
				PermutationVector.Get<FVertexProgrammableDim>() ||
				PermutationVector.Get<FPixelProgrammableDim>() ||
				PermutationVector.Get<FTessellationDim>() ||
				PermutationVector.Get<FPatchesDim>())
			{
				return false;
			}
		}

		return FNaniteMaterialShader::ShouldCompileComputePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FNaniteMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("SOFTWARE_RASTER"), 1);
		OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 1);
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);

		if (PermutationVector.Get<FPixelProgrammableDim>() || PermutationVector.Get<FTessellationDim>())
		{
			OutEnvironment.SetDefine(TEXT("NANITE_VERT_REUSE_BATCH"), 1);
		}

		if (PermutationVector.Get<FPixelProgrammableDim>() || PermutationVector.Get<FTessellationDim>() || PermutationVector.Get<FVoxelsDim>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		}

		if (PermutationVector.Get<FTessellationDim>())
		{
			OutEnvironment.SetDefine(TEXT("VIRTUAL_TEXTURE_FORCE_BILINEAR_FILTERING"), 1);
		}

		OutEnvironment.CompilerFlags.Add(CFLAG_CheckForDerivativeOps);

		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FMaterialShader::SetViewParameters(BatchedParameters, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(BatchedParameters, MaterialProxy, Material, View);
	}
};
IMPLEMENT_MATERIAL_SHADER_TYPE(, FMicropolyRasterizeCS, TEXT("/Engine/Private/Nanite/NaniteRasterizer.usf"), TEXT("MicropolyRasterize"), SF_Compute);

class FMicropolyRasterizeWG : public FMicropolyRasterizeCS
{
public:
	DECLARE_SHADER_TYPE(FMicropolyRasterizeWG, Material);

	FMicropolyRasterizeWG() = default;
	FMicropolyRasterizeWG(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMicropolyRasterizeCS(Initializer)
	{
	}
	
	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return NaniteWorkGraphMaterialsSupported() && RHISupportsWorkGraphs(Parameters.Platform) && FMicropolyRasterizeCS::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMicropolyRasterizeCS::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("WORKGRAPH_NODE"), 1);
	}
};
IMPLEMENT_MATERIAL_SHADER_TYPE(, FMicropolyRasterizeWG, TEXT("/Engine/Private/Nanite/NaniteRasterizer.usf"), TEXT("MicropolyRasterize"), SF_WorkGraphComputeNode);

static TShaderRef<FMicropolyRasterizeCS> GetMicropolyRasterizeShader(
	FMaterialShaderMap const* InShaderMap,
	FMicropolyRasterizeCS::FPermutationDomain& InPermutationVector,
	EShaderFrequency InShaderFrequency)
{
	if (InShaderFrequency == SF_WorkGraphComputeNode)
	{
		return InShaderMap->GetShader<FMicropolyRasterizeWG>(InPermutationVector);
	}
	
	return InShaderMap->GetShader<FMicropolyRasterizeCS>(InPermutationVector);
}

template<typename TShaderType, typename... TArguments>
static inline void SetShaderBundleParameters(FRHIBatchedShaderParameters& BatchedParameters, const TShaderRef<TShaderType>& InShader, const typename TShaderType::FParameters& Parameters, EShaderFrequency Frequency, TArguments&&... InArguments)
{
	SetBatchedShaderParametersMixed(BatchedParameters, InShader, Parameters, Forward<TArguments>(InArguments)...);
}

class FHWRasterizeVS : public FNaniteMaterialShader
{
	DECLARE_SHADER_TYPE(FHWRasterizeVS, Material);
	SHADER_USE_PARAMETER_STRUCT_MIXED(FHWRasterizeVS, FNaniteMaterialShader);

	class FDepthOnlyDim : SHADER_PERMUTATION_BOOL("DEPTH_ONLY");
	class FPrimShaderDim : SHADER_PERMUTATION_BOOL("NANITE_PRIM_SHADER");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FMaterialCacheDim : SHADER_PERMUTATION_BOOL("MATERIAL_CACHE");
	class FVertexProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_VERTEX_PROGRAMMABLE");
	class FPixelProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_PIXEL_PROGRAMMABLE");
	class FSplineDeformDim : SHADER_PERMUTATION_BOOL("USE_SPLINEDEFORM");
	class FSkinningDim : SHADER_PERMUTATION_BOOL("USE_SKINNING");
	class FFixedDisplacementFallbackDim : SHADER_PERMUTATION_BOOL("FIXED_DISPLACEMENT_FALLBACK");

	using FPermutationDomain = TShaderPermutationDomain<
		FDepthOnlyDim,
		FPrimShaderDim,
		FVirtualTextureTargetDim,
		FMaterialCacheDim,
		FVertexProgrammableDim,
		FPixelProgrammableDim,
		FSplineDeformDim,
		FSkinningDim,
		FFixedDisplacementFallbackDim
	>;

	using FParameters = FRasterizePassParameters;

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		
		// Only some platforms support native 64-bit atomics.
		if (!FDataDrivenShaderPlatformInfo::GetSupportsUInt64ImageAtomics(Parameters.Platform))
		{
			return false;
		}

		if (PermutationVector.Get<FPrimShaderDim>() && !FDataDrivenShaderPlatformInfo::GetSupportsPrimitiveShaders(Parameters.Platform))
		{
			// Only some platforms support primitive shaders.
			return false;
		}

		// VSM rendering is depth-only and multiview
		if (PermutationVector.Get<FVirtualTextureTargetDim>() && !PermutationVector.Get<FDepthOnlyDim>())
		{
			return false;
		}

		if (PermutationVector.Get<FSplineDeformDim>())
		{
			if (!NaniteSplineMeshesSupported() || (!Parameters.MaterialParameters.bIsDefaultMaterial && !Parameters.MaterialParameters.bIsUsedWithSplineMeshes))
			{
				return false;
			}
		}

		if (PermutationVector.Get<FSkinningDim>())
		{
			if (!NaniteSkinnedMeshesSupported() || (!Parameters.MaterialParameters.bIsDefaultMaterial && !Parameters.MaterialParameters.bIsUsedWithSkeletalMesh))
			{
				return false;
			}

			if (PermutationVector.Get<FSplineDeformDim>())
			{
				// Mutually exclusive
				return false;
			}
		}

		if (PermutationVector.Get<FFixedDisplacementFallbackDim>())
		{
			// This permutation is ONLY applicable to the default material with no programmable features
			if (!Parameters.MaterialParameters.bIsDefaultMaterial ||
				PermutationVector.Get<FVertexProgrammableDim>() ||
				PermutationVector.Get<FPixelProgrammableDim>())
			{
				return false;
			}
		}

		if (!ShouldCompileProgrammablePermutation(
				Parameters.MaterialParameters,
				PermutationVector.Get<FVertexProgrammableDim>(),
				PermutationVector.Get<FPixelProgrammableDim>(),
				/* bHWRasterShader */ true
			))
		{
			return false;
		}

		return FNaniteMaterialShader::ShouldCompileVertexPermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FNaniteMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);

		OutEnvironment.SetDefine(TEXT("SOFTWARE_RASTER"), 0);
		OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 0);
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);
		OutEnvironment.SetDefine(TEXT("NANITE_ALLOW_SV_BARYCENTRICS"), 0);

		const bool bIsPrimitiveShader = PermutationVector.Get<FPrimShaderDim>();
		
		if (bIsPrimitiveShader)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_VertexToPrimitiveShader);

			if (PermutationVector.Get<FVertexProgrammableDim>())
			{
				OutEnvironment.SetDefine(TEXT("NANITE_VERT_REUSE_BATCH"), 1);
				OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
			}
		}

		OutEnvironment.SetDefine(TEXT("NANITE_HW_COUNTER_INDEX"), bIsPrimitiveShader ? 4 : 5); // Mesh and primitive shaders use an index of 4 instead of 5

		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FMaterialShader::SetViewParameters(BatchedParameters, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(BatchedParameters, MaterialProxy, Material, View);
	}
};
IMPLEMENT_MATERIAL_SHADER_TYPE(, FHWRasterizeVS, TEXT("/Engine/Private/Nanite/NaniteRasterizer.usf"), TEXT("HWRasterizeVS"), SF_Vertex);

// TODO: Consider making a common base shader class for VS and MS (where possible)
class FHWRasterizeMS : public FNaniteMaterialShader
{
	DECLARE_SHADER_TYPE(FHWRasterizeMS, Material);
	SHADER_USE_PARAMETER_STRUCT_MIXED(FHWRasterizeMS, FNaniteMaterialShader);

	class FDepthOnlyDim : SHADER_PERMUTATION_BOOL("DEPTH_ONLY");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FMaterialCacheDim : SHADER_PERMUTATION_BOOL("MATERIAL_CACHE");
	class FVertexProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_VERTEX_PROGRAMMABLE");
	class FPixelProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_PIXEL_PROGRAMMABLE");
	class FSplineDeformDim : SHADER_PERMUTATION_BOOL("USE_SPLINEDEFORM");
	class FSkinningDim : SHADER_PERMUTATION_BOOL("USE_SKINNING");
	class FAllowSvBarycentricsDim : SHADER_PERMUTATION_BOOL("NANITE_ALLOW_SV_BARYCENTRICS");
	class FFixedDisplacementFallbackDim : SHADER_PERMUTATION_BOOL("FIXED_DISPLACEMENT_FALLBACK");

	using FPermutationDomain = TShaderPermutationDomain
	<
		FDepthOnlyDim,
		FVirtualTextureTargetDim,
		FMaterialCacheDim,
		FVertexProgrammableDim,
		FPixelProgrammableDim,
		FSplineDeformDim,
		FSkinningDim,
		FAllowSvBarycentricsDim,
		FFixedDisplacementFallbackDim
	>;

	using FParameters = FRasterizePassParameters;

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		if (!FDataDrivenShaderPlatformInfo::GetSupportsMeshShadersTier1(Parameters.Platform))
		{
			// Only some platforms support mesh shaders with tier1 support
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Only some platforms support native 64-bit atomics.
		if (!FDataDrivenShaderPlatformInfo::GetSupportsUInt64ImageAtomics(Parameters.Platform))
		{
			return false;
		}

		// VSM rendering is depth-only and multiview
		if (PermutationVector.Get<FVirtualTextureTargetDim>() && !PermutationVector.Get<FDepthOnlyDim>())
		{
			return false;
		}

		if (PermutationVector.Get<FSplineDeformDim>())
		{
			if (!NaniteSplineMeshesSupported() || (!Parameters.MaterialParameters.bIsDefaultMaterial && !Parameters.MaterialParameters.bIsUsedWithSplineMeshes))
			{
				return false;
			}
		}

		if (PermutationVector.Get<FSkinningDim>())
		{
			if (!NaniteSkinnedMeshesSupported() || (!Parameters.MaterialParameters.bIsDefaultMaterial && !Parameters.MaterialParameters.bIsUsedWithSkeletalMesh))
			{
				return false;
			}

			if (PermutationVector.Get<FSplineDeformDim>())
			{
				// Mutually exclusive
				return false;
			}
		}

		if (PermutationVector.Get<FFixedDisplacementFallbackDim>())
		{
			// This permutation is ONLY applicable to the default material with no programmable features
			if (!Parameters.MaterialParameters.bIsDefaultMaterial ||
				PermutationVector.Get<FVertexProgrammableDim>() ||
				PermutationVector.Get<FPixelProgrammableDim>())
			{
				return false;
			}
		}

		if (!ShouldCompileSvBarycentricPermutation(Parameters.Platform, PermutationVector.Get<FPixelProgrammableDim>(), true, PermutationVector.Get<FAllowSvBarycentricsDim>()))
		{
			return false;
		}

		if (!ShouldCompileProgrammablePermutation(
				Parameters.MaterialParameters,
				PermutationVector.Get<FVertexProgrammableDim>(),
				PermutationVector.Get<FPixelProgrammableDim>(),
				/* bHWRasterShader */ true
			))
		{
			return false;
		}

		return FNaniteMaterialShader::ShouldCompileVertexPermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FNaniteMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("SOFTWARE_RASTER"), 0);
		OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 0);
		OutEnvironment.SetDefine(TEXT("NANITE_MESH_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("NANITE_HW_COUNTER_INDEX"), 4); // Mesh and primitive shaders use an index of 4 instead of 5
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);

		const uint32 MSThreadGroupSize = FDataDrivenShaderPlatformInfo::GetMaxMeshShaderThreadGroupSize(Parameters.Platform);
		check(MSThreadGroupSize == 32 || MSThreadGroupSize == 128 || MSThreadGroupSize == 256);

		const bool bForceBatching = MSThreadGroupSize == 32u;
		if (bForceBatching || PermutationVector.Get<FVertexProgrammableDim>())
		{
			OutEnvironment.SetDefine(TEXT("NANITE_VERT_REUSE_BATCH"), 1);
			OutEnvironment.SetDefine(TEXT("NANITE_MESH_SHADER_TG_SIZE"), 32);
			OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("NANITE_MESH_SHADER_TG_SIZE"), MSThreadGroupSize);
		}

		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FMaterialShader::SetViewParameters(BatchedParameters, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(BatchedParameters, MaterialProxy, Material, View);
	}
};
IMPLEMENT_MATERIAL_SHADER_TYPE(, FHWRasterizeMS, TEXT("/Engine/Private/Nanite/NaniteRasterizer.usf"), TEXT("HWRasterizeMS"), SF_Mesh);

class FHWRasterizeWGMS : public FHWRasterizeMS
{
public:
	DECLARE_SHADER_TYPE(FHWRasterizeWGMS, Material);

	FHWRasterizeWGMS() = default;
	FHWRasterizeWGMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FHWRasterizeMS(Initializer)
	{
	}

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return NaniteWorkGraphMaterialsSupported() && RHISupportsWorkGraphsTier1_1(Parameters.Platform) && FHWRasterizeMS::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FHWRasterizeMS::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("WORKGRAPH_NODE"), 1);
	}
};
IMPLEMENT_MATERIAL_SHADER_TYPE(, FHWRasterizeWGMS, TEXT("/Engine/Private/Nanite/NaniteRasterizer.usf"), TEXT("HWRasterizeMS"), SF_WorkGraphComputeNode);

static TShaderRef<FHWRasterizeMS> GetHWRasterizeMeshShader(
	FMaterialShaderMap const* InShaderMap,
	FHWRasterizeMS::FPermutationDomain& InPermutationVector,
	EShaderFrequency InShaderFrequency)
{
	if (InShaderFrequency == SF_WorkGraphComputeNode)
	{
		return InShaderMap->GetShader<FHWRasterizeWGMS>(InPermutationVector);
	}

	return InShaderMap->GetShader<FHWRasterizeMS>(InPermutationVector);
}

class FHWRasterizePS : public FNaniteMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FHWRasterizePS, Material);
	SHADER_USE_PARAMETER_STRUCT_MIXED(FHWRasterizePS, FNaniteMaterialShader);

	class FDepthOnlyDim : SHADER_PERMUTATION_BOOL("DEPTH_ONLY");
	class FMeshShaderDim : SHADER_PERMUTATION_BOOL("NANITE_MESH_SHADER");
	class FPrimShaderDim : SHADER_PERMUTATION_BOOL("NANITE_PRIM_SHADER");
	class FVisualizeDim : SHADER_PERMUTATION_BOOL("VISUALIZE");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FMaterialCacheDim : SHADER_PERMUTATION_BOOL("MATERIAL_CACHE");
	class FVertexProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_VERTEX_PROGRAMMABLE");
	class FPixelProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_PIXEL_PROGRAMMABLE");
	class FAllowSvBarycentricsDim : SHADER_PERMUTATION_BOOL("NANITE_ALLOW_SV_BARYCENTRICS");

	using FPermutationDomain = TShaderPermutationDomain
	<
		FDepthOnlyDim,
		FMeshShaderDim,
		FPrimShaderDim,
		FVisualizeDim,
		FVirtualTextureTargetDim,
		FMaterialCacheDim,
		FVertexProgrammableDim,
		FPixelProgrammableDim,
		FAllowSvBarycentricsDim
	>;

	using FParameters = FRasterizePassParameters;

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Only some platforms support native 64-bit atomics.
		if (!FDataDrivenShaderPlatformInfo::GetSupportsUInt64ImageAtomics(Parameters.Platform))
		{
			return false;
		}

		if (PermutationVector.Get<FVisualizeDim>() &&
			(PermutationVector.Get<FDepthOnlyDim>() && !PermutationVector.Get<FVirtualTextureTargetDim>()))
		{
			// Visualization not supported with standard depth only, but is with VSM
			return false;
		}

		if (PermutationVector.Get<FMeshShaderDim>() &&
			!FDataDrivenShaderPlatformInfo::GetSupportsMeshShadersTier1(Parameters.Platform))
		{
			// Only some platforms support mesh shaders with tier1 support.
			return false;
		}

		if (PermutationVector.Get<FPrimShaderDim>() &&
			!FDataDrivenShaderPlatformInfo::GetSupportsPrimitiveShaders(Parameters.Platform))
		{
			// Only some platforms support primitive shaders.
			return false;
		}

		if (PermutationVector.Get<FMeshShaderDim>() && PermutationVector.Get<FPrimShaderDim>())
		{
			// Mutually exclusive.
			return false;
		}

		// VSM rendering is depth-only and multiview
		if (PermutationVector.Get<FVirtualTextureTargetDim>() && !PermutationVector.Get<FDepthOnlyDim>())
		{
			return false;
		}

		if (!ShouldCompileSvBarycentricPermutation(Parameters.Platform, PermutationVector.Get<FPixelProgrammableDim>(), PermutationVector.Get<FMeshShaderDim>(), PermutationVector.Get<FAllowSvBarycentricsDim>()))
		{
			return false;
		}

		if (!ShouldCompileProgrammablePermutation(
				Parameters.MaterialParameters,
				PermutationVector.Get<FVertexProgrammableDim>(),
				PermutationVector.Get<FPixelProgrammableDim>(),
				/* bHWRasterShader */ true
			))
		{
			return false;
		}

		return FNaniteMaterialShader::ShouldCompilePixelPermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FNaniteMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);

		OutEnvironment.SetRenderTargetOutputFormat(0, EPixelFormat::PF_R32_UINT);
		OutEnvironment.SetDefine(TEXT("SOFTWARE_RASTER"), 0);
		OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 0);
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);

		const bool bForceBatching = FDataDrivenShaderPlatformInfo::GetMaxMeshShaderThreadGroupSize(Parameters.Platform) == 32u;
		if ((bForceBatching || PermutationVector.Get<FVertexProgrammableDim>()) && (PermutationVector.Get<FMeshShaderDim>() || PermutationVector.Get<FPrimShaderDim>()))
		{
			OutEnvironment.SetDefine(TEXT("NANITE_VERT_REUSE_BATCH"), 1);
		}

		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FMaterialShader::SetViewParameters(BatchedParameters, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(BatchedParameters, MaterialProxy, Material, View);
	}
};
IMPLEMENT_MATERIAL_SHADER_TYPE(, FHWRasterizePS, TEXT("/Engine/Private/Nanite/NaniteRasterizer.usf"), TEXT("HWRasterizePS"), SF_Pixel);

namespace Nanite
{

struct FRasterizerPass
{
	TShaderRef<FHWRasterizePS> RasterPixelShader;
	TShaderRef<FHWRasterizeVS> RasterVertexShader;
	TShaderRef<FHWRasterizeMS> RasterMeshShader;

	TShaderRef<FMicropolyRasterizeCS> ClusterComputeShader;
	TShaderRef<FMicropolyRasterizeCS> PatchComputeShader;

	FNaniteRasterPipeline RasterPipeline{};

	FNaniteRasterMaterialCache* RasterMaterialCache = nullptr;

	const FMaterialRenderProxy* VertexMaterialProxy = nullptr;
	const FMaterialRenderProxy* PixelMaterialProxy = nullptr;
	const FMaterialRenderProxy* ComputeMaterialProxy = nullptr;

	const FMaterial* VertexMaterial = nullptr;
	const FMaterial* PixelMaterial = nullptr;
	const FMaterial* ComputeMaterial = nullptr;

	bool bVertexProgrammable = false;
	bool bPixelProgrammable = false;
	bool bDisplacement = false;
	bool bHidden = false;
	bool bSplineMesh = false;
	bool bSkinnedMesh = false;
	bool bTwoSided = false;
	bool bCastShadow = false;
	bool bVertexUVs = false;
	bool bUseWorkGraphSW = false;
	bool bUseWorkGraphHW = false;

	uint32 IndirectOffset = 0u;
	uint32 RasterBin = ~uint32(0u);

	inline FRHIMeshShader* GetRasterMeshShaderRHI() const
	{
		return (RasterMeshShader.IsValid() && (RasterMeshShader->GetFrequency() == SF_Mesh)) ? RasterMeshShader.GetMeshShader() : nullptr;
	}
	inline FRHIWorkGraphShader* GetRasterWorkGraphShaderRHI() const
	{
		return (RasterMeshShader.IsValid() && (RasterMeshShader->GetFrequency() == SF_WorkGraphComputeNode)) ? RasterMeshShader.GetWorkGraphShader() : nullptr;
	}
	inline FRHIComputeShader* GetClusterComputeShaderRHI() const
	{
		return (ClusterComputeShader.IsValid() && (ClusterComputeShader->GetFrequency() == SF_Compute)) ? ClusterComputeShader.GetComputeShader() : nullptr;
	}
	inline FRHIWorkGraphShader* GetClusterWorkGraphShaderRHI() const
	{
		return (ClusterComputeShader.IsValid() && (ClusterComputeShader->GetFrequency() == SF_WorkGraphComputeNode)) ? ClusterComputeShader.GetWorkGraphShader() : nullptr;
	}

	inline uint32 CalcSortKey() const
	{
		uint32 SortKey = 0u;

		if (IsFixedFunction())
		{
			// Keep fixed function bins in definition order for stability
			SortKey = RasterBin;
		}
		else
		{
			// Sort programmable rasterizers based on shader to minimize state changes
			uint32 Hash = 0u;
			Hash = RasterPixelShader.GetPixelShader()		? GetTypeHash(RasterPixelShader.GetPixelShader()->GetHash()) : Hash;
			Hash = RasterVertexShader.GetVertexShader()		? HashCombineFast(Hash, GetTypeHash(RasterVertexShader.GetVertexShader()->GetHash())) : Hash;
			Hash = GetRasterMeshShaderRHI()					? HashCombineFast(Hash, GetTypeHash(GetRasterMeshShaderRHI()->GetHash())) : Hash;
			Hash = GetRasterWorkGraphShaderRHI()			? HashCombineFast(Hash, GetTypeHash(GetRasterWorkGraphShaderRHI()->GetHash())) : Hash;
			Hash = GetClusterComputeShaderRHI()				? HashCombineFast(Hash, GetTypeHash(GetClusterComputeShaderRHI()->GetHash())) : Hash;
			Hash = GetClusterWorkGraphShaderRHI()			? HashCombineFast(Hash, GetTypeHash(GetClusterWorkGraphShaderRHI()->GetHash())) : Hash;
			Hash = PatchComputeShader.GetComputeShader()	? HashCombineFast(Hash, GetTypeHash(PatchComputeShader.GetComputeShader()->GetHash())) : Hash;

			SortKey |= (1u << 27) | (Hash >> 5);
		}

		const bool bDepthTest = bPixelProgrammable || RasterPipeline.bVoxel;
		if (bDepthTest)
		{
			// Place voxel and pixel programmable rasterizers last as they do depth rejection.
			// Assume pixel programmable shaders are likely closer than voxels, so draw them first.
			SortKey |= RasterPipeline.bVoxel ? (1u << 31) : 0u;
			SortKey |= bPixelProgrammable ? (1u << 30) : 0u;
			

			// Draw depth-testing vertex programmable and skinning permutations earlier as they are likely disabled in the distance.
			SortKey |= !RasterPipeline.bSkinnedMesh ? (1u << 29) : 0u;
			SortKey |= !bVertexProgrammable ? (1u << 28) : 0u;
		}

		return SortKey;
	}

	inline bool HasDerivativeOps() const
	{
		bool bHasDerivativeOps = false;

		if (ClusterComputeShader.IsValid())
		{
			FRHIComputeShader* ClusterCS = GetClusterComputeShaderRHI();
			bHasDerivativeOps |= ClusterCS ? !ClusterCS->HasNoDerivativeOps() : false;
			
			FRHIWorkGraphShader* ClusterWGCS = GetClusterWorkGraphShaderRHI();
			bHasDerivativeOps |= ClusterWGCS ? !ClusterWGCS->HasNoDerivativeOps() : false;
		}

		if (PatchComputeShader.IsValid())
		{
			FRHIComputeShader* PatchCS = PatchComputeShader.GetComputeShader();
			bHasDerivativeOps |= PatchCS ? !PatchCS->HasNoDerivativeOps() : false;
		}

		return bHasDerivativeOps;
	}

	inline bool IsFixedFunction() const
	{
		return (RasterBin <= NANITE_FIXED_FUNCTION_BIN_MASK);
	}
};

#if WANTS_DRAW_MESH_EVENTS
static FORCEINLINE const FString& GetRasterMaterialName(const FRasterizerPass& InRasterPass)
{
	const FMaterialRenderProxy* RasterMaterial = InRasterPass.RasterPipeline.RasterMaterial;
	check(RasterMaterial);

	// TODO: Possibly do a lazy-init with FStringBuilderBase to populate a look up table,
	// but we need to ensure we avoid dynamic allocations here, and allow return-by-ref

	// Any bins within the fixed function bin mask are special cased
	const bool bFixedFunctionBin = InRasterPass.RasterBin <= NANITE_FIXED_FUNCTION_BIN_MASK;
	if (bFixedFunctionBin)
	{
		static const FString Bin0	= TEXT("Fixed Function");

		static const FString Bin1	= TEXT("Fixed Function (TwoSided)");
		static const FString Bin2	= TEXT("Fixed Function (Spline)");
		static const FString Bin4	= TEXT("Fixed Function (Skinned)");
		static const FString Bin8	= TEXT("Fixed Function (CastShadow)");
		static const FString Bin16	= TEXT("Fixed Function (Voxel)");

		// Note: Spline and Skinned are mutually exclusive

		static const FString Bin9	= TEXT("Fixed Function (TwoSided | CastShadow)");

		static const FString Bin3	= TEXT("Fixed Function (Spline | TwoSided)");
		static const FString Bin10	= TEXT("Fixed Function (Spline | CastShadow)");
		static const FString Bin11	= TEXT("Fixed Function (Spline | TwoSided | CastShadow)");

		static const FString Bin5	= TEXT("Fixed Function (Skinned | TwoSided)");
		static const FString Bin12	= TEXT("Fixed Function (Skinned | CastShadow)");
		static const FString Bin13	= TEXT("Fixed Function (Skinned | TwoSided | CastShadow)");

		static const FString Bin20	= TEXT("Fixed Function (Voxel | Skinned)");
		static const FString Bin24	= TEXT("Fixed Function (Voxel | CastShadow)");
		static const FString Bin28	= TEXT("Fixed Function (Voxel | CastShadow | Skinned)");

		switch (InRasterPass.RasterBin)
		{
		default:
			check(false);
		case NANITE_FIXED_FUNCTION_BIN:
			return Bin0;

		case NANITE_FIXED_FUNCTION_BIN_TWOSIDED:
			return Bin1;

		case NANITE_FIXED_FUNCTION_BIN_SPLINE:
			return Bin2;

		case NANITE_FIXED_FUNCTION_BIN_SKINNED:
			return Bin4;

		case NANITE_FIXED_FUNCTION_BIN_CAST_SHADOW:
			return Bin8;

		case NANITE_FIXED_FUNCTION_BIN_VOXEL:
			return Bin16;

		case (NANITE_FIXED_FUNCTION_BIN_TWOSIDED | NANITE_FIXED_FUNCTION_BIN_CAST_SHADOW):
			return Bin9;

		case (NANITE_FIXED_FUNCTION_BIN_SPLINE | NANITE_FIXED_FUNCTION_BIN_TWOSIDED):
			return Bin3;

		case (NANITE_FIXED_FUNCTION_BIN_SPLINE | NANITE_FIXED_FUNCTION_BIN_CAST_SHADOW):
			return Bin10;

		case (NANITE_FIXED_FUNCTION_BIN_SPLINE | NANITE_FIXED_FUNCTION_BIN_TWOSIDED | NANITE_FIXED_FUNCTION_BIN_CAST_SHADOW):
			return Bin11;

		case (NANITE_FIXED_FUNCTION_BIN_SKINNED | NANITE_FIXED_FUNCTION_BIN_TWOSIDED):
			return Bin5;

		case (NANITE_FIXED_FUNCTION_BIN_SKINNED | NANITE_FIXED_FUNCTION_BIN_CAST_SHADOW):
			return Bin12;

		case (NANITE_FIXED_FUNCTION_BIN_SKINNED | NANITE_FIXED_FUNCTION_BIN_TWOSIDED | NANITE_FIXED_FUNCTION_BIN_CAST_SHADOW):
			return Bin13;

		case (NANITE_FIXED_FUNCTION_BIN_VOXEL | NANITE_FIXED_FUNCTION_BIN_SKINNED):
			return Bin20;

		case (NANITE_FIXED_FUNCTION_BIN_VOXEL | NANITE_FIXED_FUNCTION_BIN_CAST_SHADOW):
			return Bin24;

		case (NANITE_FIXED_FUNCTION_BIN_VOXEL | NANITE_FIXED_FUNCTION_BIN_CAST_SHADOW | NANITE_FIXED_FUNCTION_BIN_SKINNED):
			return Bin28;
		}
	}

	return RasterMaterial->GetMaterialName();
}
#endif

void SetupPermutationVectors(
	EOutputBufferMode RasterMode,
	ERasterHardwarePath HardwarePath,
	bool bVisualizeActive,
	bool bHasVirtualShadowMapArray,
	bool bIsMaterialCache,
	FHWRasterizeVS::FPermutationDomain& PermutationVectorVS,
	FHWRasterizeMS::FPermutationDomain& PermutationVectorMS,
	FHWRasterizePS::FPermutationDomain& PermutationVectorPS,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCS_Cluster,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCS_Patch)
{
	bool bDepthOnly = RasterMode == EOutputBufferMode::DepthOnly;
	bool bEnableVisualize = bVisualizeActive && (!bDepthOnly || bHasVirtualShadowMapArray);

	PermutationVectorVS.Set<FHWRasterizeVS::FDepthOnlyDim>(bDepthOnly);
	PermutationVectorVS.Set<FHWRasterizeVS::FPrimShaderDim>(HardwarePath == ERasterHardwarePath::PrimitiveShader);
	PermutationVectorVS.Set<FHWRasterizeVS::FVirtualTextureTargetDim>(bHasVirtualShadowMapArray);
	PermutationVectorVS.Set<FHWRasterizeVS::FMaterialCacheDim>(bIsMaterialCache);

	PermutationVectorMS.Set<FHWRasterizeMS::FDepthOnlyDim>(bDepthOnly);
	PermutationVectorMS.Set<FHWRasterizeMS::FVirtualTextureTargetDim>(bHasVirtualShadowMapArray);
	PermutationVectorMS.Set<FHWRasterizeMS::FMaterialCacheDim>(bIsMaterialCache);

	PermutationVectorPS.Set<FHWRasterizePS::FDepthOnlyDim>(bDepthOnly);
	PermutationVectorPS.Set<FHWRasterizePS::FMeshShaderDim>(IsMeshShaderRasterPath(HardwarePath));
	PermutationVectorPS.Set<FHWRasterizePS::FPrimShaderDim>(HardwarePath == ERasterHardwarePath::PrimitiveShader);
	PermutationVectorPS.Set<FHWRasterizePS::FVisualizeDim>(bEnableVisualize);
	PermutationVectorPS.Set<FHWRasterizePS::FVirtualTextureTargetDim>(bHasVirtualShadowMapArray);
	PermutationVectorPS.Set<FHWRasterizePS::FMaterialCacheDim>(bIsMaterialCache);

	// SW Rasterize
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FPatchesDim>(false); // Clusters
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FDepthOnlyDim>(bDepthOnly);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FVisualizeDim>(bEnableVisualize);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FVirtualTextureTargetDim>(bHasVirtualShadowMapArray);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FMaterialCacheDim>(bIsMaterialCache);

	PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FTessellationDim>(true);
	PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FPatchesDim>(true); // Patches
	PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FDepthOnlyDim>(bDepthOnly);
	PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FVisualizeDim>(bEnableVisualize);
	PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FVirtualTextureTargetDim>(bHasVirtualShadowMapArray);
	PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FMaterialCacheDim>(bIsMaterialCache);
}

static void GetMaterialShaderTypes(
	EShaderPlatform ShaderPlatform,
	const ERasterHardwarePath HardwarePath,
	bool bVertexProgrammable,
	bool bPixelProgrammable,
	bool bIsTwoSided,
	bool bSplineMesh,
	bool bSkinnedMesh,
	bool bDisplacement,
	bool bFixedDisplacementFallback,
	bool bVoxel,
	bool bUseWorkGraphSW,
	bool bUseWorkGraphHW,
	FHWRasterizeVS::FPermutationDomain& PermutationVectorVS,
	FHWRasterizeMS::FPermutationDomain& PermutationVectorMS,
	FHWRasterizePS::FPermutationDomain& PermutationVectorPS,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCS_Cluster,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCS_Patch,
	FMaterialShaderTypes& ProgrammableShaderTypes,
	FMaterialShaderTypes& NonProgrammableShaderTypes,
	FMaterialShaderTypes& PatchShaderTypes)
{
	check(!bSplineMesh  || NaniteSplineMeshesSupported());
	check(!bSkinnedMesh || NaniteSkinnedMeshesSupported());
	check((!bSplineMesh && !bSkinnedMesh) || (bSplineMesh != bSkinnedMesh)); // Mutually exclusive
	check(!bVoxel || !(bSplineMesh || bIsTwoSided));
	
	ProgrammableShaderTypes.PipelineType = nullptr;

	const bool bMeshShaderRasterPath = IsMeshShaderRasterPath(HardwarePath);
	const bool bUseBarycentricPermutation = ShouldUseSvBarycentricPermutation(ShaderPlatform, bPixelProgrammable, bMeshShaderRasterPath);
	const bool bVertexProgrammableHW = !bDisplacement && bVertexProgrammable; // Displacement forces SW raster, so ensure we don't require programmable HW shaders

	// Mesh shader
	if (bMeshShaderRasterPath)
	{
		PermutationVectorMS.Set<FHWRasterizeMS::FSplineDeformDim>(bSplineMesh);
		PermutationVectorMS.Set<FHWRasterizeMS::FSkinningDim>(bSkinnedMesh);
		PermutationVectorMS.Set<FHWRasterizeMS::FVertexProgrammableDim>(bVertexProgrammableHW);
		PermutationVectorMS.Set<FHWRasterizeMS::FPixelProgrammableDim>(bPixelProgrammable);
		PermutationVectorMS.Set<FHWRasterizeMS::FAllowSvBarycentricsDim>(bUseBarycentricPermutation);
		PermutationVectorMS.Set<FHWRasterizeMS::FFixedDisplacementFallbackDim>(bFixedDisplacementFallback);
		if (bVertexProgrammableHW)
		{
			if (bUseWorkGraphHW)
			{
				ProgrammableShaderTypes.AddShaderType<FHWRasterizeWGMS>(PermutationVectorMS.ToDimensionValueId());
			}
			else
			{
				ProgrammableShaderTypes.AddShaderType<FHWRasterizeMS>(PermutationVectorMS.ToDimensionValueId());
			}
		}
		else
		{
			if (bUseWorkGraphHW)
			{
				NonProgrammableShaderTypes.AddShaderType<FHWRasterizeWGMS>(PermutationVectorMS.ToDimensionValueId());
			}
			else
			{
				NonProgrammableShaderTypes.AddShaderType<FHWRasterizeMS>(PermutationVectorMS.ToDimensionValueId());
			}
		}
	}
	// Vertex shader
	else
	{
		PermutationVectorVS.Set<FHWRasterizeVS::FSplineDeformDim>(bSplineMesh);
		PermutationVectorVS.Set<FHWRasterizeVS::FSkinningDim>(bSkinnedMesh);
		PermutationVectorVS.Set<FHWRasterizeVS::FVertexProgrammableDim>(bVertexProgrammableHW);
		PermutationVectorVS.Set<FHWRasterizeVS::FPixelProgrammableDim>(bPixelProgrammable);
		PermutationVectorVS.Set<FHWRasterizeVS::FFixedDisplacementFallbackDim>(bFixedDisplacementFallback);
		if (bVertexProgrammableHW)
		{
			ProgrammableShaderTypes.AddShaderType<FHWRasterizeVS>(PermutationVectorVS.ToDimensionValueId());
		}
		else
		{
			NonProgrammableShaderTypes.AddShaderType<FHWRasterizeVS>(PermutationVectorVS.ToDimensionValueId());
		}
	}

	// Pixel Shader
	PermutationVectorPS.Set<FHWRasterizePS::FVertexProgrammableDim>(bVertexProgrammableHW);
	PermutationVectorPS.Set<FHWRasterizePS::FPixelProgrammableDim>(bPixelProgrammable);
	PermutationVectorPS.Set<FHWRasterizePS::FAllowSvBarycentricsDim>(bUseBarycentricPermutation);
	if (bPixelProgrammable)
	{
		ProgrammableShaderTypes.AddShaderType<FHWRasterizePS>(PermutationVectorPS.ToDimensionValueId());
	}
	else
	{
		NonProgrammableShaderTypes.AddShaderType<FHWRasterizePS>(PermutationVectorPS.ToDimensionValueId());
	}

	// Programmable micropoly features
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FTessellationDim>(bDisplacement);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FPatchesDim>(false);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FTwoSidedDim>(bIsTwoSided);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FVoxelsDim>(bVoxel);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FSplineDeformDim>(bSplineMesh);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FSkinningDim>(bSkinnedMesh);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FVertexProgrammableDim>(bVertexProgrammable);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FPixelProgrammableDim>(bPixelProgrammable);
	PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FFixedDisplacementFallbackDim>(bFixedDisplacementFallback);

	if (bVertexProgrammable || bPixelProgrammable)
	{
		if (bUseWorkGraphSW)
		{
			ProgrammableShaderTypes.AddShaderType<FMicropolyRasterizeWG>(PermutationVectorCS_Cluster.ToDimensionValueId());
		}
		else
		{
			ProgrammableShaderTypes.AddShaderType<FMicropolyRasterizeCS>(PermutationVectorCS_Cluster.ToDimensionValueId());
		}
	}
	else
	{
		if (bUseWorkGraphSW)
		{
			NonProgrammableShaderTypes.AddShaderType<FMicropolyRasterizeWG>(PermutationVectorCS_Cluster.ToDimensionValueId());
		}
		else
		{
			NonProgrammableShaderTypes.AddShaderType<FMicropolyRasterizeCS>(PermutationVectorCS_Cluster.ToDimensionValueId());
		}
	}

	if (bDisplacement)
	{
		PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FTessellationDim>(true);
		PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FPatchesDim>(true);
		PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FTwoSidedDim>(bIsTwoSided);
		PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FSplineDeformDim>(bSplineMesh);
		PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FSkinningDim>(bSkinnedMesh);
		PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FVertexProgrammableDim>(bVertexProgrammable);
		PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FPixelProgrammableDim>(bPixelProgrammable);
		PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FFixedDisplacementFallbackDim>(bFixedDisplacementFallback);
		PatchShaderTypes.AddShaderType<FMicropolyRasterizeCS>(PermutationVectorCS_Patch.ToDimensionValueId());
	}
}

void CollectRasterPSOInitializersForPermutation(
	const FMaterial& Material,
	EShaderPlatform ShaderPlatform,
	const ERasterHardwarePath HardwarePath,
	bool bVertexProgrammable,
	bool bPixelProgrammable,
	bool bIsTwoSided,
	bool bSplineMesh,
	bool bSkinnedMesh,
	bool bDisplacement,
	bool bFixedDisplacementFallback,
	bool bVoxel,
	FHWRasterizeVS::FPermutationDomain& PermutationVectorVS,
	FHWRasterizeMS::FPermutationDomain& PermutationVectorMS,
	FHWRasterizePS::FPermutationDomain& PermutationVectorPS,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCS_Cluster,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCS_Patch,
	int32 PSOCollectorIndex,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	FMaterialShaderTypes ProgrammableShaderTypes;
	FMaterialShaderTypes NonProgrammableShaderTypes;
	FMaterialShaderTypes PatchShaderTypes;

	GetMaterialShaderTypes(
		ShaderPlatform,
		HardwarePath,
		bVertexProgrammable,
		bPixelProgrammable,
		bIsTwoSided,
		bSplineMesh,
		bSkinnedMesh,
		bDisplacement,
		bFixedDisplacementFallback,
		bVoxel,
		false, /* bUseWorkGraphSW */
		false, /* bUseWorkGraphHW */
		PermutationVectorVS,
		PermutationVectorMS,
		PermutationVectorPS,
		PermutationVectorCS_Cluster,
		PermutationVectorCS_Patch,
		ProgrammableShaderTypes,
		NonProgrammableShaderTypes,
		PatchShaderTypes
	);
	
	// Retrieve shaders from default material for fixed function vertex or pixel shaders
	const FMaterialResource* FixedMaterialResource = UMaterial::GetDefaultMaterial(MD_Surface)->GetMaterialResource(Material.GetFeatureLevel(), Material.GetQualityLevel());
	check(FixedMaterialResource);
	
	FMaterialShaders ProgrammableShaders;
	FMaterialShaders NonProgrammableShaders;
	FMaterialShaders PatchShader;

	const bool bFetchProgrammable = Material.TryGetShaders(ProgrammableShaderTypes, nullptr, ProgrammableShaders);
	const bool bFetchNonProgrammable = FixedMaterialResource->TryGetShaders(NonProgrammableShaderTypes, nullptr, NonProgrammableShaders);
	const bool bFetchPatch = !bDisplacement || Material.TryGetShaders(PatchShaderTypes, nullptr, PatchShader);

	if (bFetchProgrammable && bFetchNonProgrammable && bFetchPatch)
	{		
		// Graphics PSO setup
		{
			FGraphicsMinimalPipelineStateInitializer MinimalPipelineStateInitializer;
			MinimalPipelineStateInitializer.BlendState = TStaticBlendState<>::GetRHI();
			MinimalPipelineStateInitializer.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI(); // TODO: PROG_RASTER - Support depth clip as a rasterizer bin and remove shader permutations
			MinimalPipelineStateInitializer.PrimitiveType = HardwarePath == ERasterHardwarePath::PrimitiveShader ? PT_PointList : PT_TriangleList;
			MinimalPipelineStateInitializer.BoundShaderState.VertexDeclarationRHI = IsMeshShaderRasterPath(HardwarePath) ? nullptr : GEmptyVertexDeclaration.VertexDeclarationRHI;
			MinimalPipelineStateInitializer.RasterizerState = GetStaticRasterizerState<false>(FM_Solid, bIsTwoSided ? CM_None : CM_CW);

		#if PLATFORM_SUPPORTS_MESH_SHADERS
			if (IsMeshShaderRasterPath(HardwarePath))
			{
				FMaterialShaders* MeshMaterialShaders = ProgrammableShaders.Shaders[SF_Mesh] ? &ProgrammableShaders : &NonProgrammableShaders;
				MinimalPipelineStateInitializer.BoundShaderState.MeshShaderResource = MeshMaterialShaders->ShaderMap->GetResource();
				MinimalPipelineStateInitializer.BoundShaderState.MeshShaderIndex = MeshMaterialShaders->Shaders[SF_Mesh]->GetResourceIndex();
			}
			else
		#else
			check(!IsMeshShaderRasterPath(HardwarePath));
		#endif
			{
				FMaterialShaders* VertexMaterialShaders = ProgrammableShaders.Shaders[SF_Vertex] ? &ProgrammableShaders : &NonProgrammableShaders;
				MinimalPipelineStateInitializer.BoundShaderState.VertexShaderResource = VertexMaterialShaders->ShaderMap->GetResource();
				MinimalPipelineStateInitializer.BoundShaderState.VertexShaderIndex = VertexMaterialShaders->Shaders[SF_Vertex]->GetResourceIndex();
			}

			FMaterialShaders* PixelMaterialShaders = ProgrammableShaders.Shaders[SF_Pixel] ? &ProgrammableShaders : &NonProgrammableShaders;
			MinimalPipelineStateInitializer.BoundShaderState.PixelShaderResource = PixelMaterialShaders->ShaderMap->GetResource();
			MinimalPipelineStateInitializer.BoundShaderState.PixelShaderIndex = PixelMaterialShaders->Shaders[SF_Pixel]->GetResourceIndex();

			// NOTE: AsGraphicsPipelineStateInitializer will create the RHIShaders internally if they are not cached yet
			FGraphicsPipelineStateInitializer GraphicsPSOInit = MinimalPipelineStateInitializer.AsGraphicsPipelineStateInitializer();

		#if PSO_PRECACHING_VALIDATE
			if (PSOCollectorStats::IsFullPrecachingValidationEnabled())
			{
				MinimalPipelineStateInitializer.StatePrecachePSOHash = GraphicsPSOInit.StatePrecachePSOHash;
				FGraphicsMinimalPipelineStateInitializer ShadersOnlyInitializer = PSOCollectorStats::GetShadersOnlyInitializer(MinimalPipelineStateInitializer);
				PSOCollectorStats::GetShadersOnlyPSOPrecacheStatsCollector().AddStateToCache(EPSOPrecacheType::MeshPass, ShadersOnlyInitializer, PSOCollectorStats::GetPSOPrecacheHash, &Material, PSOCollectorIndex, nullptr);
				FGraphicsMinimalPipelineStateInitializer PatchedMinimalInitializer = PSOCollectorStats::PatchMinimalPipelineStateToCheck(MinimalPipelineStateInitializer);
				PSOCollectorStats::GetMinimalPSOPrecacheStatsCollector().AddStateToCache(EPSOPrecacheType::MeshPass, PatchedMinimalInitializer, PSOCollectorStats::GetPSOPrecacheHash, &Material, PSOCollectorIndex, nullptr);
			}
		#endif
			
			FPSOPrecacheData PSOPrecacheData;
			PSOPrecacheData.Type = FPSOPrecacheData::EType::Graphics;
			PSOPrecacheData.GraphicsPSOInitializer = GraphicsPSOInit;
		#if PSO_PRECACHING_VALIDATE
			PSOPrecacheData.PSOCollectorIndex = PSOCollectorIndex;
			PSOPrecacheData.VertexFactoryType = &FNaniteVertexFactory::StaticType;
		#endif
			PSOInitializers.Add(MoveTemp(PSOPrecacheData));
		}

		// Cluster CS PSO Setup
		{
			FMaterialShaders* ClusterShaders = ProgrammableShaders.Shaders[SF_Compute] ? &ProgrammableShaders : &NonProgrammableShaders;

			TShaderRef<FMicropolyRasterizeCS> ClusterCS;
			if (ClusterShaders->TryGetComputeShader(&ClusterCS))
			{
				FPSOPrecacheData ComputePSOPrecacheData;
				ComputePSOPrecacheData.Type = FPSOPrecacheData::EType::Compute;
				ComputePSOPrecacheData.SetComputeShader(ClusterCS);
			#if PSO_PRECACHING_VALIDATE
				ComputePSOPrecacheData.PSOCollectorIndex = PSOCollectorIndex;
				ComputePSOPrecacheData.VertexFactoryType = nullptr;
				if (PSOCollectorStats::IsFullPrecachingValidationEnabled())
				{
					ComputePSOPrecacheData.bDefaultMaterial = Material.IsDefaultMaterial();
					ConditionalBreakOnPSOPrecacheShader(ComputePSOPrecacheData.ComputeShader);
				}
			#endif
				PSOInitializers.Add(MoveTemp(ComputePSOPrecacheData));
			}
		}

		// Patch CS PSO Setup
		if (bDisplacement)
		{
			TShaderRef<FMicropolyRasterizeCS> PatchCS;

			if (PatchShader.TryGetComputeShader(&PatchCS))
			{
				FPSOPrecacheData ComputePSOPrecacheData;
				ComputePSOPrecacheData.Type = FPSOPrecacheData::EType::Compute;
				ComputePSOPrecacheData.SetComputeShader(PatchCS);
				#if PSO_PRECACHING_VALIDATE
				ComputePSOPrecacheData.PSOCollectorIndex = PSOCollectorIndex;
				ComputePSOPrecacheData.VertexFactoryType = nullptr;
				if (PSOCollectorStats::IsFullPrecachingValidationEnabled())
				{
					ComputePSOPrecacheData.bDefaultMaterial = Material.IsDefaultMaterial();
					ConditionalBreakOnPSOPrecacheShader(ComputePSOPrecacheData.ComputeShader);
				}
				#endif
				PSOInitializers.Add(MoveTemp(ComputePSOPrecacheData));
			}
		}
	}
}

void CollectRasterPSOInitializersForDefaultMaterial(
	const FMaterial& Material,
	EShaderPlatform ShaderPlatform,
	const ERasterHardwarePath HardwarePath,
	FHWRasterizeVS::FPermutationDomain& PermutationVectorVS,
	FHWRasterizeMS::FPermutationDomain& PermutationVectorMS,
	FHWRasterizePS::FPermutationDomain& PermutationVectorPS,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCluster,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorPatch,
	int32 PSOCollectorIndex,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	// Collect PSOs for all possible combinations of vertex/pixel programmable and if two sided or not
	for (uint32 VertexProgrammable = 0; VertexProgrammable < 2; ++VertexProgrammable)
	{
		bool bVertexProgrammable = VertexProgrammable > 0;
		for (uint32 PixelProgrammable = 0; PixelProgrammable < 2; ++PixelProgrammable)
		{
			bool bPixelProgrammable = PixelProgrammable > 0;
			for (uint32 IsTwoSided = 0; IsTwoSided < 2; ++IsTwoSided)
			{
				bool bIsTwoSided = IsTwoSided > 0;
				for (uint32 IsSkinned = 0; IsSkinned < 2; ++IsSkinned)
				{
					bool bSkinnedMesh = IsSkinned > 0;
					for (uint32 SplineMesh = 0; SplineMesh < 2; ++SplineMesh)
					{
						bool bSplineMesh = SplineMesh > 0;
						for (uint32 DisplacementMesh = 0; DisplacementMesh < 2; ++DisplacementMesh)
						{
							bool bDisplacement = DisplacementMesh > 0;
							for (uint32 FixedDisplacementFallbackMesh = 0; FixedDisplacementFallbackMesh < 2; ++FixedDisplacementFallbackMesh)
							{
								bool bFixedDisplacementFallback = FixedDisplacementFallbackMesh > 0;

								if (bSplineMesh && !NaniteSplineMeshesSupported())
									continue;

								if (bSkinnedMesh && !NaniteSkinnedMeshesSupported())
									continue;

								if (bSkinnedMesh && bSplineMesh)
									continue; // Mutually exclusive

								for (uint32 Voxel = 0; Voxel < 2; ++Voxel)
								{
									bool bVoxel = Voxel > 0;

									if (bVoxel && (bIsTwoSided || bSplineMesh))
										continue;

									CollectRasterPSOInitializersForPermutation(Material, ShaderPlatform, HardwarePath, bVertexProgrammable, bPixelProgrammable, bIsTwoSided, bSplineMesh, bSkinnedMesh,
										bDisplacement, bFixedDisplacementFallback, bVoxel, PermutationVectorVS, PermutationVectorMS, PermutationVectorPS, PermutationVectorCluster, PermutationVectorPatch, PSOCollectorIndex, PSOInitializers);
								}
							}
						}
					}
				}
			}
		}
	}
}

void CollectRasterPSOInitializersForPipeline(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& RasterMaterial,
	const FPSOPrecacheParams& PreCacheParams,
	EShaderPlatform ShaderPlatform,
	int32 PSOCollectorIndex,
	EPipeline Pipeline,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	const ERasterHardwarePath HardwarePath = GetRasterHardwarePath(ShaderPlatform, Pipeline);

	const EOutputBufferMode RasterMode = Pipeline == EPipeline::Shadows ? EOutputBufferMode::DepthOnly : EOutputBufferMode::VisBuffer;
	const bool bHasVirtualShadowMapArray = Pipeline == EPipeline::Shadows && UseVirtualShadowMaps(ShaderPlatform, SceneTexturesConfig.FeatureLevel); // true during shadow pass
	const bool bIsMaterialCache = Pipeline == EPipeline::MaterialCache && IsMaterialCacheSupported(ShaderPlatform);
	const bool bVisualizeActive = false; // no precache for visualization modes

	FHWRasterizeVS::FPermutationDomain PermutationVectorVS;
	FHWRasterizeMS::FPermutationDomain PermutationVectorMS;
	FHWRasterizePS::FPermutationDomain PermutationVectorPS;

	FMicropolyRasterizeCS::FPermutationDomain PermutationVectorCS_Cluster;
	FMicropolyRasterizeCS::FPermutationDomain PermutationVectorCS_Patch;

	SetupPermutationVectors(
		RasterMode,
		HardwarePath,
		bVisualizeActive,
		bHasVirtualShadowMapArray,
		bIsMaterialCache,
		PermutationVectorVS,
		PermutationVectorMS,
		PermutationVectorPS,
		PermutationVectorCS_Cluster,
		PermutationVectorCS_Patch
	);

	if (PreCacheParams.bDefaultMaterial)
	{
		CollectRasterPSOInitializersForDefaultMaterial(RasterMaterial, ShaderPlatform, HardwarePath, PermutationVectorVS, PermutationVectorMS, PermutationVectorPS, PermutationVectorCS_Cluster, PermutationVectorCS_Patch, PSOCollectorIndex, PSOInitializers);
	}
	else
	{
		const auto AddPSOInitializers = [&](bool bForceDisableWPOOrDisplacement, bool bForceDisablePixelEval)
		{
			// Set up a theoretical RasterPipeline that enables the feature set we're collecting for
			// NOTE: When we force disable pixel programmable, we also force disable displacement
			FNaniteRasterPipeline RasterPipeline;
			RasterPipeline.bWPOEnabled = !bForceDisableWPOOrDisplacement;
			RasterPipeline.bDisplacementEnabled = !bForceDisableWPOOrDisplacement;
			RasterPipeline.bPerPixelEval = !bForceDisablePixelEval;
			RasterPipeline.bSkinnedMesh = PreCacheParams.bSkinnedMesh;
			if (RasterPipeline.bSkinnedMesh)
			{
				RasterPipeline.bSplineMesh = false;
			}
			else
			{
				RasterPipeline.bSplineMesh = PreCacheParams.bSplineMesh;
			}
			RasterPipeline.bIsTwoSided = RasterMaterial.IsTwoSided();

			const uint32 MaterialBitFlags = PackMaterialBitFlags_GameThread(RasterMaterial, RasterPipeline);
			const bool bVertexProgrammable = FNaniteMaterialShader::IsVertexProgrammable(MaterialBitFlags);
			const bool bPixelProgrammable = FNaniteMaterialShader::IsPixelProgrammable(MaterialBitFlags);
			const bool bIsTwoSided = MaterialBitFlags & NANITE_MATERIAL_FLAG_TWO_SIDED;
			const bool bDisplacement = MaterialBitFlags & NANITE_MATERIAL_FLAG_DISPLACEMENT;
			const bool bSplineMesh = MaterialBitFlags & NANITE_MATERIAL_FLAG_SPLINE_MESH;
			const bool bSkinnedMesh = MaterialBitFlags & NANITE_MATERIAL_FLAG_SKINNED_MESH;
			const bool bFixedDisplacementFallback = false;

			const FMeshPassProcessor::FMeshDrawingPolicyOverrideSettings OverrideSettings = FMeshPassProcessor::ComputeMeshOverrideSettings(PreCacheParams);
			ERasterizerCullMode MeshCullMode = FMeshPassProcessor::ComputeMeshCullMode(RasterMaterial, OverrideSettings);

			CollectRasterPSOInitializersForPermutation(RasterMaterial, ShaderPlatform, HardwarePath, bVertexProgrammable, bPixelProgrammable, bIsTwoSided, bSplineMesh, bSkinnedMesh, bDisplacement, bFixedDisplacementFallback,
				/* bVoxel */ false, PermutationVectorVS, PermutationVectorMS, PermutationVectorPS, PermutationVectorCS_Cluster, PermutationVectorCS_Patch, PSOCollectorIndex, PSOInitializers);
		};

		// Add initializers for all features that can be toggled in fallback bins (NOTE: can't disable both)
		AddPSOInitializers(false /*bForceDisableWPOOrDisplacement*/, false /*bForceDisablePixelEval*/);
		AddPSOInitializers(false /*bForceDisableWPOOrDisplacement*/, true /*bForceDisablePixelEval*/);
		AddPSOInitializers(true /*bForceDisableWPOOrDisplacement*/, false /*bForceDisablePixelEval*/);
	}
}

void CollectRasterPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& RasterMaterial,
	const FPSOPrecacheParams& PreCacheParams,
	EShaderPlatform ShaderPlatform,
	int32 PSOCollectorIndex,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	// Collect for primary & shadows
	CollectRasterPSOInitializersForPipeline(SceneTexturesConfig, RasterMaterial, PreCacheParams, ShaderPlatform, PSOCollectorIndex, EPipeline::Primary, PSOInitializers);
	CollectRasterPSOInitializersForPipeline(SceneTexturesConfig, RasterMaterial, PreCacheParams, ShaderPlatform, PSOCollectorIndex, EPipeline::Shadows, PSOInitializers);
}


class FTessellationTableResources : public FRenderResource
{
public:
	FByteAddressBuffer	Offsets;
	FByteAddressBuffer	VertsAndIndexes;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;
};

template< typename T >
static void CreateAndUpload( FRHICommandListBase& RHICmdList, FByteAddressBuffer& Buffer, const TArray<T>& Array, const TCHAR* InDebugName )
{
	Buffer.Initialize( RHICmdList, InDebugName, Array.Num() * Array.GetTypeSize() );

	uint8* DataPtr = (uint8*)RHICmdList.LockBuffer( Buffer.Buffer, 0, Buffer.NumBytes, RLM_WriteOnly );

	FMemory::Memcpy( DataPtr, Array.GetData(), Buffer.NumBytes );

	RHICmdList.UnlockBuffer( Buffer.Buffer );
}

void FTessellationTableResources::InitRHI(FRHICommandListBase& RHICmdList)
{
	if( DoesPlatformSupportNanite( GMaxRHIShaderPlatform ) )
	{
		FTessellationTable TessellationTable;

		CreateAndUpload( RHICmdList, Offsets,			TessellationTable.OffsetTable,		TEXT("TessellationTable.Offsets") );
		CreateAndUpload( RHICmdList, VertsAndIndexes,	TessellationTable.VertsAndIndexes,	TEXT("TessellationTable.VertsAndIndexes") );
	}
}

void FTessellationTableResources::ReleaseRHI()
{
	if( DoesPlatformSupportNanite( GMaxRHIShaderPlatform ) )
	{
		Offsets.Release();
		VertsAndIndexes.Release();
	}
}

TGlobalResource< FTessellationTableResources > GTessellationTable;


static void AddPassInitNodesAndClusterBatchesUAV( FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGBufferUAVRef UAVRef )
{
	LLM_SCOPE_BYTAG(Nanite);

	{
		FInitCandidateNodes_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitCandidateNodes_CS::FParameters >();
		PassParameters->OutMainAndPostNodesAndClusterBatches= UAVRef;
		PassParameters->MaxCandidateClusters				= Nanite::FGlobalResources::GetMaxCandidateClusters();
		PassParameters->MaxNodes							= Nanite::FGlobalResources::GetMaxNodes();

		auto ComputeShader = ShaderMap->GetShader< FInitCandidateNodes_CS >();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME( "Nanite::InitNodes" ),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCountWrapped(Nanite::FGlobalResources::GetMaxNodes(), 64)
		);
	}

	{
		FInitClusterBatches_CS::FParameters* PassParameters	= GraphBuilder.AllocParameters< FInitClusterBatches_CS::FParameters >();
		PassParameters->OutMainAndPostNodesAndClusterBatches= UAVRef;
		PassParameters->MaxCandidateClusters				= Nanite::FGlobalResources::GetMaxCandidateClusters();
		PassParameters->MaxNodes							= Nanite::FGlobalResources::GetMaxNodes();

		auto ComputeShader = ShaderMap->GetShader< FInitClusterBatches_CS >();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME( "Nanite::InitCullingBatches" ),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCountWrapped(Nanite::FGlobalResources::GetMaxClusterBatches(), 64)
		);
	}
}

/** Creates a line slope/offset to calculate displacement fade from max displacement in terms of on-screen triangle size */
static void CalcDisplacementFadeSizes(const FDisplacementFadeRange& Range, float& FadeSizeStart, float& FadeSizeStop)
{
	const float EdgesPerPixel = 1.0f / CVarNaniteMaxPixelsPerEdge.GetValueOnRenderThread();
	if (!Range.IsValid())
	{
		FadeSizeStart = FadeSizeStop = 0.0f;
	}
	else
	{
		// Ensure a non-zero domain, a negative slope, and that it doesn't converge at zero
		FadeSizeStop = EdgesPerPixel * FMath::Max(Range.EndSizePixels, UE_KINDA_SMALL_NUMBER);
		FadeSizeStart = EdgesPerPixel * FMath::Max(Range.StartSizePixels, Range.EndSizePixels + UE_KINDA_SMALL_NUMBER);
	}
}

class FRenderer;

class FInstanceHierarchyDriver
{
public:
	inline bool IsEnabled() const { return bIsEnabled; }

	bool bIsEnabled = false;
	bool bAllowStaticGeometryPath = true;

	uint32 GroupWorkArgsMaxCount = 0u;
	// pass around hierarhcy arguments to drive culling etc etc.
	FInstanceHierarchyParameters ShaderParameters;

	FRDGBuffer* ViewDrawRangesRDG = nullptr;
	FRDGBuffer* InstanceWorkGroupsRDG = nullptr;
	FRDGBuffer*	InstanceWorkArgs[2];

	FRDGBuffer* OccludedChunkArgsRDG = nullptr;
	FRDGBuffer* OccludedChunkDrawsRDG = nullptr;
	FRDGBuffer* ChunkDrawViewGroupIdsRDG = nullptr;

	struct FDeferredSetupContext
	{
		void Sync()
		{
			// Only do the first time
			if (bAlreadySynced)
			{
				return;
			}
			bAlreadySynced = true;
			SceneInstanceCullResult = SceneInstanceCullingQuery->GetResult();
			check(SceneInstanceCullResult->NumInstanceGroups >= 0 && SceneInstanceCullResult->NumInstanceGroups < 4 * 1024 * 1024);
			MaxInstanceWorkGroups = FMath::RoundUpToPowerOfTwo(SceneInstanceCullResult->NumInstanceGroups);
			NumViewDrawRanges = SceneInstanceCullingQuery->GetViewDrawGroups().Num();
			MaxOccludedChunkDrawsPOT = FMath::RoundUpToPowerOfTwo(SceneInstanceCullResult->MaxOccludedChunkDraws);
			NumChunkViewGroups = SceneInstanceCullResult->ChunkCullViewGroupIds.Num();
			NumAllocatedChunks = SceneInstanceCullResult->NumAllocatedChunks;
		}

		uint32 GetMaxInstanceWorkGroups()
		{
			check(bAlreadySynced);
			check(MaxInstanceWorkGroups != ~0u);
			return MaxInstanceWorkGroups;
		}

		FSceneInstanceCullingQuery* SceneInstanceCullingQuery = nullptr;
		FSceneInstanceCullResult* SceneInstanceCullResult = nullptr;
		uint32 MaxOccludedChunkDrawsPOT = 0u;
		uint32 MaxInstanceWorkGroups = ~0u;
		uint32 NumViewDrawRanges = ~0u;
		uint32 NumChunkViewGroups = ~0u;
		uint32 NumAllocatedChunks = ~0u;		
		bool bAlreadySynced = false;
	};
	FDeferredSetupContext *DeferredSetupContext = nullptr;

	void Init(FRDGBuilder& GraphBuilder, bool bInIsEnabled, bool bTwoPassOcclusion, const FGlobalShaderMap* ShaderMap, FSceneInstanceCullingQuery* SceneInstanceCullingQuery, FRDGBufferRef InViewDrawRanges);
	FInstanceWorkGroupParameters DispatchCullingPass(FRDGBuilder& GraphBuilder, uint32 CullingPass, const FRenderer& Renderer);
};

template<typename TShaderType>
inline void SetHWBundleParameters(
	TOptional<FRHIBatchedShaderParameters>& BatchedParameters,
	FRHIBatchedShaderParametersAllocator& ScratchAllocator,
	const TShaderRef<TShaderType>& InShader,
	const FHWRasterizePS::FParameters& Parameters,
	bool bUsingSharedParameters,
	const FViewInfo& View,
	const FMaterialRenderProxy* MaterialProxy,
	const FMaterial& Material)
{
	BatchedParameters.Emplace(ScratchAllocator);

	// New Style first
	if (!bUsingSharedParameters)
	{
		SetShaderParameters(*BatchedParameters, InShader, Parameters);
	}

	// Legacy second
	InShader->SetParameters(*BatchedParameters, View, MaterialProxy, Material);

	BatchedParameters->Finish();
}

class FRenderer : public FSceneRenderingAllocatorObject< FRenderer >, public IRenderer
{
public:
	FRenderer(
		FRDGBuilder&			InGraphBuilder,
		const FScene&			InScene,
		const FViewInfo&		InSceneView,
		const TRDGUniformBufferRef<FSceneUniformParameters>& InSceneUniformBuffer,
		const FSharedContext&	InSharedContext,
		const FRasterContext&	InRasterContext,
		const FConfiguration&	InConfiguration,
		const FIntRect&			InViewRect,
		const TRefCountPtr<IPooledRenderTarget>& InPrevHZB,
		FVirtualShadowMapArray*	InVirtualShadowMapArray );

	friend class FInstanceHierarchyDriver;

private:
	using FRasterBinMetaArray = TArray<FNaniteRasterBinMeta, SceneRenderingAllocator>;

	struct FDispatchContext
	{
		struct FDispatchList
		{
			TArray<int32, SceneRenderingAllocator> Indirections;
		};

		FDispatchList Dispatches_HW_Triangles;
		FDispatchList Dispatches_SW_Triangles;
		FDispatchList Dispatches_SW_Tessellated;

		TArray<FRasterizerPass, SceneRenderingAllocator> RasterizerPasses;

		FRasterBinMetaArray MetaBufferData;
		FRDGBufferRef MetaBuffer = nullptr;

		FShaderBundleRHIRef HWShaderBundle;
		FShaderBundleRHIRef SWShaderBundle;
		FShaderBundleRHIRef SWShaderBundleAsync;

		const FMaterialRenderProxy* FixedMaterialProxy = nullptr;
		const FMaterialRenderProxy* HiddenMaterialProxy = nullptr;

		TRDGUniformBufferRef<FNaniteRasterUniformParameters> RasterUniformBuffer = nullptr;

		uint32 NumDepthBlocks = 0;
		bool bAnyBindless = false;

		void Reserve(int32 BinCount)
		{
			RasterizerPasses.Reserve(BinCount);
			Dispatches_HW_Triangles.Indirections.Reserve(BinCount);
			Dispatches_SW_Triangles.Indirections.Reserve(BinCount);
			Dispatches_SW_Tessellated.Indirections.Reserve(BinCount);
		}

		bool HasTessellated() const
		{
			return Dispatches_SW_Tessellated.Indirections.Num() > 0;
		}

		void DispatchHW(
			FRHICommandList& RHICmdList,
			const FDispatchList& DispatchList,
			const FViewInfo& ViewInfo,
			const FIntRect& ViewRect,
			const ERasterHardwarePath HardwarePath,
			int32 PSOCollectorIndex,
			FHWRasterizePS::FParameters Parameters /* Intentional Copy */
		) const
		{
			const bool bShowDrawEvents		= GShowMaterialDrawEvents != 0;
			const bool bAllowPrecacheSkip	= GSkipDrawOnPSOPrecaching != 0;
			const bool bTestPrecacheSkip	= CVarNaniteTestPrecacheDrawSkipping.GetValueOnRenderThread() != 0;
			const bool bBundleEmulation		= CVarNaniteBundleEmulation.GetValueOnRenderThread() != 0;

			if (DispatchList.Indirections.Num() > 0)
			{
				Parameters.IndirectArgs->MarkResourceAsUsed();

				FRHIRenderPassInfo RPInfo;
				RPInfo.ResolveRect = FResolveRect(ViewRect);

				RHICmdList.BeginRenderPass(RPInfo, TEXT("HW Rasterize"));
				RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, FMath::Min(ViewRect.Max.X, 32767), FMath::Min(ViewRect.Max.Y, 32767), 1.0f);
				RHICmdList.SetStreamSource(0, nullptr, 0);

				EPrimitiveType PrimitiveType = (HardwarePath == ERasterHardwarePath::PrimitiveShader) ? PT_PointList : PT_TriangleList;
				FRHIBlendState* BlendState = TStaticBlendState<>::GetRHI();
				FRHIDepthStencilState* DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				FRHIVertexDeclaration* VertexDeclaration = IsMeshShaderRasterPath(HardwarePath) ? nullptr : GEmptyVertexDeclaration.VertexDeclarationRHI;

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				GraphicsPSOInit.BlendState = BlendState;
				GraphicsPSOInit.DepthStencilState = DepthStencilState;
				GraphicsPSOInit.PrimitiveType = PrimitiveType;
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDeclaration;

				auto BindShadersToPSOInit = [HardwarePath, &GraphicsPSOInit](const FRasterizerPass& PassToBind)
				{
					if (IsMeshShaderRasterPath(HardwarePath))
					{
						GraphicsPSOInit.BoundShaderState.SetMeshShader(PassToBind.GetRasterMeshShaderRHI());
						GraphicsPSOInit.BoundShaderState.SetWorkGraphShader(PassToBind.GetRasterWorkGraphShaderRHI());
					}
					else
					{
						GraphicsPSOInit.BoundShaderState.VertexShaderRHI = PassToBind.RasterVertexShader.GetVertexShader();
					}

					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PassToBind.RasterPixelShader.GetPixelShader();
				};

				if (HWShaderBundle != nullptr)
				{
					auto RecordDispatches = [&](FRHICommandDispatchGraphicsShaderBundle& Command)
					{
						Command.ShaderBundle		= HWShaderBundle;
						Command.bEmulated			= bBundleEmulation;
						Command.RecordArgBuffer		= Parameters.IndirectArgs->GetIndirectRHICallBuffer();

						Command.BundleState.ViewRect		= ViewRect;
						Command.BundleState.PrimitiveType	= HardwarePath == ERasterHardwarePath::PrimitiveShader ? PT_PointList : PT_TriangleList;

						Command.Dispatches.SetNum(HWShaderBundle->NumRecords);

						for (FRHIShaderBundleGraphicsDispatch& Dispatch : Command.Dispatches)
						{
							// TODO: Allow for sending partial dispatch lists, but for now we'll leave the record index invalid so bundle dispatch skips it
							Dispatch.RecordIndex = ~uint32(0u);
						}

						FRHIBatchedShaderParametersAllocator& ScratchAllocator = RHICmdList.GetScratchShaderParameters().Allocator;

						FRHIBatchedShaderParameters CommonParameters(ScratchAllocator);
						const bool bUsingSharedParameters = GRHIGlobals.ShaderBundles.RequiresSharedBindlessParameters;
						if (bUsingSharedParameters)
						{
							SetAllShaderParametersAsBindless(CommonParameters, Parameters);
							CommonParameters.Finish();
							Command.SharedBindlessParameters = CommonParameters.BindlessParameters;
						}

						for (const int32 Indirection : DispatchList.Indirections)
						{
							const FRasterizerPass& RasterizerPass = RasterizerPasses[Indirection];
							Parameters.PassData = FUintVector4(RasterizerPass.RasterBin, 0u, 0u, 0u);

							FRHIShaderBundleGraphicsDispatch& Dispatch = Command.Dispatches[RasterizerPass.RasterBin];
							Dispatch.RecordIndex = RasterizerPass.RasterBin;
							Dispatch.Constants = Parameters.PassData;

							// NOTE: We do *not* use any CullMode overrides here because HWRasterize[VS/MS] already
							// changes the index order in cases where the culling should be flipped.
							// The exception is if CM_None is specified for two sided materials, or if the entire raster pass has CM_None specified.
							const bool bCullModeNone = RasterizerPass.RasterPipeline.bIsTwoSided;
							GraphicsPSOInit.RasterizerState = GetStaticRasterizerState<false>(FM_Solid, bCullModeNone ? CM_None : CM_CW);

							BindShadersToPSOInit(RasterizerPass);

						#if PSO_PRECACHING_VALIDATE
							if (PSOCollectorStats::IsFullPrecachingValidationEnabled())
							{
								PSOCollectorStats::CheckFullPipelineStateInCache(GraphicsPSOInit, EPSOPrecacheResult::Unknown, RasterizerPass.RasterPipeline.RasterMaterial, &FNaniteVertexFactory::StaticType, nullptr, PSOCollectorIndex);
							}
						#endif

							if (IsMeshShaderRasterPath(HardwarePath))
							{
								SetHWBundleParameters(Dispatch.Parameters_MSVS, ScratchAllocator, RasterizerPass.RasterMeshShader, Parameters, bUsingSharedParameters, ViewInfo, RasterizerPass.VertexMaterialProxy, *RasterizerPass.VertexMaterial);
							}
							else
							{
								SetHWBundleParameters(Dispatch.Parameters_MSVS, ScratchAllocator, RasterizerPass.RasterVertexShader, Parameters, bUsingSharedParameters, ViewInfo, RasterizerPass.VertexMaterialProxy, *RasterizerPass.VertexMaterial);
							}

							SetHWBundleParameters(Dispatch.Parameters_PS, ScratchAllocator, RasterizerPass.RasterPixelShader, Parameters, bUsingSharedParameters, ViewInfo, RasterizerPass.PixelMaterialProxy, *RasterizerPass.PixelMaterial);

							Dispatch.PipelineInitializer = GraphicsPSOInit;
							Dispatch.PipelineState = RasterizerPass.bUseWorkGraphHW ? nullptr : FindGraphicsPipelineState(Dispatch.PipelineInitializer);
							if (Dispatch.PipelineState == nullptr && !(RasterizerPass.bUseWorkGraphHW && GraphicsPSOInit.BoundShaderState.GetWorkGraphShader()))
							{
								// If we don't have precaching, then GetGraphicsPipelineState() might return a PipelineState that isn't ready.
								const bool bSkipDraw = !PipelineStateCache::IsPSOPrecachingEnabled();

								Dispatch.PipelineState = GetGraphicsPipelineState(RHICmdList, Dispatch.PipelineInitializer, !bSkipDraw);

								if (bSkipDraw)
								{
									Dispatch.RecordIndex = ~uint32(0u);
									continue;
								}
							}
						}
					};

					RHICmdList.DispatchGraphicsShaderBundle(RecordDispatches);
				}
				else
				{
					for (const int32 Indirection : DispatchList.Indirections)
					{
						const FRasterizerPass& RasterizerPass = RasterizerPasses[Indirection];

					#if WANTS_DRAW_MESH_EVENTS
						SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, HWRaster, bShowDrawEvents != 0, TEXT("%s"), GetRasterMaterialName(RasterizerPass));
					#endif

						Parameters.PassData = FUintVector4(RasterizerPass.RasterBin, 0u, 0u, 0u);

						// NOTE: We do *not* use any CullMode overrides here because HWRasterize[VS/MS] already
						// changes the index order in cases where the culling should be flipped.
						// The exception is if CM_None is specified for two sided materials, or if the entire raster pass has CM_None specified.
						const bool bCullModeNone = RasterizerPass.RasterPipeline.bIsTwoSided;
						GraphicsPSOInit.RasterizerState = GetStaticRasterizerState<false>(FM_Solid, bCullModeNone ? CM_None : CM_CW);

						auto BindShaderParameters = [HardwarePath, &RHICmdList, &ViewInfo, &Parameters](const FRasterizerPass& PassToBind)
						{
							if (IsMeshShaderRasterPath(HardwarePath))
							{
								SetShaderParametersMixedMS(RHICmdList, PassToBind.RasterMeshShader, Parameters, ViewInfo, PassToBind.VertexMaterialProxy, *PassToBind.VertexMaterial);
							}
							else
							{
								SetShaderParametersMixedVS(RHICmdList, PassToBind.RasterVertexShader, Parameters, ViewInfo, PassToBind.VertexMaterialProxy, *PassToBind.VertexMaterial);
							}

							SetShaderParametersMixedPS(RHICmdList, PassToBind.RasterPixelShader, Parameters, ViewInfo, PassToBind.PixelMaterialProxy, *PassToBind.PixelMaterial);
						};

						// Disabled for now because this will call PipelineStateCache::IsPrecaching which requires the PSO to have
						// the minimal state hash computed. Computing this for each PSO each frame is not cheap and ideally the minimal
						// PSO state can be cached like regular MDCs before activating this (UE-171561)
						if (false) //bAllowPrecacheSkip && (bTestPrecacheSkip || PipelineStateCache::IsPrecaching(GraphicsPSOInit)))
						{
							// Programmable raster PSO has not been precached yet, fallback to fixed function in the meantime to avoid hitching.

							uint32 FixedFunctionBin = NANITE_FIXED_FUNCTION_BIN;

							if (RasterizerPass.bTwoSided && !RasterizerPass.RasterPipeline.bVoxel)
							{
								FixedFunctionBin |= NANITE_FIXED_FUNCTION_BIN_TWOSIDED;
							}

							// Mutually exclusive
							if (RasterizerPass.bSkinnedMesh)
							{
								FixedFunctionBin |= NANITE_FIXED_FUNCTION_BIN_SKINNED;
							}
							else if (RasterizerPass.bSplineMesh && !RasterizerPass.RasterPipeline.bVoxel)
							{
								FixedFunctionBin |= NANITE_FIXED_FUNCTION_BIN_SPLINE;
							}

							if (RasterizerPass.RasterPipeline.bVoxel)
							{
								FixedFunctionBin |= NANITE_FIXED_FUNCTION_BIN_VOXEL;
							}

							const FRasterizerPass* FixedFunctionPass = RasterizerPasses.FindByPredicate([FixedFunctionBin](const FRasterizerPass& Pass)
							{
								return Pass.RasterBin == FixedFunctionBin;
							});

							check(FixedFunctionPass);

							BindShadersToPSOInit(*FixedFunctionPass);
							SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
							BindShaderParameters(*FixedFunctionPass);
						}
						else
						{
							BindShadersToPSOInit(RasterizerPass);

						#if PSO_PRECACHING_VALIDATE
							if (PSOCollectorStats::IsFullPrecachingValidationEnabled())
							{
								PSOCollectorStats::CheckFullPipelineStateInCache(GraphicsPSOInit, EPSOPrecacheResult::Unknown, RasterizerPass.RasterPipeline.RasterMaterial, &FNaniteVertexFactory::StaticType, nullptr, PSOCollectorIndex);
							}
						#endif

							SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
							BindShaderParameters(RasterizerPass);
						}

						if (GRHISupportsShaderRootConstants)
						{
							RHICmdList.SetShaderRootConstants(Parameters.PassData);
						}

						if (IsMeshShaderRasterPath(HardwarePath))
						{
							RHICmdList.DispatchIndirectMeshShader(Parameters.IndirectArgs->GetIndirectRHICallBuffer(), RasterizerPass.IndirectOffset + 16);
						}
						else
						{
							RHICmdList.DrawPrimitiveIndirect(Parameters.IndirectArgs->GetIndirectRHICallBuffer(), RasterizerPass.IndirectOffset + 16);
						}
					}
				}

				RHICmdList.EndRenderPass();
			}
		}

		void DispatchSW(
			FRHIComputeCommandList& RHICmdList,
			const FDispatchList& DispatchList,
			const FViewInfo& ViewInfo,
			int32 PSOCollectorIndex,
			FRasterizePassParameters Parameters, /* Intentional Copy */
			bool bPatches
		) const
		{
			const bool bShowDrawEvents	= GShowMaterialDrawEvents != 0;
			const bool bBundleEmulation	= CVarNaniteBundleEmulation.GetValueOnRenderThread() != 0;

			FShaderBundleRHIRef ShaderBundleToUse = RHICmdList.IsAsyncCompute() ? SWShaderBundleAsync : SWShaderBundle;

			if (DispatchList.Indirections.Num() > 0)
			{
				Parameters.IndirectArgs->MarkResourceAsUsed();

				if (ShaderBundleToUse != nullptr)
				{
					auto RecordDispatches = [&](FRHICommandDispatchComputeShaderBundle& Command)
					{
						Command.ShaderBundle	= ShaderBundleToUse;
						Command.bEmulated		= bBundleEmulation;
						Command.RecordArgBuffer	= Parameters.IndirectArgs->GetIndirectRHICallBuffer();

						Command.Dispatches.SetNum(ShaderBundleToUse->NumRecords);

						for (FRHIShaderBundleComputeDispatch& Dispatch : Command.Dispatches)
						{
							// TODO: Allow for sending partial dispatch lists, but for now we'll leave the record index invalid so bundle dispatch skips it
							Dispatch.RecordIndex = ~uint32(0u);
						}

						FRHIBatchedShaderParametersAllocator& ScratchAllocator = RHICmdList.GetScratchShaderParameters().Allocator;

						if (GRHIGlobals.ShaderBundles.RequiresSharedBindlessParameters)
						{
							FRHIBatchedShaderParameters CommonParameters(ScratchAllocator);
							SetAllShaderParametersAsBindless(CommonParameters, Parameters);
							CommonParameters.Finish();

							Command.SharedBindlessParameters = CommonParameters.BindlessParameters;
						}

						for (const int32 Indirection : DispatchList.Indirections)
						{
							const FRasterizerPass& RasterizerPass = RasterizerPasses[Indirection];
							Parameters.PassData = FUintVector4(RasterizerPass.RasterBin, 0u, 0u, 0u);

							FRHIShaderBundleComputeDispatch& Dispatch = Command.Dispatches[RasterizerPass.RasterBin];
							Dispatch.RecordIndex = RasterizerPass.RasterBin;
							Dispatch.Constants = Parameters.PassData;

							const TShaderRef<FMicropolyRasterizeCS>* Shader = bPatches ? &RasterizerPass.PatchComputeShader : &RasterizerPass.ClusterComputeShader;
							const EShaderFrequency ShaderFrequency = Shader->GetShader()->GetFrequency();
							Dispatch.Shader = ShaderFrequency == SF_Compute ? Shader->GetComputeShader() : nullptr;
							Dispatch.WorkGraphShader = ShaderFrequency == SF_WorkGraphComputeNode ? Shader->GetWorkGraphShader() : nullptr;

							Dispatch.Parameters.Emplace(ScratchAllocator);

							SetShaderBundleParameters(
								*Dispatch.Parameters,
								*Shader,
								Parameters,
								ShaderFrequency,
								ViewInfo,
								RasterizerPass.ComputeMaterialProxy,
								*RasterizerPass.ComputeMaterial
							);

							Dispatch.Parameters->Finish();

							// TODO: Implement support for testing precache and skipping if needed

						#if PSO_PRECACHING_VALIDATE
							if (Dispatch.Shader != nullptr)
							{
								EPSOPrecacheResult PSOPrecacheResult = PipelineStateCache::CheckPipelineStateInCache(Dispatch.Shader);
								PSOCollectorStats::CheckComputePipelineStateInCache(*Dispatch.Shader, PSOPrecacheResult, RasterizerPass.ComputeMaterialProxy, PSOCollectorIndex);
							}
						#endif

							Dispatch.PipelineState = Dispatch.Shader != nullptr ? FindComputePipelineState(Dispatch.Shader) : nullptr;
							if (Dispatch.Shader != nullptr && Dispatch.PipelineState == nullptr)
							{
								// If we don't have precaching, then GetComputePipelineState() might return a PipelineState that isn't ready.
								const bool bSkipDraw = !PipelineStateCache::IsPSOPrecachingEnabled();

								Dispatch.PipelineState = GetComputePipelineState(RHICmdList, Dispatch.Shader, !bSkipDraw);

								if (bSkipDraw)
								{
									Dispatch.RecordIndex = ~uint32(0u);
									continue;
								}
							}
						}
					};

					RHICmdList.DispatchComputeShaderBundle(RecordDispatches);
				}
				else
				{
					for (const int32 Indirection : DispatchList.Indirections)
					{
						const FRasterizerPass& RasterizerPass = RasterizerPasses[Indirection];

					#if WANTS_DRAW_MESH_EVENTS
						SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, SWRaster, bShowDrawEvents, TEXT("%s"), GetRasterMaterialName(RasterizerPass));
					#endif

						Parameters.PassData = FUintVector4(RasterizerPass.RasterBin, 0u, 0u, 0u);

						const TShaderRef<FMicropolyRasterizeCS>* ComputeShader = bPatches ? &RasterizerPass.PatchComputeShader : &RasterizerPass.ClusterComputeShader;

						FRHIBuffer* IndirectArgsBuffer = Parameters.IndirectArgs->GetIndirectRHICallBuffer();
						FRHIComputeShader* ShaderRHI = ComputeShader->GetComputeShader();

						// TODO: Implement support for testing precache and skipping if needed

						FComputeShaderUtils::ValidateIndirectArgsBuffer(IndirectArgsBuffer->GetSize(), RasterizerPass.IndirectOffset);

						SetComputePipelineState(RHICmdList, ShaderRHI);

					#if PSO_PRECACHING_VALIDATE
						EPSOPrecacheResult PSOPrecacheResult = PipelineStateCache::CheckPipelineStateInCache(ShaderRHI);
						PSOCollectorStats::CheckComputePipelineStateInCache(*ShaderRHI, PSOPrecacheResult, RasterizerPass.ComputeMaterialProxy, PSOCollectorIndex);
					#endif

						if (GRHISupportsShaderRootConstants)
						{
							RHICmdList.SetComputeShaderRootConstants(Parameters.PassData);
						}

						SetShaderParametersMixedCS(
							RHICmdList,
							*ComputeShader,
							Parameters,
							ViewInfo,
							RasterizerPass.ComputeMaterialProxy,
							*RasterizerPass.ComputeMaterial
						);

						RHICmdList.DispatchIndirectComputeShader(IndirectArgsBuffer, RasterizerPass.IndirectOffset);
						UnsetShaderUAVs(RHICmdList, *ComputeShader, ShaderRHI);
					}
				}
			}
		}
	};

private:
	FRDGBuilder&									GraphBuilder;
	const FScene&									Scene;
	const FViewInfo&								SceneView;
	TRDGUniformBufferRef<FSceneUniformParameters>	SceneUniformBuffer;
	const FSharedContext&							SharedContext;
	const FRasterContext&							RasterContext;
	FVirtualShadowMapArray*							VirtualShadowMapArray;

	FConfiguration	Configuration;
	uint32			DrawPassIndex		= 0;
	uint32			RenderFlags			= 0;
	uint32			DebugFlags			= 0;
	uint32			NumInstancesPreCull;
	bool			bMultiView			= false;

	TRefCountPtr<IPooledRenderTarget> PrevHZB; // If non-null, HZB culling is enabled
	FIntRect		HZBBuildViewRect;

	FIntVector4		PageConstants;

	FRDGBufferRef	MainRasterizeArgsSWHW		= nullptr;
	FRDGBufferRef	PostRasterizeArgsSWHW		= nullptr;

	FRDGBufferRef	SafeMainRasterizeArgsSWHW	= nullptr;
	FRDGBufferRef	SafePostRasterizeArgsSWHW	= nullptr;

	FRDGBufferRef	ClusterCountSWHW			= nullptr;
	FRDGBufferRef	ClusterClassifyArgs			= nullptr;

	FRDGBufferRef	QueueState					= nullptr;
	FRDGBufferRef	VisibleClustersSWHW			= nullptr;
	FRDGBufferRef	OccludedInstances			= nullptr;
	FRDGBufferRef	OccludedInstancesArgs		= nullptr;
	FRDGBufferRef	TotalPrevDrawClustersBuffer	= nullptr;
	FRDGBufferRef	StreamingRequests			= nullptr;
	FRDGBufferRef	ViewsBuffer					= nullptr;
	FRDGBufferRef	InstanceDrawsBuffer			= nullptr;
	FRDGBufferRef	PrimitiveFilterBuffer		= nullptr;
	FRDGBufferRef	HiddenPrimitivesBuffer		= nullptr;
	FRDGBufferRef	ShowOnlyPrimitivesBuffer	= nullptr;
	FRDGBufferRef	RasterBinMetaBuffer			= nullptr;

	FRDGBufferRef	MainAndPostNodesAndClusterBatchesBuffer	= nullptr;
	FRDGBufferRef	MainAndPostCandidateClustersBuffer	= nullptr;

	FRDGBufferRef	ClusterIndirectArgsBuffer	= nullptr;
	FRDGBufferRef	ClusterStatsBuffer			= nullptr;

	FRDGBufferRef	 StatsBuffer				= nullptr;
	FRDGBufferUAVRef StatsBufferSkipBarrierUAV	= nullptr;

	FCullingParameters			CullingParameters;
	FVirtualTargetParameters	VirtualTargetParameters;
	FInstanceHierarchyDriver	InstanceHierarchyDriver;

	void PrepareRasterizerPasses(
		FDispatchContext& Context,
		const ERasterHardwarePath HardwarePath,
		const ERHIFeatureLevel::Type FeatureLevel,
		const FNaniteRasterPipelines& RasterPipelines,
		const FNaniteVisibilityQuery* VisibilityQuery,
		bool bCustomPass,
		bool bLumenCapture
	);

	void		AddPass_PrimitiveFilter();

	void		AddPass_NodeAndClusterCull(
		FRDGEventName&& PassName,
		const FNodeAndClusterCullSharedParameters& SharedParameters,
		FRDGBufferRef CurrentIndirectArgs,
		FRDGBufferRef NextIndirectArgs,
		uint32 NodeLevel,
		uint32 CullingPass,
		uint32 CullingType
	);
	void		AddPass_NodeAndClusterCull( uint32 CullingPass );
	void		AddPass_InstanceHierarchyAndClusterCull( uint32 CullingPass );
	
	FBinningData	AddPass_Binning(
		const FDispatchContext& DispatchContext,
		const ERasterHardwarePath HardwarePath,
		FRDGBufferRef ClusterOffsetSWHW,
		FRDGBufferRef VisiblePatches,
		FRDGBufferRef VisiblePatchesArgs,
		const FGlobalWorkQueueParameters& SplitWorkQueue,
		bool bMainPass,
		ERDGPassFlags PassFlags
	);

	FBinningData	AddPass_Rasterize(
		const FDispatchContext& DispatchContext,
		FRDGBufferRef IndirectArgs,
		FRDGBufferRef VisiblePatches,
		FRDGBufferRef VisiblePatchesArgs,
		const FGlobalWorkQueueParameters& SplitWorkQueue,
		const FGlobalWorkQueueParameters& OccludedPatches,
		bool bMainPass );

	void			AddPass_PatchSplit(
		const FDispatchContext& DispatchContext,
		const FGlobalWorkQueueParameters& SplitWorkQueue,
		const FGlobalWorkQueueParameters& OccludedPatches,
		FRDGBufferRef VisiblePatches,
		FRDGBufferRef VisiblePatchesArgs,
		uint32 CullingPass,
		ERDGPassFlags PassFlags);

	virtual void	DrawGeometry(
		FNaniteRasterPipelines& RasterPipelines,
		const FNaniteVisibilityQuery* VisibilityQuery,
		FRDGBufferRef ViewsArray,
		FRDGBufferRef InViewDrawRanges,
		int32 NumViews,
		FSceneInstanceCullingQuery* SceneInstanceCullingQuery,
		const TConstArrayView<FInstanceDraw>* OptionalInstanceDraws );

	virtual void	DrawGeometry(
		FNaniteRasterPipelines& RasterPipelines,
		const FNaniteVisibilityQuery* VisibilityQuery,
		const FPackedViewArray& ViewArray,
		FSceneInstanceCullingQuery* OptionalSceneInstanceCullingQuery,
		const TConstArrayView<FInstanceDraw>* OptionalInstanceDraws );

	void CalculateClusterIndirectArgsBuffer();

	void ExtractVSMPerformanceFeedback();

	void			ExtractStats( const FBinningData& MainPassBinning, const FBinningData& PostPassBinning );
	void			FeedbackStatus();
	virtual void	ExtractResults( FRasterResults& RasterResults );
	
	inline bool IsUsingVirtualShadowMap() const { return VirtualShadowMapArray != nullptr; }

	inline bool IsMaterialCache() const { return RenderFlags & NANITE_RENDER_FLAG_IS_MATERIAL_CACHE; }

	inline bool IsDebuggingEnabled() const
	{
		return DebugFlags != 0 || (RenderFlags & NANITE_RENDER_FLAG_WRITE_STATS) != 0u;
	}
};

TUniquePtr< IRenderer > IRenderer::Create(
	FRDGBuilder&			GraphBuilder,
	const FScene&			Scene,
	const FViewInfo&		SceneView,
	FSceneUniformBuffer&	SceneUniformBuffer,
	const FSharedContext&	SharedContext,
	const FRasterContext&	RasterContext,
	const FConfiguration&	Configuration,
	const FIntRect&			ViewRect,
	const TRefCountPtr<IPooledRenderTarget>& PrevHZB,
	FVirtualShadowMapArray*	VirtualShadowMapArray )
{
	return MakeUnique< FRenderer >(
		GraphBuilder,
		Scene,
		SceneView,
		SceneUniformBuffer.GetBuffer(GraphBuilder),
		SharedContext,
		RasterContext,
		Configuration,
		ViewRect,
		PrevHZB,
		VirtualShadowMapArray );
}

FRenderer::FRenderer(
	FRDGBuilder&			InGraphBuilder,
	const FScene&			InScene,
	const FViewInfo&		InSceneView,
	const TRDGUniformBufferRef<FSceneUniformParameters>& InSceneUniformBuffer,
	const FSharedContext&	InSharedContext,
	const FRasterContext&	InRasterContext,
	const FConfiguration&	InConfiguration,
	const FIntRect&			InViewRect,
	const TRefCountPtr<IPooledRenderTarget>& InPrevHZB,
	FVirtualShadowMapArray*	InVirtualShadowMapArray
)
	: GraphBuilder( InGraphBuilder )
	, Scene( InScene )
	, SceneView( InSceneView )
	, SceneUniformBuffer( InSceneUniformBuffer )
	, SharedContext( InSharedContext )
	, RasterContext( InRasterContext )
	, VirtualShadowMapArray( InVirtualShadowMapArray )
	, Configuration( InConfiguration )
	, PrevHZB( InPrevHZB )
	, HZBBuildViewRect( InViewRect )
{
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::InitContext");

	INC_DWORD_STAT(STAT_NaniteCullingContexts);

	const EShaderPlatform ShaderPlatform = Scene.GetShaderPlatform();

	// Disable two pass occlusion if previous HZB is invalid
	if (PrevHZB == nullptr || GNaniteCullingTwoPass == 0)
	{
		Configuration.bTwoPassOcclusion = false;
	}

	if (RasterContext.RasterScheduling == ERasterScheduling::HardwareOnly)
	{
		// Force HW Rasterization in the culling config if the RasterConfig is HardwareOnly
		Configuration.bForceHWRaster = true;
	}

	if (CVarNaniteProgrammableRaster.GetValueOnRenderThread() == 0)
	{
		Configuration.bDisableProgrammable = true;
	}

	RenderFlags |= Configuration.bDisableProgrammable	? NANITE_RENDER_FLAG_DISABLE_PROGRAMMABLE : 0u;
	RenderFlags |= Configuration.bForceHWRaster			? NANITE_RENDER_FLAG_FORCE_HW_RASTER : 0u;
	RenderFlags |= Configuration.bUpdateStreaming		? NANITE_RENDER_FLAG_OUTPUT_STREAMING_REQUESTS : 0u;
	RenderFlags |= Configuration.bIsShadowPass			? NANITE_RENDER_FLAG_IS_SHADOW_PASS : 0u;
	RenderFlags |= Configuration.bIsSceneCapture		? NANITE_RENDER_FLAG_IS_SCENE_CAPTURE : 0u;
	RenderFlags |= Configuration.bIsReflectionCapture	? NANITE_RENDER_FLAG_IS_REFLECTION_CAPTURE : 0u;
	RenderFlags |= Configuration.bIsLumenCapture		? NANITE_RENDER_FLAG_IS_LUMEN_CAPTURE : 0u;
	RenderFlags |= Configuration.bIsMaterialCache       ? NANITE_RENDER_FLAG_IS_MATERIAL_CACHE : 0u;
	RenderFlags |= Configuration.bIsGameView			? NANITE_RENDER_FLAG_IS_GAME_VIEW : 0u;
	RenderFlags |= Configuration.bGameShowFlag			? NANITE_RENDER_FLAG_GAME_SHOW_FLAG_ENABLED : 0u;
#if WITH_EDITOR
	RenderFlags |= Configuration.bEditorShowFlag		? NANITE_RENDER_FLAG_EDITOR_SHOW_FLAG_ENABLED : 0u;
#endif
	RenderFlags |= GNaniteShowStats != 0				? NANITE_RENDER_FLAG_WRITE_STATS : 0u;

	if (UseMeshShader(ShaderPlatform, SharedContext.Pipeline))
	{
		RenderFlags |= NANITE_RENDER_FLAG_MESH_SHADER;
	}
	else if (UsePrimitiveShader())
	{
		RenderFlags |= NANITE_RENDER_FLAG_PRIMITIVE_SHADER;
	}

	if (CVarNaniteVSMInvalidateOnLODDelta.GetValueOnRenderThread() != 0)
	{
		RenderFlags |= NANITE_RENDER_FLAG_INVALIDATE_VSM_ON_LOD_DELTA;
	}

	// TODO: Exclude from shipping builds
	{
		if (CVarNaniteCullingFrustum.GetValueOnRenderThread() == 0)
		{
			DebugFlags |= NANITE_DEBUG_FLAG_DISABLE_CULL_FRUSTUM;
		}

		if (CVarNaniteCullingHZB.GetValueOnRenderThread() == 0)
		{
			DebugFlags |= NANITE_DEBUG_FLAG_DISABLE_CULL_HZB;
		}

		if (CVarNaniteCullingGlobalClipPlane.GetValueOnRenderThread() == 0)
		{
			DebugFlags |= NANITE_DEBUG_FLAG_DISABLE_CULL_GLOBAL_CLIP_PLANE;
		}

		if (CVarNaniteCullingDrawDistance.GetValueOnRenderThread() == 0)
		{
			DebugFlags |= NANITE_DEBUG_FLAG_DISABLE_CULL_DRAW_DISTANCE;
		}

		if (CVarNaniteCullingWPODisableDistance.GetValueOnRenderThread() == 0)
		{
			DebugFlags |= NANITE_DEBUG_FLAG_DISABLE_WPO_DISABLE_DISTANCE;
		}

		if (CVarNaniteCullingShowAssemblyParts.GetValueOnRenderThread() == 0)
		{
			DebugFlags |= NANITE_DEBUG_FLAG_HIDE_ASSEMBLY_PARTS;
		}

		if (Configuration.bDrawOnlyRootGeometry)
		{
			DebugFlags |= NANITE_DEBUG_FLAG_DRAW_ONLY_ROOT_DATA;
		}
	}

	// TODO: Might this not break if the view has overridden the InstanceSceneData?
	const uint32 NumSceneInstancesPo2 = 
		uint32(CVarNaniteOccludedInstancesBufferSizeMultiplier.GetValueOnRenderThread() *
			   FMath::RoundUpToPowerOfTwo(FMath::Max(1024u * 128u, Scene.GPUScene.GetInstanceIdUpperBoundGPU())));

	const uint32 VisibleClusterSize = NANITE_MAX_VISIBLE_CLUSTER_SIZE_DWORDS * 4;
	
	PageConstants.X					= 0;
	PageConstants.Y					= Nanite::GStreamingManager.GetMaxStreamingPages();
	
	QueueState						= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateStructuredDesc( (6*2 + 1) * sizeof(uint32), 1), TEXT("Nanite.QueueState"));

	VisibleClustersSWHW				= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc(VisibleClusterSize * Nanite::FGlobalResources::GetMaxVisibleClusters()), TEXT("Nanite.VisibleClustersSWHW"));
	MainRasterizeArgsSWHW			= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(NANITE_RASTERIZER_ARG_COUNT), TEXT("Nanite.MainRasterizeArgsSWHW"));
	SafeMainRasterizeArgsSWHW		= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(NANITE_RASTERIZER_ARG_COUNT), TEXT("Nanite.SafeMainRasterizeArgsSWHW"));
	
	if (Configuration.bTwoPassOcclusion)
	{
		OccludedInstances			= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FInstanceDraw), NumSceneInstancesPo2), TEXT("Nanite.OccludedInstances"));
		OccludedInstancesArgs		= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(4), TEXT("Nanite.OccludedInstancesArgs"));
		PostRasterizeArgsSWHW		= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(NANITE_RASTERIZER_ARG_COUNT), TEXT("Nanite.PostRasterizeArgsSWHW"));
		SafePostRasterizeArgsSWHW	= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(NANITE_RASTERIZER_ARG_COUNT), TEXT("Nanite.SafePostRasterizeArgsSWHW"));
	}

	ClusterCountSWHW				= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FUintVector2), 1), TEXT("Nanite.SWHWClusterCount"));
	ClusterClassifyArgs				= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(), TEXT("Nanite.ClusterClassifyArgs"));

	StreamingRequests = Nanite::GStreamingManager.GetStreamingRequestsBuffer(GraphBuilder);
	
	if (Configuration.bSupportsMultiplePasses)
	{
		TotalPrevDrawClustersBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(8, 1), TEXT("Nanite.TotalPrevDrawClustersBuffer"));
	}
}

void FRenderer::AddPass_PrimitiveFilter()
{
	LLM_SCOPE_BYTAG(Nanite);
	
	const uint32 PrimitiveCount = uint32(Scene.GetMaxPersistentPrimitiveIndex());
	const bool bHLODActive = Scene.SceneLODHierarchy.IsActive();
	const uint32 HiddenHLODPrimitiveCount = bHLODActive && SceneView.ViewState ? SceneView.ViewState->HLODVisibilityState.ForcedHiddenPrimitiveMap.CountSetBits() : 0;
	const uint32 HiddenPrimitiveCount = SceneView.HiddenPrimitives.Num() + HiddenHLODPrimitiveCount;
	const uint32 ShowOnlyPrimitiveCount = SceneView.ShowOnlyPrimitives.IsSet() ? SceneView.ShowOnlyPrimitives->Num() : 0u;
	
	EFilterFlags HiddenFilterFlags = Configuration.HiddenFilterFlags;
	
	if (!SceneView.Family->EngineShowFlags.StaticMeshes)
	{
		HiddenFilterFlags |= EFilterFlags::StaticMesh;
	}

	if (!SceneView.Family->EngineShowFlags.InstancedStaticMeshes)
	{
		HiddenFilterFlags |= EFilterFlags::InstancedStaticMesh;
	}

	if (!SceneView.Family->EngineShowFlags.InstancedFoliage)
	{
		HiddenFilterFlags |= EFilterFlags::Foliage;
	}

	if (!SceneView.Family->EngineShowFlags.InstancedGrass)
	{
		HiddenFilterFlags |= EFilterFlags::Grass;
	}

	if (!SceneView.Family->EngineShowFlags.Landscape)
	{
		HiddenFilterFlags |= EFilterFlags::Landscape;
	}

	const bool bAnyPrimitiveFilter = (HiddenPrimitiveCount + ShowOnlyPrimitiveCount) > 0;
	const bool bAnyFilterFlags = PrimitiveCount > 0 && HiddenFilterFlags != EFilterFlags::None;
	
	if (CVarNaniteFilterPrimitives.GetValueOnRenderThread() != 0 && (bAnyPrimitiveFilter || bAnyFilterFlags))
	{
		check(PrimitiveCount > 0);
		const uint32 DWordCount = FMath::DivideAndRoundUp(PrimitiveCount, 32u); // 32 primitive bits per uint32
		const uint32 PrimitiveFilterBufferElements = FMath::RoundUpToPowerOfTwo(DWordCount);

		PrimitiveFilterBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), PrimitiveFilterBufferElements), TEXT("Nanite.PrimitiveFilter"));

		// Zeroed initially to indicate "all primitives unfiltered / visible"
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PrimitiveFilterBuffer), 0);

		// Create buffer from "show only primitives" set
		if (ShowOnlyPrimitiveCount > 0)
		{
			TArray<uint32, SceneRenderingAllocator> ShowOnlyPrimitiveIds;
			ShowOnlyPrimitiveIds.Reserve(FMath::RoundUpToPowerOfTwo(ShowOnlyPrimitiveCount));

			const TSet<FPrimitiveComponentId>& ShowOnlyPrimitivesSet = SceneView.ShowOnlyPrimitives.GetValue();
			for (TSet<FPrimitiveComponentId>::TConstIterator It(ShowOnlyPrimitivesSet); It; ++It)
			{
				ShowOnlyPrimitiveIds.Add(It->PrimIDValue);
			}

			// Add extra entries to ensure the buffer is valid pow2 in size
			ShowOnlyPrimitiveIds.SetNumZeroed(FMath::RoundUpToPowerOfTwo(ShowOnlyPrimitiveCount));

			// Sort the buffer by ascending value so the GPU binary search works properly
			Algo::Sort(ShowOnlyPrimitiveIds);

			ShowOnlyPrimitivesBuffer = CreateUploadBuffer(
				GraphBuilder,
				TEXT("Nanite.ShowOnlyPrimitivesBuffer"),
				sizeof(uint32),
				ShowOnlyPrimitiveIds.Num(),
				ShowOnlyPrimitiveIds.GetData(),
				sizeof(uint32) * ShowOnlyPrimitiveIds.Num()
			);
		}

		// Create buffer from "hidden primitives" set
		if (HiddenPrimitiveCount > 0)
		{
			TArray<uint32, SceneRenderingAllocator> HiddenPrimitiveIds;
			HiddenPrimitiveIds.Reserve(FMath::RoundUpToPowerOfTwo(HiddenPrimitiveCount));

			for (TSet<FPrimitiveComponentId>::TConstIterator It(SceneView.HiddenPrimitives); It; ++It)
			{
				HiddenPrimitiveIds.Add(It->PrimIDValue);
			}

			// HLOD visibily state
			if (HiddenHLODPrimitiveCount > 0)
			{
				for (TConstSetBitIterator It(SceneView.ViewState->HLODVisibilityState.ForcedHiddenPrimitiveMap); It; ++It)
				{
					const int32 Index = It.GetIndex();
					const FPrimitiveComponentId& PrimitiveComponentId = Scene.PrimitiveComponentIds[Index];
					HiddenPrimitiveIds.Add(PrimitiveComponentId.PrimIDValue);
				}
			}

			// Add extra entries to ensure the buffer is valid pow2 in size
			HiddenPrimitiveIds.SetNumZeroed(FMath::RoundUpToPowerOfTwo(HiddenPrimitiveCount));

			// Sort the buffer by ascending value so the GPU binary search works properly
			Algo::Sort(HiddenPrimitiveIds);

			HiddenPrimitivesBuffer = CreateUploadBuffer(
				GraphBuilder,
				TEXT("Nanite.HiddenPrimitivesBuffer"),
				sizeof(uint32),
				HiddenPrimitiveIds.Num(),
				HiddenPrimitiveIds.GetData(),
				sizeof(uint32) * HiddenPrimitiveIds.Num()
			);
		}

		FPrimitiveFilter_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPrimitiveFilter_CS::FParameters>();

		PassParameters->NumPrimitives = PrimitiveCount;
		PassParameters->HiddenFilterFlags = uint32(HiddenFilterFlags);
		PassParameters->NumHiddenPrimitives = FMath::RoundUpToPowerOfTwo(HiddenPrimitiveCount);
		PassParameters->NumShowOnlyPrimitives = FMath::RoundUpToPowerOfTwo(ShowOnlyPrimitiveCount);
		PassParameters->Scene = SceneUniformBuffer;
		PassParameters->PrimitiveFilterBuffer = GraphBuilder.CreateUAV(PrimitiveFilterBuffer);

		if (HiddenPrimitivesBuffer != nullptr)
		{
			PassParameters->HiddenPrimitivesList = GraphBuilder.CreateSRV(HiddenPrimitivesBuffer, PF_R32_UINT);
		}

		if (ShowOnlyPrimitivesBuffer != nullptr)
		{
			PassParameters->ShowOnlyPrimitivesList = GraphBuilder.CreateSRV(ShowOnlyPrimitivesBuffer, PF_R32_UINT);
		}

		FPrimitiveFilter_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FPrimitiveFilter_CS::FHiddenPrimitivesListDim>(HiddenPrimitivesBuffer != nullptr);
		PermutationVector.Set<FPrimitiveFilter_CS::FShowOnlyPrimitivesListDim>(ShowOnlyPrimitivesBuffer != nullptr);

		auto ComputeShader = SharedContext.ShaderMap->GetShader<FPrimitiveFilter_CS>(PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("PrimitiveFilter"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCountWrapped(PrimitiveCount, 64)
		);
	}
}

void AddPass_InitClusterCullArgs(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FRDGEventName&& PassName,
	FRDGBufferUAVRef QueueStateUAV,
	FRDGBufferRef ClusterCullArgs,
	uint32 CullingPass
)
{
	FInitClusterCullArgs_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitClusterCullArgs_CS::FParameters >();

	PassParameters->OutQueueState			= QueueStateUAV;
	PassParameters->OutClusterCullArgs		= GraphBuilder.CreateUAV(ClusterCullArgs);
	PassParameters->MaxCandidateClusters	= Nanite::FGlobalResources::GetMaxCandidateClusters();
	PassParameters->InitIsPostPass			= (CullingPass == CULLING_PASS_OCCLUSION_POST) ? 1 : 0;

	auto ComputeShader = ShaderMap->GetShader<FInitClusterCullArgs_CS>();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		Forward<FRDGEventName>(PassName),
		ComputeShader,
		PassParameters,
		FIntVector(1, 1, 1)
	);
}

void AddPass_InitNodeCullArgs(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FRDGEventName&& PassName,
	FRDGBufferUAVRef QueueStateUAV,
	FRDGBufferRef NodeCullArgs0,
	FRDGBufferRef NodeCullArgs1,
	uint32 CullingPass
)
{
	FInitNodeCullArgs_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitNodeCullArgs_CS::FParameters >();

	PassParameters->OutQueueState			= QueueStateUAV;
	PassParameters->OutNodeCullArgs0		= GraphBuilder.CreateUAV(NodeCullArgs0);
	PassParameters->OutNodeCullArgs1		= GraphBuilder.CreateUAV(NodeCullArgs1);
	PassParameters->MaxNodes				= Nanite::FGlobalResources::GetMaxNodes();
	PassParameters->InitIsPostPass			= (CullingPass == CULLING_PASS_OCCLUSION_POST) ? 1 : 0;

	auto ComputeShader = ShaderMap->GetShader<FInitNodeCullArgs_CS>();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		Forward<FRDGEventName>(PassName),
		ComputeShader,
		PassParameters,
		FIntVector(2, 1, 1)
	);
}


void FRenderer::AddPass_NodeAndClusterCull(
	FRDGEventName&& PassName,
	const FNodeAndClusterCullSharedParameters& SharedParameters,
	FRDGBufferRef CurrentIndirectArgs,
	FRDGBufferRef NextIndirectArgs,
	uint32 NodeLevel,
	uint32 CullingPass,
	uint32 CullingType
	)
{
	FNodeAndClusterCull_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FNodeAndClusterCull_CS::FParameters >();
	PassParameters->SharedParameters	= SharedParameters;
	PassParameters->NodeLevel			= NodeLevel;
	
	FNodeAndClusterCull_CS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FNodeAndClusterCull_CS::FCullingPassDim>(CullingPass);
	PermutationVector.Set<FNodeAndClusterCull_CS::FMultiViewDim>(bMultiView);
	PermutationVector.Set<FNodeAndClusterCull_CS::FVirtualTextureTargetDim>(IsUsingVirtualShadowMap());
	PermutationVector.Set<FNodeAndClusterCull_CS::FMaterialCacheDim>(IsMaterialCache());
	PermutationVector.Set<FNodeAndClusterCull_CS::FSplineDeformDim>(NaniteSplineMeshesSupported()); // TODO: Nanite-Skinning - leverage this?
	PermutationVector.Set<FNodeAndClusterCull_CS::FDebugFlagsDim>(IsDebuggingEnabled());
	PermutationVector.Set<FNodeAndClusterCull_CS::FCullingTypeDim>(CullingType);
	auto ComputeShader = SharedContext.ShaderMap->GetShader<FNodeAndClusterCull_CS>(PermutationVector);

	if (CullingType == NANITE_CULLING_TYPE_NODES || CullingType == NANITE_CULLING_TYPE_CLUSTERS)
	{
		if (CullingType == NANITE_CULLING_TYPE_NODES)
		{
			PassParameters->CurrentNodeIndirectArgs = GraphBuilder.CreateSRV(CurrentIndirectArgs);
			PassParameters->NextNodeIndirectArgs = GraphBuilder.CreateUAV(NextIndirectArgs);
		}
		
		PassParameters->IndirectArgs = CurrentIndirectArgs;
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			Forward<FRDGEventName>(PassName),
			ComputeShader,
			PassParameters,
			CurrentIndirectArgs,
			NodeLevel * NANITE_NODE_CULLING_ARG_COUNT * sizeof(uint32)
		);
	}
	else if(CullingType == NANITE_CULLING_TYPE_PERSISTENT_NODES_AND_CLUSTERS)
	{
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			Forward<FRDGEventName>(PassName),
			ComputeShader,
			PassParameters,
			FIntVector(GRHIPersistentThreadGroupCount, 1, 1)
		);
	}
	else
	{
		checkf(false, TEXT("Unknown culling type: %d"), CullingType);
	}
}

void FRenderer::AddPass_NodeAndClusterCull( uint32 CullingPass )
{
	FNodeAndClusterCullSharedParameters SharedParameters;
	SharedParameters.Scene = SceneUniformBuffer;
	SharedParameters.CullingParameters = CullingParameters;
	SharedParameters.MaxNodes = Nanite::FGlobalResources::GetMaxNodes();
	SharedParameters.ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	SharedParameters.HierarchyBuffer = Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);

	check(DrawPassIndex == 0 || RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA); // sanity check
	if (RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA)
	{
		SharedParameters.InTotalPrevDrawClusters = GraphBuilder.CreateSRV(TotalPrevDrawClustersBuffer);
	}
	else
	{
		FRDGBufferRef Dummy = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 8);
		SharedParameters.InTotalPrevDrawClusters = GraphBuilder.CreateSRV(Dummy);
	}

	SharedParameters.QueueState = GraphBuilder.CreateUAV(QueueState);
	SharedParameters.MainAndPostNodesAndClusterBatches = GraphBuilder.CreateUAV(MainAndPostNodesAndClusterBatchesBuffer);
	SharedParameters.MainAndPostCandidateClusters = GraphBuilder.CreateUAV(MainAndPostCandidateClustersBuffer);

	if (CullingPass == CULLING_PASS_NO_OCCLUSION || CullingPass == CULLING_PASS_OCCLUSION_MAIN)
	{
		SharedParameters.VisibleClustersArgsSWHW = GraphBuilder.CreateUAV(MainRasterizeArgsSWHW);
	}
	else
	{
		SharedParameters.OffsetClustersArgsSWHW = GraphBuilder.CreateSRV(MainRasterizeArgsSWHW);
		SharedParameters.VisibleClustersArgsSWHW = GraphBuilder.CreateUAV(PostRasterizeArgsSWHW);
	}

	SharedParameters.OutVisibleClustersSWHW = GraphBuilder.CreateUAV(VisibleClustersSWHW);
	SharedParameters.OutStreamingRequests = GraphBuilder.CreateUAV(StreamingRequests);

	SharedParameters.VirtualShadowMap = VirtualTargetParameters;

	if (StatsBuffer)
	{
		SharedParameters.OutStatsBuffer = StatsBufferSkipBarrierUAV;
	}

	SharedParameters.LargePageRectThreshold = CVarLargePageRectThreshold.GetValueOnRenderThread();
	SharedParameters.StreamingRequestsBufferVersion = GStreamingManager.GetStreamingRequestsBufferVersion();
	SharedParameters.StreamingRequestsBufferSize = StreamingRequests->Desc.NumElements;
	SharedParameters.DepthBucketsMinZ = CVarNaniteDepthBucketsMinZ.GetValueOnRenderThread();
	SharedParameters.DepthBucketsMaxZ = CVarNaniteDepthBucketsMaxZ.GetValueOnRenderThread();

	check(ViewsBuffer);

	if (CVarNanitePersistentThreadsCulling.GetValueOnRenderThread())
	{
		AddPass_NodeAndClusterCull(
			RDG_EVENT_NAME("PersistentCull"),
			SharedParameters,
			nullptr,
			nullptr,
			0u,
			CullingPass,
			NANITE_CULLING_TYPE_PERSISTENT_NODES_AND_CLUSTERS);
	}
	else
	{
		RDG_EVENT_SCOPE(GraphBuilder, "NodeAndClusterCull");

		
		// Ping-pong between two sets of indirect args to get around that indirect args resource state is read-only.
		FRDGBufferRef NodeCullArgs0 = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc((NANITE_MAX_CLUSTER_HIERARCHY_DEPTH + 1) * NANITE_NODE_CULLING_ARG_COUNT), TEXT("Nanite.CullArgs0"));
		FRDGBufferRef NodeCullArgs1 = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc((NANITE_MAX_CLUSTER_HIERARCHY_DEPTH + 1) * NANITE_NODE_CULLING_ARG_COUNT), TEXT("Nanite.CullArgs1"));

		FRDGBufferUAVRef QueueStateUAV = GraphBuilder.CreateUAV(QueueState);

		AddPass_InitNodeCullArgs(GraphBuilder, SharedContext.ShaderMap, RDG_EVENT_NAME("InitNodeCullArgs"), QueueStateUAV, NodeCullArgs0, NodeCullArgs1, CullingPass);

		const uint32 MaxLevels = Nanite::GStreamingManager.GetMaxHierarchyLevels();
		for (uint32 NodeLevel = 0; NodeLevel < MaxLevels; NodeLevel++)
		{
			AddPass_NodeAndClusterCull(
				RDG_EVENT_NAME("NodeCull_%d", NodeLevel),
				SharedParameters,
				(NodeLevel & 1) ? NodeCullArgs1 : NodeCullArgs0,
				(NodeLevel & 1) ? NodeCullArgs0 : NodeCullArgs1,
				NodeLevel,
				CullingPass,
				NANITE_CULLING_TYPE_NODES);
		}

		FRDGBufferRef ClusterCullArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(3), TEXT("Nanite.ClusterCullArgs"));
		AddPass_InitClusterCullArgs(GraphBuilder, SharedContext.ShaderMap, RDG_EVENT_NAME("InitClusterCullArgs"), QueueStateUAV, ClusterCullArgs, CullingPass);

		AddPass_NodeAndClusterCull(
			RDG_EVENT_NAME("ClusterCull"),
			SharedParameters,
			ClusterCullArgs,
			nullptr,
			0,
			CullingPass,
			NANITE_CULLING_TYPE_CLUSTERS);
	}
}

void FRenderer::AddPass_InstanceHierarchyAndClusterCull( uint32 CullingPass )
{
	LLM_SCOPE_BYTAG(Nanite);

	checkf(GRHIPersistentThreadGroupCount > 0, TEXT("GRHIPersistentThreadGroupCount must be configured correctly in the RHI."));

	FRDGBufferRef Dummy = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 8);

	{
		RDG_EVENT_SCOPE(GraphBuilder, "InstanceCulling");

	FInstanceWorkGroupParameters InstanceWorkGroupParameters;
	// Run hierarchical instance culling pass
	if (InstanceHierarchyDriver.IsEnabled())
	{
		InstanceWorkGroupParameters = InstanceHierarchyDriver.DispatchCullingPass(GraphBuilder, CullingPass, *this);
	}

		// make sure the passes can overlap
		FRDGBufferUAVRef QueueStateSkipBarrierUAV = GraphBuilder.CreateUAV( QueueState, ERDGUnorderedAccessViewFlags::SkipBarrier );
		FRDGBufferUAVRef MainAndPostNodesAndClusterBatcheUAV = GraphBuilder.CreateUAV( MainAndPostNodesAndClusterBatchesBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier );
		FRDGBufferUAVRef OccludedInstancesSkipBarrierUAV = nullptr;
		FRDGBufferUAVRef OccludedInstancesArgsSkipBarrierUAV = nullptr;
	
		if (CullingPass == CULLING_PASS_OCCLUSION_MAIN)
		{
			OccludedInstancesSkipBarrierUAV = GraphBuilder.CreateUAV( OccludedInstances, ERDGUnorderedAccessViewFlags::SkipBarrier );
			OccludedInstancesArgsSkipBarrierUAV = GraphBuilder.CreateUAV( OccludedInstancesArgs, ERDGUnorderedAccessViewFlags::SkipBarrier );
		}

		auto DispatchInstanceCullPass = [&](const FInstanceWorkGroupParameters &InstanceWorkGroupParameters)
		{
			FInstanceCull_CS::FParameters SharedParameters;

			SharedParameters.NumInstances						= NumInstancesPreCull;
			SharedParameters.MaxNodes							= Nanite::FGlobalResources::GetMaxNodes();
			SharedParameters.ImposterMaxPixels					= CVarNaniteImposterMaxPixels.GetValueOnRenderThread();

			SharedParameters.Scene = SceneUniformBuffer;
			SharedParameters.RasterParameters = RasterContext.Parameters;
			SharedParameters.CullingParameters = CullingParameters;

			SharedParameters.ImposterAtlas = Nanite::GStreamingManager.GetImposterDataSRV(GraphBuilder);

			SharedParameters.OutQueueState = QueueStateSkipBarrierUAV;

			SharedParameters.VirtualShadowMap = VirtualTargetParameters;

			if (StatsBuffer)
			{
				SharedParameters.OutStatsBuffer					= StatsBufferSkipBarrierUAV;
			}

			SharedParameters.OutMainAndPostNodesAndClusterBatches = MainAndPostNodesAndClusterBatcheUAV;
			if (CullingPass == CULLING_PASS_NO_OCCLUSION)
			{
				if( InstanceDrawsBuffer )
				{
					SharedParameters.InInstanceDraws			= GraphBuilder.CreateSRV( InstanceDrawsBuffer );
				}
			}
			else if (CullingPass == CULLING_PASS_OCCLUSION_MAIN)
			{
				SharedParameters.OutOccludedInstances		= OccludedInstancesSkipBarrierUAV;
				SharedParameters.OutOccludedInstancesArgs	= OccludedInstancesArgsSkipBarrierUAV;
			}
			else if (!IsValid(InstanceWorkGroupParameters))
			{
				SharedParameters.InInstanceDraws				= GraphBuilder.CreateSRV( OccludedInstances );
				SharedParameters.InOccludedInstancesArgs		= GraphBuilder.CreateSRV( OccludedInstancesArgs );
			}

			SharedParameters.InstanceWorkGroupParameters = InstanceWorkGroupParameters;

			if (PrimitiveFilterBuffer)
			{
				SharedParameters.InPrimitiveFilterBuffer		= GraphBuilder.CreateSRV(PrimitiveFilterBuffer);
			}

			check(ViewsBuffer);
			const bool bUseExplicitListCullingPass = InstanceDrawsBuffer != nullptr;
			const uint32 InstanceCullingPass = bUseExplicitListCullingPass ? CULLING_PASS_EXPLICIT_LIST : CullingPass;
			FInstanceCull_CS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FInstanceCull_CS::FCullingPassDim>(InstanceCullingPass);
			PermutationVector.Set<FInstanceCull_CS::FMultiViewDim>(bMultiView);
			PermutationVector.Set<FInstanceCull_CS::FPrimitiveFilterDim>(PrimitiveFilterBuffer != nullptr);
			PermutationVector.Set<FInstanceCull_CS::FDebugFlagsDim>(IsDebuggingEnabled());
			PermutationVector.Set<FInstanceCull_CS::FDepthOnlyDim>(RasterContext.RasterMode == EOutputBufferMode::DepthOnly);
			// Make sure these permutations are orthogonally enabled WRT CULLING_PASS_EXPLICIT_LIST as they can never co-exist
			check(!(IsUsingVirtualShadowMap() && bUseExplicitListCullingPass));
			check(!(IsValid(InstanceWorkGroupParameters) && bUseExplicitListCullingPass));
			PermutationVector.Set<FInstanceCull_CS::FVirtualTextureTargetDim>(IsUsingVirtualShadowMap() && !bUseExplicitListCullingPass);
			PermutationVector.Set<FInstanceCull_CS::FMaterialCacheDim>(IsMaterialCache());
			bool bGroupWorkBuffer = IsValid(InstanceWorkGroupParameters) && !bUseExplicitListCullingPass;
			PermutationVector.Set<FInstanceCull_CS::FUseGroupWorkBufferDim>(bGroupWorkBuffer);

			if (bGroupWorkBuffer)
			{
				FInstanceCull_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInstanceCull_CS::FParameters >(&SharedParameters);
				PassParameters->IndirectArgs = InstanceWorkGroupParameters.InInstanceWorkArgs->GetParent();

				// Get the general (not specialized for static) CS and use that to clear any unused graph resources. There is no difference between the permutations.
				PermutationVector.Set<FInstanceCull_CS::FStaticGeoDim>(false);
				auto GeneralComputeShader = SharedContext.ShaderMap->GetShader<FInstanceCull_CS>(PermutationVector);
				PermutationVector.Set<FInstanceCull_CS::FStaticGeoDim>(true);
				auto StaticComputeShader = SharedContext.ShaderMap->GetShader<FInstanceCull_CS>(PermutationVector);
				ClearUnusedGraphResources(GeneralComputeShader, PassParameters);

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("InstanceCull - GroupWork"),
					PassParameters,
					ERDGPassFlags::Compute,
					[DeferredSetupContext = InstanceHierarchyDriver.DeferredSetupContext, 
					bAllowStaticGeometryPath = InstanceHierarchyDriver.bAllowStaticGeometryPath,
					PassParameters, 
					PermutationVector, 
					GeneralComputeShader, 
					StaticComputeShader](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList) mutable
					{
						PassParameters->MaxInstanceWorkGroups = DeferredSetupContext->GetMaxInstanceWorkGroups();

						// always run the general path, everything gets funneled here if the static path is disabled
						FComputeShaderUtils::DispatchIndirect(RHICmdList, GeneralComputeShader, *PassParameters, PassParameters->IndirectArgs->GetIndirectRHICallBuffer(), 4 * sizeof(uint32));

						// Run the static dispatch after to bias the more expensive clusters to the start of the queue.
						if (bAllowStaticGeometryPath)
						{
							FComputeShaderUtils::DispatchIndirect(RHICmdList, StaticComputeShader, *PassParameters, PassParameters->IndirectArgs->GetIndirectRHICallBuffer(), 0);
						}
					});
			}
			else 
			{
				auto ComputeShader = SharedContext.ShaderMap->GetShader<FInstanceCull_CS>(PermutationVector);
				FInstanceCull_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInstanceCull_CS::FParameters >(&SharedParameters);
				if (InstanceCullingPass == CULLING_PASS_OCCLUSION_POST)
				{
					PassParameters->IndirectArgs = OccludedInstancesArgs;
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME( "InstanceCull" ),
						ComputeShader,
						PassParameters,
						PassParameters->IndirectArgs,
						0
					);
				}
				else
				{
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						InstanceCullingPass == CULLING_PASS_EXPLICIT_LIST ? RDG_EVENT_NAME("InstanceCull - Explicit List") : RDG_EVENT_NAME("InstanceCull"),
						ComputeShader,
						PassParameters,
						FComputeShaderUtils::GetGroupCountWrapped(NumInstancesPreCull, 64)
					);
				}
			}
		};
		// We need to add an extra pass to cover for the post-pass occluded instances, this is a workaround for an issue where the instances from 
		// pre-pass & hierarchy cull were not able to co-exist in the same args, for obscure reasons. We should perhaps re-merge them.
		if (CullingPass == CULLING_PASS_OCCLUSION_POST && IsValid(InstanceWorkGroupParameters))
		{
			static FInstanceWorkGroupParameters DummyInstanceWorkGroupParameters;
			DispatchInstanceCullPass(DummyInstanceWorkGroupParameters);
		}
		DispatchInstanceCullPass(InstanceWorkGroupParameters);
	}

	AddPass_NodeAndClusterCull( CullingPass );

	{
		FCalculateSafeRasterizerArgs_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FCalculateSafeRasterizerArgs_CS::FParameters >();

		const bool bPrevDrawData		= (RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA) != 0;
		const bool bPostPass			= (CullingPass == CULLING_PASS_OCCLUSION_POST) != 0;

		if (bPrevDrawData)
		{
			PassParameters->InTotalPrevDrawClusters		= GraphBuilder.CreateSRV(TotalPrevDrawClustersBuffer);
		}
		else
		{
			PassParameters->InTotalPrevDrawClusters		= GraphBuilder.CreateSRV(Dummy);
		}

		if (bPostPass)
		{
			PassParameters->OffsetClustersArgsSWHW		= GraphBuilder.CreateSRV(MainRasterizeArgsSWHW);
			PassParameters->InRasterizerArgsSWHW		= GraphBuilder.CreateSRV(PostRasterizeArgsSWHW);
			PassParameters->OutSafeRasterizerArgsSWHW	= GraphBuilder.CreateUAV(SafePostRasterizeArgsSWHW);
		}
		else
		{
			PassParameters->InRasterizerArgsSWHW		= GraphBuilder.CreateSRV(MainRasterizeArgsSWHW);
			PassParameters->OutSafeRasterizerArgsSWHW	= GraphBuilder.CreateUAV(SafeMainRasterizeArgsSWHW);
		}

		PassParameters->OutClusterCountSWHW				= GraphBuilder.CreateUAV(ClusterCountSWHW);
		PassParameters->OutClusterClassifyArgs			= GraphBuilder.CreateUAV(ClusterClassifyArgs);
		
		PassParameters->MaxVisibleClusters				= Nanite::FGlobalResources::GetMaxVisibleClusters();
		PassParameters->RenderFlags						= RenderFlags;
		
		FCalculateSafeRasterizerArgs_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCalculateSafeRasterizerArgs_CS::FIsPostPass>(bPostPass);

		auto ComputeShader = SharedContext.ShaderMap->GetShader< FCalculateSafeRasterizerArgs_CS >(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CalculateSafeRasterizerArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1)
		);
	}
}

static ENaniteMeshPass::Type GetMeshPass(const FConfiguration& Configuration)
{
	if (Configuration.bIsMaterialCache)
	{
		return ENaniteMeshPass::MaterialCache;
	}
	else if (Configuration.bIsLumenCapture)
	{
		return ENaniteMeshPass::LumenCardCapture;
	}
	else
	{
		return ENaniteMeshPass::BasePass;
	}
}
	
FBinningData FRenderer::AddPass_Binning(
	const FDispatchContext& DispatchContext,
	const ERasterHardwarePath HardwarePath,
	FRDGBufferRef ClusterOffsetSWHW,
	FRDGBufferRef VisiblePatches,
	FRDGBufferRef VisiblePatchesArgs,
	const FGlobalWorkQueueParameters& SplitWorkQueue,
	bool bMainPass,
	ERDGPassFlags PassFlags
)
{
	const EShaderPlatform ShaderPlatform = Scene.GetShaderPlatform();

	FBinningData BinningData = {};
	BinningData.BinCount = DispatchContext.MetaBufferData.Num();

	ENaniteMeshPass::Type MeshPass = GetMeshPass(Configuration);

	if (BinningData.BinCount > 0)
	{
		if ((RenderFlags & NANITE_RENDER_FLAG_WRITE_STATS) != 0u && StatsBuffer != nullptr)
		{
			BinningData.MetaBuffer = GraphBuilder.CreateBuffer(DispatchContext.MetaBuffer->Desc, DispatchContext.MetaBuffer->Name);
			AddCopyBufferPass(GraphBuilder, BinningData.MetaBuffer, DispatchContext.MetaBuffer);
		}
		else
		{
			BinningData.MetaBuffer = DispatchContext.MetaBuffer;
		}

		// Initialize Bin Ranges
		{
			FRasterBinInit_CS::FParameters* InitPassParameters = GraphBuilder.AllocParameters<FRasterBinInit_CS::FParameters>();
			InitPassParameters->OutRasterBinMeta = GraphBuilder.CreateUAV(BinningData.MetaBuffer);
			InitPassParameters->RasterBinCount = BinningData.BinCount;

			auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterBinInit_CS>();
			ClearUnusedGraphResources(ComputeShader, InitPassParameters);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RasterBinInit"),
				InitPassParameters,
				PassFlags,
				[InitPassParameters, &DispatchContext, VisiblePatches, ComputeShader, BinCount = BinningData.BinCount](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(
						RHICmdList,
						ComputeShader,
						*InitPassParameters,
						FComputeShaderUtils::GetGroupCountWrapped(BinCount, 64)
					);
				}
			);
		}

		BinningData.IndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(BinningData.BinCount * NANITE_RASTERIZER_ARG_COUNT), TEXT("Nanite.RasterBinIndirectArgs"));

		const uint32 MaxVisibleClusters = Nanite::FGlobalResources::GetMaxVisibleClusters();
		const uint32 MaxClusterIndirections = uint32(float(MaxVisibleClusters) * FMath::Max<float>(1.0f, CVarNaniteRasterIndirectionMultiplier.GetValueOnRenderThread()));
		check(MaxClusterIndirections > 0);
		BinningData.DataBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 2, MaxClusterIndirections), TEXT("Nanite.RasterBinData"));

		const bool bDepthBucketing = NANITE_DEPTH_BUCKETING && (CVarNaniteDepthBucketing.GetValueOnRenderThread() != 0) && (NANITE_VOXEL_DATA || CVarNaniteDepthBucketPixelProgrammable.GetValueOnRenderThread() != 0);
		
		FRasterBinBuild_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRasterBinBuild_CS::FParameters>();

		PassParameters->Scene					= SceneUniformBuffer;
		PassParameters->VisibleClustersSWHW		= GraphBuilder.CreateSRV(VisibleClustersSWHW);
		PassParameters->ClusterPageData			= GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		PassParameters->InClusterCountSWHW		= GraphBuilder.CreateSRV(ClusterCountSWHW);
		PassParameters->InClusterOffsetSWHW		= GraphBuilder.CreateSRV(ClusterOffsetSWHW, PF_R32_UINT);
		PassParameters->IndirectArgs			= VisiblePatchesArgs ? VisiblePatchesArgs : ClusterClassifyArgs;
		PassParameters->InTotalPrevDrawClusters	= GraphBuilder.CreateSRV(TotalPrevDrawClustersBuffer);
		PassParameters->OutRasterBinMeta		= GraphBuilder.CreateUAV(BinningData.MetaBuffer);

		FRDGBuffer* DepthBucketsBuffer = nullptr;
		if (bDepthBucketing)
		{
			//TODO: Can't use DispatchContext.NumDepthBlocks here because that is filled out in a RDG task. Is there some workaround, so we don't need to allocate and clear the full buffer?
			DepthBucketsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), BinningData.BinCount * 2 * NANITE_NUM_DEPTH_BUCKETS_PER_BLOCK), TEXT("Nanite.DepthBuckets"));
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DepthBucketsBuffer), 0);
			PassParameters->OutDepthBuckets	= GraphBuilder.CreateUAV(DepthBucketsBuffer);
		}

		if (VisiblePatches)
		{
			PassParameters->VisiblePatches		= GraphBuilder.CreateSRV(VisiblePatches);
			PassParameters->VisiblePatchesArgs	= GraphBuilder.CreateSRV(VisiblePatchesArgs);
			PassParameters->SplitWorkQueue		= SplitWorkQueue;
			PassParameters->MaxVisiblePatches	= FGlobalResources::GetMaxVisiblePatches();
		}

		PassParameters->PageConstants = PageConstants;
		PassParameters->RenderFlags = RenderFlags;
		PassParameters->MaxVisibleClusters = MaxVisibleClusters;
		PassParameters->RegularMaterialRasterBinCount = Scene.NaniteRasterPipelines[MeshPass].GetRegularBinCount();
		PassParameters->bUsePrimOrMeshShader = HardwarePath != ERasterHardwarePath::VertexShader;
		PassParameters->MaxPatchesPerGroup = GetMaxPatchesPerGroup();
		PassParameters->MeshPassIndex = MeshPass;
		PassParameters->MinSupportedWaveSize = GRHIMinimumWaveSize;
		PassParameters->MaxClusterIndirections = MaxClusterIndirections;

		// Count SW & HW Clusters
		{
			FRasterBinBuild_CS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRasterBinBuild_CS::FIsPostPass>(!bMainPass);
			PermutationVector.Set<FRasterBinBuild_CS::FPatches>(VisiblePatches != nullptr);
			PermutationVector.Set<FRasterBinBuild_CS::FVirtualTextureTargetDim>(VirtualShadowMapArray != nullptr);
			PermutationVector.Set<FRasterBinBuild_CS::FBuildPassDim>(NANITE_RASTER_BIN_COUNT);
			PermutationVector.Set<FRasterBinBuild_CS::FDepthBucketingDim>(bDepthBucketing);

			auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterBinBuild_CS>(PermutationVector);
			ClearUnusedGraphResources(ComputeShader, PassParameters);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RasterBinCount"),
				PassParameters,
				PassFlags,
				[PassParameters, &DispatchContext, VisiblePatches, ComputeShader](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
				{
					if (VisiblePatches == nullptr || DispatchContext.HasTessellated())
					{
						FComputeShaderUtils::DispatchIndirect(
							RHICmdList,
							ComputeShader,
							*PassParameters,
							PassParameters->IndirectArgs->GetIndirectRHICallBuffer(),
							0
						);
					}
				}
			);
		}

		// Reserve Bin Ranges
		{
			FRDGBufferRef RangeAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Nanite.RangeAllocatorBuffer"));
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(RangeAllocatorBuffer), 0);

			FRasterBinReserve_CS::FParameters* ReservePassParameters = GraphBuilder.AllocParameters<FRasterBinReserve_CS::FParameters>();
			ReservePassParameters->OutRasterBinArgsSWHW = GraphBuilder.CreateUAV(BinningData.IndirectArgs);
			ReservePassParameters->OutRasterBinMeta = GraphBuilder.CreateUAV(BinningData.MetaBuffer);
			ReservePassParameters->OutRangeAllocator = GraphBuilder.CreateUAV(RangeAllocatorBuffer);
			ReservePassParameters->RasterBinCount = BinningData.BinCount;
			ReservePassParameters->RenderFlags = RenderFlags;

			auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterBinReserve_CS>();
			ClearUnusedGraphResources(ComputeShader, ReservePassParameters);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RasterBinReserve"),
				ReservePassParameters,
				PassFlags,
				[ReservePassParameters, &DispatchContext, VisiblePatches, ComputeShader, BinCount = BinningData.BinCount](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
				{
					if (VisiblePatches == nullptr || DispatchContext.HasTessellated())
					{
						FComputeShaderUtils::Dispatch(
							RHICmdList,
							ComputeShader,
							*ReservePassParameters,
							FComputeShaderUtils::GetGroupCountWrapped(BinCount, 64)
						);
					}
				}
			);
		}

		PassParameters->OutRasterBinData = GraphBuilder.CreateUAV(BinningData.DataBuffer);
		PassParameters->OutRasterBinArgsSWHW = GraphBuilder.CreateUAV(BinningData.IndirectArgs);

		if(bDepthBucketing)
		{
			FRasterBinDepthBlock_CS::FParameters* DepthBlockPassParameters = GraphBuilder.AllocParameters<FRasterBinDepthBlock_CS::FParameters>();
			DepthBlockPassParameters->OutDepthBuckets = GraphBuilder.CreateUAV(DepthBucketsBuffer);

			auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterBinDepthBlock_CS>();
			ClearUnusedGraphResources(ComputeShader, DepthBlockPassParameters);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RasterBinDepthBlock"),
				DepthBlockPassParameters,
				PassFlags,
				[DepthBlockPassParameters, &DispatchContext, VisiblePatches, ComputeShader](FRHIComputeCommandList& RHICmdList)
				{
					if (VisiblePatches == nullptr || DispatchContext.HasTessellated())
					{
						FComputeShaderUtils::Dispatch(
							RHICmdList,
							ComputeShader,
							*DepthBlockPassParameters,
							FIntVector3(DispatchContext.NumDepthBlocks,1,1)
						);
					}
				}
			);
		}

		// Scatter SW & HW Clusters
		{
			PassParameters->OutRasterBinMeta = GraphBuilder.CreateUAV(BinningData.MetaBuffer);

			FRasterBinBuild_CS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRasterBinBuild_CS::FIsPostPass>(!bMainPass);
			PermutationVector.Set<FRasterBinBuild_CS::FPatches>(VisiblePatches != nullptr);
			PermutationVector.Set<FRasterBinBuild_CS::FVirtualTextureTargetDim>(VirtualShadowMapArray != nullptr);
			PermutationVector.Set<FRasterBinBuild_CS::FBuildPassDim>(NANITE_RASTER_BIN_SCATTER);
			PermutationVector.Set<FRasterBinBuild_CS::FDepthBucketingDim>(bDepthBucketing);

			auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterBinBuild_CS>(PermutationVector);
			ClearUnusedGraphResources(ComputeShader, PassParameters);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RasterBinScatter"),
				PassParameters,
				PassFlags,
				[PassParameters, &DispatchContext, VisiblePatches, ComputeShader](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
				{
					if (VisiblePatches == nullptr || DispatchContext.HasTessellated())
					{
						FComputeShaderUtils::DispatchIndirect(
							RHICmdList,
							ComputeShader,
							*PassParameters,
							PassParameters->IndirectArgs->GetIndirectRHICallBuffer(),
							0
						);
					}
				}
			);
		}

		// Finalize Bin Ranges
		if(VisiblePatches == nullptr)
		{
			const uint32 FinalizeMode = (HardwarePath == ERasterHardwarePath::MeshShaderWrapped) ? 0u :
										(HardwarePath == ERasterHardwarePath::MeshShaderNV) ? 1u :
										2u;

			FRasterBinFinalize_CS::FParameters* FinalizePassParameters = GraphBuilder.AllocParameters<FRasterBinFinalize_CS::FParameters>();
			FinalizePassParameters->OutRasterBinArgsSWHW = GraphBuilder.CreateUAV(BinningData.IndirectArgs);
			FinalizePassParameters->OutRasterBinMeta = GraphBuilder.CreateUAV(BinningData.MetaBuffer);
			FinalizePassParameters->RasterBinCount = BinningData.BinCount;
			FinalizePassParameters->FinalizeMode = FinalizeMode;
			FinalizePassParameters->RenderFlags = RenderFlags;
			FinalizePassParameters->MaxClusterIndirections = MaxClusterIndirections;

			auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterBinFinalize_CS>();
			ClearUnusedGraphResources(ComputeShader, FinalizePassParameters);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RasterBinFinalize"),
				FinalizePassParameters,
				PassFlags,
				[FinalizePassParameters, &DispatchContext, VisiblePatches, ComputeShader, BinCount = BinningData.BinCount](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
				{
					if (VisiblePatches == nullptr || DispatchContext.HasTessellated())
					{
						FComputeShaderUtils::Dispatch(
							RHICmdList,
							ComputeShader,
							*FinalizePassParameters,
							FComputeShaderUtils::GetGroupCountWrapped(BinCount, 64)
						);
					}
				}
			);
		}
	}

	return BinningData;
}

static bool UseRasterShaderBundleSW(EShaderPlatform Platform)
{
	return CVarNaniteBundleRaster.GetValueOnRenderThread() != 0 && CVarNaniteBundleRasterSW.GetValueOnAnyThread() != 0 && (GRHISupportsShaderBundleDispatch || CanUseShaderBundleWorkGraphSW(Platform));
}

static bool UseRasterShaderBundleHW(EShaderPlatform Platform)
{
	return CVarNaniteBundleRaster.GetValueOnRenderThread() != 0 && CVarNaniteBundleRasterHW.GetValueOnAnyThread() != 0 && (GRHISupportsShaderBundleDispatch || CanUseShaderBundleWorkGraphHW(Platform));
}

void FRenderer::PrepareRasterizerPasses(
	FRenderer::FDispatchContext& Context,
	const ERasterHardwarePath HardwarePath,
	const ERHIFeatureLevel::Type FeatureLevel,
	const FNaniteRasterPipelines& RasterPipelines,
	const FNaniteVisibilityQuery* VisibilityQuery,
	bool bCustomPass,
	bool bLumenCapture
)
{
	const bool bHasVirtualShadowMap = IsUsingVirtualShadowMap();
	const bool bIsMaterialCache     = IsMaterialCache();

	Context.FixedMaterialProxy	= UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	Context.HiddenMaterialProxy	= GEngine->NaniteHiddenSectionMaterial->GetRenderProxy();

	const FNaniteRasterPipelineMap& Pipelines = RasterPipelines.GetRasterPipelineMap();

	const uint32 RasterBinCount = RasterPipelines.GetBinCount();

	Context.MetaBufferData.SetNumZeroed(RasterBinCount);

	Context.SWShaderBundle = nullptr;
	Context.SWShaderBundleAsync = nullptr;
	Context.HWShaderBundle = nullptr;

	const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);

	// Create Shader Bundle
	if (RasterBinCount > 0)
	{
		/*  Nanite Notes:
				8x Total DWords
				See: WriteRasterizerArgsSWHW

			SW (1/2):
				SW: ThreadGroupCountX
				SW: ThreadGroupCountY
				SW: ThreadGroupCountZ
				Padding
			MS (2/2):
				HW: ThreadGroupCountX (NumClustersHW)
				HW: ThreadGroupCountY (1 unless wrapped platform)
				HW: ThreadGroupCountZ (1 unless wrapped platform)
				Padding
			VS (2/2):
				HW: VertexCountPerInstance (NANITE_MAX_CLUSTER_TRIANGLES * 3)
				HW: InstanceCount (NumClustersHW)
				HW: StartVertexLocation (Always 0)
				HW: StartInstanceLocation (Always 0)
		*/
		const uint32 NumRecords = RasterBinCount;
		const uint32 ArgStride = NANITE_RASTERIZER_ARG_COUNT * 4u;
		
		// SW shader bundle
		if (UseRasterShaderBundleSW(ShaderPlatform))
		{
			FShaderBundleCreateInfo BundleCreateInfo;
			BundleCreateInfo.ArgOffset = 0u;
			BundleCreateInfo.ArgStride = ArgStride;
			BundleCreateInfo.NumRecords = NumRecords;
			BundleCreateInfo.Mode = ERHIShaderBundleMode::CS;
			Context.SWShaderBundle = RHICreateShaderBundle(BundleCreateInfo);
			check(Context.SWShaderBundle != nullptr);

			if (CVarNaniteEnableAsyncRasterization.GetValueOnRenderThread() != 0)
			{
				Context.SWShaderBundleAsync = RHICreateShaderBundle(BundleCreateInfo);
				check(Context.SWShaderBundleAsync != nullptr);
			}
		}

		// HW shader bundle
		if (UseRasterShaderBundleHW(ShaderPlatform))
		{
			FShaderBundleCreateInfo BundleCreateInfo;
			BundleCreateInfo.ArgOffset = 16u;
			BundleCreateInfo.ArgStride = ArgStride;
			BundleCreateInfo.NumRecords = NumRecords;
			BundleCreateInfo.Mode = IsMeshShaderRasterPath(HardwarePath) ? ERHIShaderBundleMode::MSPS : ERHIShaderBundleMode::VSPS;
			Context.HWShaderBundle = RHICreateShaderBundle(BundleCreateInfo);
			check(Context.HWShaderBundle != nullptr);
		}
	}

	static UE::Tasks::FPipe GNaniteRasterSetupPipe(TEXT("NaniteRasterSetupPipe"));

	// Threshold of active passes to launch an async task.
	const int32 VisiblePassAsyncThreshold = 8;

	const bool bUseSetupCache = UseRasterSetupCache();

	GraphBuilder.AddSetupTask(
	[
		&Context,
		&RasterPipelines,
		VisibilityQuery,
		RasterBinCount,
		RenderFlags = RenderFlags,
		FeatureLevel,
		ShaderPlatform,
		bUseSetupCache,
		bCustomPass,
		bLumenCapture,
		RasterMode = RasterContext.RasterMode,
		VisualizeActive = RasterContext.VisualizeActive,
		HardwarePath,
		bHasVirtualShadowMap,
		bIsMaterialCache
	]
	{
		SCOPED_NAMED_EVENT(PrepareRasterizerPasses_Async, FColor::Emerald);

		const FMaterial* FixedMaterial = Context.FixedMaterialProxy->GetMaterialNoFallback(FeatureLevel);
		const FMaterialShaderMap* FixedMaterialShaderMap = FixedMaterial ? FixedMaterial->GetRenderingThreadShaderMap() : nullptr;

		FHWRasterizeVS::FPermutationDomain PermutationVectorVS;
		FHWRasterizeMS::FPermutationDomain PermutationVectorMS;
		FHWRasterizePS::FPermutationDomain PermutationVectorPS;

		FMicropolyRasterizeCS::FPermutationDomain PermutationVectorCS_Cluster;
		FMicropolyRasterizeCS::FPermutationDomain PermutationVectorCS_Patch;

		const bool bDepthBucketPixelProgrammable = CVarNaniteDepthBucketPixelProgrammable.GetValueOnRenderThread() != 0;

		SetupPermutationVectors(
			RasterMode,
			HardwarePath,
			VisualizeActive,
			bHasVirtualShadowMap,
			bIsMaterialCache,
			PermutationVectorVS,
			PermutationVectorMS,
			PermutationVectorPS,
			PermutationVectorCS_Cluster,
			PermutationVectorCS_Patch
		);

		const auto FillFixedMaterialShaders = [&](FRasterizerPass& RasterizerPass)
		{
			const bool bMeshShaderRasterPath = IsMeshShaderRasterPath(HardwarePath);
			const bool bUseBarycentricPermutation = ShouldUseSvBarycentricPermutation(ShaderPlatform, RasterizerPass.bPixelProgrammable, bMeshShaderRasterPath);
			const bool bFixedDisplacementFallback = RasterizerPass.RasterPipeline.bFixedDisplacementFallback;

			if (bMeshShaderRasterPath)
			{
				PermutationVectorMS.Set<FHWRasterizeMS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
				PermutationVectorMS.Set<FHWRasterizeMS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
				PermutationVectorMS.Set<FHWRasterizeMS::FSplineDeformDim>(RasterizerPass.bSplineMesh);
				PermutationVectorMS.Set<FHWRasterizeMS::FSkinningDim>(RasterizerPass.bSkinnedMesh);
				PermutationVectorMS.Set<FHWRasterizeMS::FAllowSvBarycentricsDim>(bUseBarycentricPermutation);
				PermutationVectorMS.Set<FHWRasterizeMS::FFixedDisplacementFallbackDim>(bFixedDisplacementFallback);
				
				const EShaderFrequency ShaderFrequencyMS = RasterizerPass.bUseWorkGraphHW ? SF_WorkGraphComputeNode : SF_Mesh;
				RasterizerPass.RasterMeshShader = GetHWRasterizeMeshShader(FixedMaterialShaderMap, PermutationVectorMS, ShaderFrequencyMS);
				check(!RasterizerPass.RasterMeshShader.IsNull());
			}
			else
			{
				PermutationVectorVS.Set<FHWRasterizeVS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
				PermutationVectorVS.Set<FHWRasterizeVS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
				PermutationVectorVS.Set<FHWRasterizeVS::FSplineDeformDim>(RasterizerPass.bSplineMesh);
				PermutationVectorVS.Set<FHWRasterizeVS::FSkinningDim>(RasterizerPass.bSkinnedMesh);
				PermutationVectorVS.Set<FHWRasterizeVS::FFixedDisplacementFallbackDim>(bFixedDisplacementFallback);
				RasterizerPass.RasterVertexShader = FixedMaterialShaderMap->GetShader<FHWRasterizeVS>(PermutationVectorVS);
				check(!RasterizerPass.RasterVertexShader.IsNull());
			}

			PermutationVectorPS.Set<FHWRasterizePS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
			PermutationVectorPS.Set<FHWRasterizePS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
			PermutationVectorPS.Set<FHWRasterizePS::FAllowSvBarycentricsDim>(bUseBarycentricPermutation);

			RasterizerPass.RasterPixelShader = FixedMaterialShaderMap->GetShader<FHWRasterizePS>(PermutationVectorPS);
			check(!RasterizerPass.RasterPixelShader.IsNull());

			PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FTessellationDim>(false);
			PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FTwoSidedDim>(RasterizerPass.RasterPipeline.bIsTwoSided);
			PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
			PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
			PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FVoxelsDim>(RasterizerPass.RasterPipeline.bVoxel); 
			PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FSplineDeformDim>(RasterizerPass.bSplineMesh);
			PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FSkinningDim>(RasterizerPass.bSkinnedMesh);
			PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FFixedDisplacementFallbackDim>(bFixedDisplacementFallback);
			
			const EShaderFrequency ShaderFrequencyCS = RasterizerPass.bUseWorkGraphSW ? SF_WorkGraphComputeNode : SF_Compute;
			RasterizerPass.ClusterComputeShader = GetMicropolyRasterizeShader(FixedMaterialShaderMap, PermutationVectorCS_Cluster, ShaderFrequencyCS);
			check(!RasterizerPass.ClusterComputeShader.IsNull());

			RasterizerPass.PatchComputeShader.Reset();

			RasterizerPass.VertexMaterial  = FixedMaterial;
			RasterizerPass.PixelMaterial   = FixedMaterial;
			RasterizerPass.ComputeMaterial = FixedMaterial;
		};

		uint32 TotalDepthBlocks = 0u;

		const auto CacheRasterizerPass = [&](const FNaniteRasterEntry& RasterEntry, FRasterizerPass& RasterizerPass, FNaniteRasterMaterialCache& RasterMaterialCache)
		{
			FNaniteRasterBinMeta& BinMeta = Context.MetaBufferData[RasterizerPass.RasterBin];
			uint32 MaterialBitFlags = BinMeta.MaterialFlags_DepthBlock & 0xFFFFu;

			RasterizerPass.RasterMaterialCache = &RasterMaterialCache;

			if (RasterMaterialCache.MaterialBitFlags)
			{
				MaterialBitFlags = RasterMaterialCache.MaterialBitFlags.GetValue();
			}
			else
			{
				const FMaterial& RasterMaterial = RasterizerPass.RasterPipeline.RasterMaterial->GetIncompleteMaterialWithFallback(FeatureLevel);
				MaterialBitFlags = PackMaterialBitFlags_RenderThread(RasterMaterial, RasterEntry.RasterPipeline);

				RasterMaterialCache.MaterialBitFlags = MaterialBitFlags;
				RasterMaterialCache.DisplacementScaling = RasterizerPass.RasterPipeline.DisplacementScaling;
				RasterMaterialCache.DisplacementFadeRange = RasterizerPass.RasterPipeline.DisplacementFadeRange;
			}

			BinMeta.MaterialDisplacementParams.Center = RasterMaterialCache.DisplacementScaling->Center;
			BinMeta.MaterialDisplacementParams.Magnitude = RasterMaterialCache.DisplacementScaling->Magnitude;
			CalcDisplacementFadeSizes(
				*RasterMaterialCache.DisplacementFadeRange,
				BinMeta.MaterialDisplacementParams.FadeSizeStart,
				BinMeta.MaterialDisplacementParams.FadeSizeStop
			);

			RasterizerPass.bVertexProgrammable = FNaniteMaterialShader::IsVertexProgrammable(MaterialBitFlags);
			RasterizerPass.bPixelProgrammable = FNaniteMaterialShader::IsPixelProgrammable(MaterialBitFlags);
			RasterizerPass.bDisplacement = MaterialBitFlags & NANITE_MATERIAL_FLAG_DISPLACEMENT;
			RasterizerPass.bSplineMesh = MaterialBitFlags & NANITE_MATERIAL_FLAG_SPLINE_MESH;
			RasterizerPass.bSkinnedMesh = MaterialBitFlags & NANITE_MATERIAL_FLAG_SKINNED_MESH;
			RasterizerPass.bTwoSided = MaterialBitFlags & NANITE_MATERIAL_FLAG_TWO_SIDED;
			RasterizerPass.bCastShadow = MaterialBitFlags & NANITE_MATERIAL_FLAG_CAST_SHADOW;
			RasterizerPass.bVertexUVs = MaterialBitFlags & NANITE_MATERIAL_FLAG_VERTEX_UVS;

			if (RasterMaterialCache.bFinalized)
			{
				RasterizerPass.VertexMaterialProxy = RasterMaterialCache.VertexMaterialProxy;
				RasterizerPass.PixelMaterialProxy = RasterMaterialCache.PixelMaterialProxy;
				RasterizerPass.ComputeMaterialProxy = RasterMaterialCache.ComputeMaterialProxy;
				RasterizerPass.RasterVertexShader = RasterMaterialCache.RasterVertexShader;
				RasterizerPass.RasterPixelShader = RasterMaterialCache.RasterPixelShader;
				RasterizerPass.RasterMeshShader = RasterMaterialCache.RasterMeshShader;
				RasterizerPass.ClusterComputeShader = RasterMaterialCache.ClusterComputeShader;
				RasterizerPass.PatchComputeShader = RasterMaterialCache.PatchComputeShader;
				RasterizerPass.VertexMaterial = RasterMaterialCache.VertexMaterial;
				RasterizerPass.PixelMaterial = RasterMaterialCache.PixelMaterial;
				RasterizerPass.ComputeMaterial = RasterMaterialCache.ComputeMaterial;
			}
			else if (RasterizerPass.bVertexProgrammable || RasterizerPass.bPixelProgrammable)
			{
				FMaterialShaderTypes ProgrammableShaderTypes;
				FMaterialShaderTypes NonProgrammableShaderTypes;
				FMaterialShaderTypes PatchShaderType;
				GetMaterialShaderTypes(
					ShaderPlatform,
					HardwarePath,
					RasterizerPass.bVertexProgrammable,
					RasterizerPass.bPixelProgrammable,
					RasterizerPass.RasterPipeline.bIsTwoSided,
					RasterizerPass.RasterPipeline.bSplineMesh,
					RasterizerPass.RasterPipeline.bSkinnedMesh,
					RasterizerPass.bDisplacement,
					false /*bFixedDisplacementFallback*/,
					RasterizerPass.RasterPipeline.bVoxel,
					RasterizerPass.bUseWorkGraphSW,
					RasterizerPass.bUseWorkGraphHW,
					PermutationVectorVS,
					PermutationVectorMS,
					PermutationVectorPS,
					PermutationVectorCS_Cluster,
					PermutationVectorCS_Patch,
					ProgrammableShaderTypes,
					NonProgrammableShaderTypes,
					PatchShaderType
				);

				const FMaterialRenderProxy* ProgrammableRasterProxy = RasterEntry.RasterPipeline.RasterMaterial;
				while (ProgrammableRasterProxy)
				{
					const FMaterial* Material = ProgrammableRasterProxy->GetMaterialNoFallback(FeatureLevel);
					if (Material)
					{
						FMaterialShaders ProgrammableShaders;
						FMaterialShaders PatchShader;

						const bool bFetch1 = Material->TryGetShaders(ProgrammableShaderTypes, nullptr, ProgrammableShaders);
						const bool bFetch2 = !RasterizerPass.bDisplacement || Material->TryGetShaders(PatchShaderType, nullptr, PatchShader);

						if (bFetch1 && bFetch2)
						{
							if (RasterizerPass.bVertexProgrammable)
							{
								if (IsMeshShaderRasterPath(HardwarePath))
								{
									if (ProgrammableShaders.TryGetMeshShader(&RasterizerPass.RasterMeshShader))
									{
										RasterizerPass.VertexMaterialProxy = ProgrammableRasterProxy;
										RasterizerPass.VertexMaterial = Material;
									}
								}
								else
								{
									if (ProgrammableShaders.TryGetVertexShader(&RasterizerPass.RasterVertexShader))
									{
										RasterizerPass.VertexMaterialProxy = ProgrammableRasterProxy;
										RasterizerPass.VertexMaterial = Material;
									}
								}
							}

							if (RasterizerPass.bPixelProgrammable && ProgrammableShaders.TryGetShader(SF_Pixel, &RasterizerPass.RasterPixelShader))
							{
								RasterizerPass.PixelMaterialProxy = ProgrammableRasterProxy;
								RasterizerPass.PixelMaterial = Material;
							}

							const EShaderFrequency ShaderFrequencyCS = RasterizerPass.bUseWorkGraphSW ? SF_WorkGraphComputeNode : SF_Compute;
							if (ProgrammableShaders.TryGetShader(ShaderFrequencyCS, &RasterizerPass.ClusterComputeShader) && (!RasterizerPass.bDisplacement || PatchShader.TryGetShader(ShaderFrequencyCS, &RasterizerPass.PatchComputeShader)))
							{
								RasterizerPass.ComputeMaterialProxy = ProgrammableRasterProxy;
								RasterizerPass.ComputeMaterial = Material;
							}

							break;
						}
					}

					ProgrammableRasterProxy = ProgrammableRasterProxy->GetFallback(FeatureLevel);
				}
			#if !UE_BUILD_SHIPPING
				if (ShouldReportFeedbackMaterialPerformanceWarning() && ProgrammableRasterProxy != nullptr)
				{
					const FMaterial* Material = ProgrammableRasterProxy->GetMaterialNoFallback(FeatureLevel);
					if (Material != nullptr && (Material->MaterialUsesPixelDepthOffset_RenderThread() || Material->IsMasked()))
					{
						GGlobalResources.GetFeedbackManager()->ReportMaterialPerformanceWarning(ProgrammableRasterProxy->GetMaterialName());
					}
				}
			#endif
			}
			else
			{
				FillFixedMaterialShaders(RasterizerPass);
			}

			// Patch in the no derivative ops flags into the meta data buffer - this does not need to be present in the setup cache key
			// We just need it on the GPU for raster binning to force shaders with finite differences down the HW path.
			if (!RasterizerPass.HasDerivativeOps())
			{
				FNaniteMaterialFlags Unpacked = UnpackNaniteMaterialFlags(MaterialBitFlags);
				Unpacked.bNoDerivativeOps = true;
				MaterialBitFlags = PackNaniteMaterialBitFlags(Unpacked);
			}

			const uint32 NumDepthBlocks = RasterEntry.RasterPipeline.bVoxel ? 1 :
										(bDepthBucketPixelProgrammable && RasterEntry.RasterPipeline.bPerPixelEval) ? 2 : 0;

			uint32 DepthBlockIndex = 0xFFFFu;
			if (NumDepthBlocks != 0u)
			{
				DepthBlockIndex = TotalDepthBlocks;
				TotalDepthBlocks += NumDepthBlocks;
			}

			check(MaterialBitFlags <= 0xFFFFu);
			BinMeta.MaterialFlags_DepthBlock = (DepthBlockIndex << 16) | MaterialBitFlags;
		};

		const FNaniteRasterPipelineMap& Pipelines = RasterPipelines.GetRasterPipelineMap();
		const FNaniteRasterBinIndexTranslator BinIndexTranslator = RasterPipelines.GetBinIndexTranslator();
		const FNaniteVisibilityResults* VisibilityResults = Nanite::GetVisibilityResults(VisibilityQuery);

		const bool bDisableProgrammable = (RenderFlags & NANITE_RENDER_FLAG_DISABLE_PROGRAMMABLE) != 0u;

		Context.Reserve(RasterPipelines.GetBinCount());

		int32 RasterBinIndex = 0;
		for (const auto& RasterBin : Pipelines)
		{
			ON_SCOPE_EXIT{ RasterBinIndex++; };

			const FNaniteRasterEntry& RasterEntry = RasterBin.Value;

			const bool bIsShadowPass = (RenderFlags & NANITE_RENDER_FLAG_IS_SHADOW_PASS) != 0u;

			// Avoid caching any passes if we do not have a valid fixed function material.
			// This can happen sometimes during shader recompilation, or if the default material has errors.
			if (FixedMaterialShaderMap == nullptr)
			{
				continue;
			}

			// Any bins within the fixed function bin mask are special cased
			const bool bFixedFunctionBin = RasterEntry.BinIndex <= NANITE_FIXED_FUNCTION_BIN_MASK;
			if (bFixedFunctionBin)
			{
				// Skinning and spline meshes are mutually exclusive - do not launch bins with this combination
				const uint16 InvalidBinMask = (NANITE_FIXED_FUNCTION_BIN_SKINNED | NANITE_FIXED_FUNCTION_BIN_SPLINE);
				if ((RasterEntry.BinIndex & InvalidBinMask) == InvalidBinMask)
				{
					// Invalid combinations
					continue;
				}

				if ((RasterEntry.BinIndex & NANITE_FIXED_FUNCTION_BIN_VOXEL) != 0u)
				{
					if ((RasterEntry.BinIndex & (NANITE_FIXED_FUNCTION_BIN_TWOSIDED | NANITE_FIXED_FUNCTION_BIN_SPLINE)) != 0u)
					{
						continue;
					}
				}

				if ((RasterEntry.BinIndex & NANITE_FIXED_FUNCTION_BIN_SPLINE) != 0 && !NaniteSplineMeshesSupported())
				{
					continue;
				}

				if ((RasterEntry.BinIndex & NANITE_FIXED_FUNCTION_BIN_SKINNED) != 0 && !NaniteSkinnedMeshesSupported())
				{
					continue;
				}
			}

			// Skip any non shadow casting raster bin (including fixed function) if shadow view
			if (bIsShadowPass && !RasterEntry.RasterPipeline.bCastShadow)
			{
				continue;
			}
	
			// Fixed function bins are always visible
			if (!bFixedFunctionBin)
			{
				if (bCustomPass && !RasterPipelines.ShouldBinRenderInCustomPass(RasterEntry.BinIndex))
				{
					// Predicting that this bin will be empty if we rasterize it in the Custom Pass (i.e. Custom)
					continue;
				}
	
				// Test for visibility
				if (!bLumenCapture && VisibilityResults && !VisibilityResults->IsRasterBinVisible(RasterEntry.BinIndex))
				{
					continue;
				}
			}

			FRasterizerPass& RasterizerPass = Context.RasterizerPasses.AddDefaulted_GetRef();
			RasterizerPass.RasterBin = uint32(BinIndexTranslator.Translate(RasterEntry.BinIndex));
			RasterizerPass.RasterPipeline = RasterEntry.RasterPipeline;

			RasterizerPass.VertexMaterialProxy	= Context.FixedMaterialProxy;
			RasterizerPass.PixelMaterialProxy	= Context.FixedMaterialProxy;
			RasterizerPass.ComputeMaterialProxy	= Context.FixedMaterialProxy;

			const bool bUseWorkGraphBundles = UseWorkGraphForRasterBundles(ShaderPlatform);
			RasterizerPass.bUseWorkGraphSW = bUseWorkGraphBundles && UseRasterShaderBundleSW(ShaderPlatform);
			RasterizerPass.bUseWorkGraphHW = bUseWorkGraphBundles && UseRasterShaderBundleHW(ShaderPlatform);

			FNaniteRasterMaterialCacheKey RasterMaterialCacheKey;
			if (bUseSetupCache)
			{
				RasterMaterialCacheKey.FeatureLevel = FeatureLevel;
				RasterMaterialCacheKey.bWPOEnabled = RasterEntry.RasterPipeline.bWPOEnabled;
				RasterMaterialCacheKey.bPerPixelEval = RasterEntry.RasterPipeline.bPerPixelEval;
				RasterMaterialCacheKey.bUseMeshShader = IsMeshShaderRasterPath(HardwarePath);
				RasterMaterialCacheKey.bUsePrimitiveShader = HardwarePath == ERasterHardwarePath::PrimitiveShader;
				RasterMaterialCacheKey.bDisplacementEnabled = RasterEntry.RasterPipeline.bDisplacementEnabled;
				RasterMaterialCacheKey.bVisualizeActive = VisualizeActive;
				RasterMaterialCacheKey.bHasVirtualShadowMap = bHasVirtualShadowMap;
				RasterMaterialCacheKey.bIsDepthOnly = RasterMode == EOutputBufferMode::DepthOnly;
				RasterMaterialCacheKey.bIsTwoSided = RasterizerPass.RasterPipeline.bIsTwoSided;
				RasterMaterialCacheKey.bCastShadow = RasterizerPass.RasterPipeline.bCastShadow;
				RasterMaterialCacheKey.bVoxel = RasterEntry.RasterPipeline.bVoxel;
				RasterMaterialCacheKey.bSplineMesh = RasterEntry.RasterPipeline.bSplineMesh;
				RasterMaterialCacheKey.bSkinnedMesh = RasterEntry.RasterPipeline.bSkinnedMesh;
				RasterMaterialCacheKey.bFixedDisplacementFallback = RasterEntry.RasterPipeline.bFixedDisplacementFallback;
				RasterMaterialCacheKey.bUseWorkGraphSW = RasterizerPass.bUseWorkGraphSW;
				RasterMaterialCacheKey.bUseWorkGraphHW = RasterizerPass.bUseWorkGraphHW;;
			}

			FNaniteRasterMaterialCache  EmptyCache;
			FNaniteRasterMaterialCache& RasterMaterialCache = bUseSetupCache ? RasterEntry.CacheMap.FindOrAdd(RasterMaterialCacheKey) : EmptyCache;

			CacheRasterizerPass(RasterEntry, RasterizerPass, RasterMaterialCache);

			// Note: The indirect args offset is in bytes
			RasterizerPass.IndirectOffset = (RasterizerPass.RasterBin * NANITE_RASTERIZER_ARG_COUNT) * 4u;

			if (RasterizerPass.VertexMaterialProxy  == Context.HiddenMaterialProxy &&
				RasterizerPass.PixelMaterialProxy   == Context.HiddenMaterialProxy &&
				RasterizerPass.ComputeMaterialProxy == Context.HiddenMaterialProxy)
			{
				RasterizerPass.bHidden = true;
			}
			else if (bFixedFunctionBin)
			{
				const bool bCastShadowBin = (RasterizerPass.RasterBin & NANITE_FIXED_FUNCTION_BIN_CAST_SHADOW) != 0;
				if (bCastShadowBin != bIsShadowPass)
				{
					// Raster binning for non shadow views will remap all fixed function bins into non shadow casting
					RasterizerPass.bHidden = true;
				}
			}
			else if (bDisableProgrammable)
			{
				// If programmable is disabled, hide all programmable bins
				// Raster binning will remap from these bins to appropriate fixed function bins.
				RasterizerPass.bHidden = true;
			}

			if (!RasterizerPass.bHidden)
			{
				const bool bMeshShaderRasterPath = IsMeshShaderRasterPath(HardwarePath);
				const bool bUseBarycentricPermutation = ShouldUseSvBarycentricPermutation(ShaderPlatform, RasterizerPass.bPixelProgrammable, bMeshShaderRasterPath);

				if (bMeshShaderRasterPath)
				{
					if (RasterizerPass.RasterMeshShader.IsNull())
					{
						const FMaterialShaderMap* VertexShaderMap = RasterizerPass.VertexMaterialProxy->GetMaterialWithFallback(FeatureLevel, RasterizerPass.VertexMaterialProxy).GetRenderingThreadShaderMap();
						check(VertexShaderMap);

						PermutationVectorMS.Set<FHWRasterizeMS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
						PermutationVectorMS.Set<FHWRasterizeMS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
						PermutationVectorMS.Set<FHWRasterizeMS::FSplineDeformDim>(RasterizerPass.bSplineMesh);
						PermutationVectorMS.Set<FHWRasterizeMS::FSkinningDim>(RasterizerPass.bSkinnedMesh);
						PermutationVectorMS.Set<FHWRasterizeMS::FAllowSvBarycentricsDim>(bUseBarycentricPermutation);

						const EShaderFrequency ShaderFrequencyMS = RasterizerPass.bUseWorkGraphHW ? SF_WorkGraphComputeNode : SF_Mesh;
						RasterizerPass.RasterMeshShader = GetHWRasterizeMeshShader(VertexShaderMap, PermutationVectorMS, ShaderFrequencyMS);
						check(!RasterizerPass.RasterMeshShader.IsNull());
					}
				}
				else
				{
					if (RasterizerPass.RasterVertexShader.IsNull())
					{
						const FMaterialShaderMap* VertexShaderMap = RasterizerPass.VertexMaterialProxy->GetMaterialWithFallback(FeatureLevel, RasterizerPass.VertexMaterialProxy).GetRenderingThreadShaderMap();
						check(VertexShaderMap);

						PermutationVectorVS.Set<FHWRasterizeVS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
						PermutationVectorVS.Set<FHWRasterizeVS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
						PermutationVectorVS.Set<FHWRasterizeVS::FSplineDeformDim>(RasterizerPass.bSplineMesh);
						PermutationVectorVS.Set<FHWRasterizeVS::FSkinningDim>(RasterizerPass.bSkinnedMesh);
						RasterizerPass.RasterVertexShader = VertexShaderMap->GetShader<FHWRasterizeVS>(PermutationVectorVS);
						check(!RasterizerPass.RasterVertexShader.IsNull());
					}
				}

				if (RasterizerPass.RasterPixelShader.IsNull())
				{
					const FMaterialShaderMap* PixelShaderMap = RasterizerPass.PixelMaterialProxy->GetMaterialWithFallback(FeatureLevel, RasterizerPass.PixelMaterialProxy).GetRenderingThreadShaderMap();
					check(PixelShaderMap);

					PermutationVectorPS.Set<FHWRasterizePS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
					PermutationVectorPS.Set<FHWRasterizePS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
					PermutationVectorPS.Set<FHWRasterizePS::FAllowSvBarycentricsDim>(bUseBarycentricPermutation);

					RasterizerPass.RasterPixelShader = PixelShaderMap->GetShader<FHWRasterizePS>(PermutationVectorPS);
					check(!RasterizerPass.RasterPixelShader.IsNull());
				}

				if (RasterizerPass.ClusterComputeShader.IsNull())
				{
					const FMaterialShaderMap* ComputeShaderMap = RasterizerPass.ComputeMaterialProxy->GetMaterialWithFallback(FeatureLevel, RasterizerPass.ComputeMaterialProxy).GetRenderingThreadShaderMap();
					check(ComputeShaderMap);

					PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FPatchesDim>(false);
					PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FTwoSidedDim>(RasterizerPass.RasterPipeline.bIsTwoSided);
					PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
					PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
					PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FSplineDeformDim>(RasterizerPass.bSplineMesh);
					PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FSkinningDim>(RasterizerPass.bSkinnedMesh);
					PermutationVectorCS_Cluster.Set<FMicropolyRasterizeCS::FVoxelsDim>(RasterizerPass.RasterPipeline.bVoxel);

					const EShaderFrequency ShaderFrequencyCS = RasterizerPass.bUseWorkGraphSW ? SF_WorkGraphComputeNode : SF_Compute;
					RasterizerPass.ClusterComputeShader = GetMicropolyRasterizeShader(ComputeShaderMap, PermutationVectorCS_Cluster, ShaderFrequencyCS);
					check(!RasterizerPass.ClusterComputeShader.IsNull());
				}

				if (RasterizerPass.bDisplacement && RasterizerPass.PatchComputeShader.IsNull())
				{
					const FMaterialShaderMap* ComputeShaderMap = RasterizerPass.ComputeMaterialProxy->GetMaterialWithFallback(FeatureLevel, RasterizerPass.ComputeMaterialProxy).GetRenderingThreadShaderMap();
					check(ComputeShaderMap);

					PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FPatchesDim>(true);
					PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FTwoSidedDim>(RasterizerPass.RasterPipeline.bIsTwoSided);
					PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
					PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
					PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FSplineDeformDim>(RasterizerPass.bSplineMesh);
					PermutationVectorCS_Patch.Set<FMicropolyRasterizeCS::FSkinningDim>(RasterizerPass.bSkinnedMesh);

					const EShaderFrequency ShaderFrequencyCS = RasterizerPass.bUseWorkGraphSW ? SF_WorkGraphComputeNode : SF_Compute;
					RasterizerPass.PatchComputeShader = GetMicropolyRasterizeShader(ComputeShaderMap, PermutationVectorCS_Patch, ShaderFrequencyCS);
					check(!RasterizerPass.PatchComputeShader.IsNull());
				}

				if (!RasterizerPass.VertexMaterial)
				{
					RasterizerPass.VertexMaterial = RasterizerPass.VertexMaterialProxy->GetMaterialNoFallback(FeatureLevel);
				}
				check(RasterizerPass.VertexMaterial);

				if (!RasterizerPass.PixelMaterial)
				{
					RasterizerPass.PixelMaterial = RasterizerPass.PixelMaterialProxy->GetMaterialNoFallback(FeatureLevel);
				}
				check(RasterizerPass.PixelMaterial);

				if (!RasterizerPass.ComputeMaterial)
				{
					RasterizerPass.ComputeMaterial = RasterizerPass.ComputeMaterialProxy->GetMaterialNoFallback(FeatureLevel);
				}
				check(RasterizerPass.ComputeMaterial);

				if (bUseSetupCache && RasterizerPass.RasterMaterialCache && !RasterizerPass.RasterMaterialCache->bFinalized)
				{
					RasterizerPass.RasterMaterialCache->VertexMaterialProxy = RasterizerPass.VertexMaterialProxy;
					RasterizerPass.RasterMaterialCache->PixelMaterialProxy = RasterizerPass.PixelMaterialProxy;
					RasterizerPass.RasterMaterialCache->ComputeMaterialProxy = RasterizerPass.ComputeMaterialProxy;
					RasterizerPass.RasterMaterialCache->RasterVertexShader = RasterizerPass.RasterVertexShader;
					RasterizerPass.RasterMaterialCache->RasterPixelShader = RasterizerPass.RasterPixelShader;
					RasterizerPass.RasterMaterialCache->RasterMeshShader = RasterizerPass.RasterMeshShader;
					RasterizerPass.RasterMaterialCache->ClusterComputeShader = RasterizerPass.ClusterComputeShader;
					RasterizerPass.RasterMaterialCache->PatchComputeShader = RasterizerPass.PatchComputeShader;
					RasterizerPass.RasterMaterialCache->VertexMaterial = RasterizerPass.VertexMaterial;
					RasterizerPass.RasterMaterialCache->PixelMaterial = RasterizerPass.PixelMaterial;
					RasterizerPass.RasterMaterialCache->ComputeMaterial = RasterizerPass.ComputeMaterial;
					RasterizerPass.RasterMaterialCache->bFinalized = true;
				}

				// Build dispatch list indirections
				const int32 PassIndex = Context.RasterizerPasses.Num() - 1;
				if (RasterizerPass.bDisplacement)
				{
					// Displaced meshes never run the HW path
					Context.Dispatches_SW_Tessellated.Indirections.Emplace(PassIndex);
				}
				else
				{
					Context.Dispatches_SW_Triangles.Indirections.Emplace(PassIndex);
					Context.Dispatches_HW_Triangles.Indirections.Emplace(PassIndex);
				}
			}
		}

		if (CVarNaniteRasterSort.GetValueOnRenderThread())
		{
			auto SortIndirections = [&](FDispatchContext::FDispatchList& List)
			{
				const uint32 Num = List.Indirections.Num();

				TArray<TPair<uint32, uint32>> SortList;
				SortList.Reserve(Num);

				for (uint32 PassIndex : List.Indirections)
				{
					FRasterizerPass& Pass = Context.RasterizerPasses[PassIndex];
					SortList.Emplace(Pass.CalcSortKey(), PassIndex);
				}

				SortList.Sort();

				for (uint32 i = 0; i < Num; i++)
				{
					List.Indirections[i] = SortList[i].Value;
				}
			};

			SortIndirections(Context.Dispatches_SW_Tessellated);
			SortIndirections(Context.Dispatches_SW_Triangles);
			SortIndirections(Context.Dispatches_HW_Triangles);
		}
		Context.NumDepthBlocks = TotalDepthBlocks;
	},
		bUseSetupCache ? &GNaniteRasterSetupPipe : nullptr,
		GetVisibilityTask(VisibilityQuery),
		UE::Tasks::ETaskPriority::Normal,
		CVarNaniteRasterSetupTask.GetValueOnRenderThread() > 0
	);

	// Create raster in meta buffer (now that the setup task has completed populating the source memory)
	if (RasterBinCount > 0)
	{
		Context.MetaBuffer = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("Nanite.RasterBinMeta"),
			sizeof(FNaniteRasterBinMeta),
			FMath::RoundUpToPowerOfTwo(FMath::Max(RasterBinCount, 1u)),
			Context.MetaBufferData.GetData(),
			sizeof(FNaniteRasterBinMeta) * RasterBinCount,
			// The buffer data is allocated on the RDG timeline and and gets filled by an RDG setup task.
			ERDGInitialDataFlags::NoCopy
		);
	}
}

FBinningData FRenderer::AddPass_Rasterize(
	const FDispatchContext& DispatchContext,
	FRDGBufferRef IndirectArgs,
	FRDGBufferRef VisiblePatches,
	FRDGBufferRef VisiblePatchesArgs,
	const FGlobalWorkQueueParameters& SplitWorkQueue,
	const FGlobalWorkQueueParameters& OccludedPatches,
	bool bMainPass)
{
	SCOPED_NAMED_EVENT(AddPass_Rasterize, FColor::Emerald);
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);

	const ERHIFeatureLevel::Type FeatureLevel = Scene.GetFeatureLevel();
	const ERasterHardwarePath HardwarePath = GetRasterHardwarePath(Scene.GetShaderPlatform(), SharedContext.Pipeline);

	// Assume an arbitrary large workload when programmable raster is enabled.
	const int32 PassWorkload = (RenderFlags & NANITE_RENDER_FLAG_DISABLE_PROGRAMMABLE) != 0u ? 1 : 256;

	FRDGBufferRef ClusterOffsetSWHW = MainRasterizeArgsSWHW;
	if (bMainPass)
	{
		//check(ClusterOffsetSWHW == nullptr);
		ClusterOffsetSWHW = GSystemTextures.GetDefaultBuffer(GraphBuilder, sizeof(uint32));
		RenderFlags &= ~NANITE_RENDER_FLAG_ADD_CLUSTER_OFFSET;
	}
	else
	{
		RenderFlags |= NANITE_RENDER_FLAG_ADD_CLUSTER_OFFSET;
	}

	const ERasterScheduling Scheduling = RasterContext.RasterScheduling;
	const bool bTessellationEnabled = VisiblePatchesArgs != nullptr && (Scheduling != ERasterScheduling::HardwareOnly);

	const auto CreateSkipBarrierUAV = [&](auto& InOutUAV)
	{
		if (InOutUAV)
		{
			InOutUAV = GraphBuilder.CreateUAV(InOutUAV->Desc, ERDGUnorderedAccessViewFlags::SkipBarrier);
		}
	};

	FRDGBufferRef DummyBuffer8 = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 8);
	FRDGBufferRef DummyBufferRasterMeta = GSystemTextures.GetDefaultStructuredBuffer<FNaniteRasterBinMeta>(GraphBuilder);

	// Create a new set of UAVs with the SkipBarrier flag enabled to avoid barriers between dispatches.
	FRasterParameters RasterParameters = RasterContext.Parameters;
	CreateSkipBarrierUAV(RasterParameters.OutDepthBuffer);
	CreateSkipBarrierUAV(RasterParameters.OutDepthBufferArray);
	CreateSkipBarrierUAV(RasterParameters.OutVisBuffer64);
	CreateSkipBarrierUAV(RasterParameters.OutDbgBuffer64);
	CreateSkipBarrierUAV(RasterParameters.OutDbgBuffer32);

	const ERDGPassFlags AsyncComputeFlag = (Scheduling == ERasterScheduling::HardwareAndSoftwareOverlap) ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;

	FIntRect ViewRect = {};
	ViewRect.Min = FIntPoint::ZeroValue;
	ViewRect.Max = RasterContext.TextureSize;

	if (IsUsingVirtualShadowMap())
	{
		ViewRect.Min = FIntPoint::ZeroValue;
		ViewRect.Max = FIntPoint(FVirtualShadowMap::PageSize, FVirtualShadowMap::PageSize) * FVirtualShadowMap::RasterWindowPages;
	}

	const bool bHasPrevDrawData = (RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA);
	if (!bHasPrevDrawData)
	{
		TotalPrevDrawClustersBuffer = DummyBuffer8;
	}

	const int32 PSOCollectorIndex = FPSOCollectorCreateManager::GetIndex(EShadingPath::Deferred, TEXT("NaniteRaster"));

	const auto CreatePassParameters = [&](const FBinningData& BinningData, bool bPatches)
	{
		auto* RasterPassParameters = GraphBuilder.AllocParameters<FRasterizePassParameters>();

		RasterPassParameters->NaniteRaster				= DispatchContext.RasterUniformBuffer;
		RasterPassParameters->ClusterPageData			= GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		RasterPassParameters->HierarchyBuffer			= GStreamingManager.GetHierarchySRV(GraphBuilder);
		RasterPassParameters->Scene						= SceneUniformBuffer;
		RasterPassParameters->RasterParameters			= RasterParameters;
		RasterPassParameters->VisibleClustersSWHW		= GraphBuilder.CreateSRV(VisibleClustersSWHW);
		RasterPassParameters->IndirectArgs				= BinningData.IndirectArgs;
		RasterPassParameters->InViews					= ViewsBuffer != nullptr ? GraphBuilder.CreateSRV(ViewsBuffer) : nullptr;
		RasterPassParameters->InClusterOffsetSWHW		= GraphBuilder.CreateSRV(ClusterOffsetSWHW, PF_R32_UINT);
		RasterPassParameters->InTotalPrevDrawClusters	= GraphBuilder.CreateSRV(TotalPrevDrawClustersBuffer);
		RasterPassParameters->RasterBinData				= GraphBuilder.CreateSRV(BinningData.DataBuffer);
		RasterPassParameters->RasterBinMeta				= GraphBuilder.CreateSRV(BinningData.MetaBuffer);

		RasterPassParameters->TessellationTable_Offsets	= GTessellationTable.Offsets.SRV;
		RasterPassParameters->TessellationTable_VertsAndIndexes	= GTessellationTable.VertsAndIndexes.SRV;

		RasterPassParameters->VirtualShadowMap			= VirtualTargetParameters;

		RasterPassParameters->OutStatsBuffer			= StatsBufferSkipBarrierUAV;

		if (bPatches)
		{
			RasterPassParameters->VisiblePatches		= GraphBuilder.CreateSRV(VisiblePatches);
			RasterPassParameters->VisiblePatchesArgs	= GraphBuilder.CreateSRV(VisiblePatchesArgs);
		}

		RasterPassParameters->SplitWorkQueue = SplitWorkQueue;
		CreateSkipBarrierUAV(RasterPassParameters->SplitWorkQueue.DataBuffer);
		CreateSkipBarrierUAV(RasterPassParameters->SplitWorkQueue.StateBuffer);

		return RasterPassParameters;
	};

	// Rasterizer Cluster Binning
	FBinningData ClusterBinning = AddPass_Binning(
		DispatchContext,
		HardwarePath,
		ClusterOffsetSWHW,
		nullptr,
		nullptr,
		SplitWorkQueue,
		bMainPass,
		ERDGPassFlags::Compute
	);

	if (ClusterBinning.DataBuffer == nullptr)
	{
		ClusterBinning.DataBuffer = DummyBuffer8;
	}

	if (ClusterBinning.MetaBuffer == nullptr)
	{
		ClusterBinning.MetaBuffer = DummyBufferRasterMeta;
	}

	const FRasterizePassParameters* ClusterPassParameters = CreatePassParameters(ClusterBinning, false /* Patches */);

	if (bTessellationEnabled)
	{
		// Always run SW tessellation first on graphics pipe
		FRDGPass* SWTessellatedPass = GraphBuilder.AddPass(
			RDG_EVENT_NAME("SW Rasterize (Tessellated)"),
			ClusterPassParameters,
			ERDGPassFlags::Compute,
			[ClusterPassParameters, &DispatchContext, &SceneView = SceneView, RenderFlags = RenderFlags, PSOCollectorIndex](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
			{
				if (DispatchContext.HasTessellated())
				{
					DispatchContext.DispatchSW(
						RHICmdList,
						DispatchContext.Dispatches_SW_Tessellated,
						SceneView,
						PSOCollectorIndex,
						*ClusterPassParameters,
						false /* Patches */
					);
				}
			}
		);

		GraphBuilder.SetPassWorkload(SWTessellatedPass, PassWorkload);
	}

	FRDGPass* HWTrianglesPass = GraphBuilder.AddPass(
		RDG_EVENT_NAME("HW Rasterize (Triangles)"),
		ClusterPassParameters,
		ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
		[ClusterPassParameters, &DispatchContext, ViewRect, &SceneView = SceneView, bMainPass, HardwarePath, PSOCollectorIndex, RenderFlags = RenderFlags](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			DispatchContext.DispatchHW(
				RHICmdList,
				DispatchContext.Dispatches_HW_Triangles,
				SceneView,
				ViewRect,
				HardwarePath,
				PSOCollectorIndex,
				*ClusterPassParameters
			);
		}
	);

	GraphBuilder.SetPassWorkload(HWTrianglesPass, PassWorkload);

	if (Scheduling != ERasterScheduling::HardwareOnly)
	{
		FRDGPass* SWTrianglesPass = GraphBuilder.AddPass(
			RDG_EVENT_NAME("SW Rasterize (Triangles)"),
			ClusterPassParameters,
			AsyncComputeFlag,
			[ClusterPassParameters, &DispatchContext, &SceneView = SceneView, RenderFlags = RenderFlags, PSOCollectorIndex](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
			{
				DispatchContext.DispatchSW(
					RHICmdList,
					DispatchContext.Dispatches_SW_Triangles,
					SceneView,
					PSOCollectorIndex,
					*ClusterPassParameters,
					false /* Patches */
				);
			}
		);

		GraphBuilder.SetPassWorkload(SWTrianglesPass, PassWorkload);
	}

	if (bTessellationEnabled)
	{
		// Ensure all dependent passes use the same queue
		const ERDGPassFlags PatchPassFlags = ERDGPassFlags::Compute;

		AddPass_PatchSplit(
			DispatchContext,
			SplitWorkQueue,
			OccludedPatches,
			VisiblePatches,
			VisiblePatchesArgs,
			bMainPass ? (Configuration.bTwoPassOcclusion ? CULLING_PASS_OCCLUSION_MAIN : CULLING_PASS_NO_OCCLUSION) : CULLING_PASS_OCCLUSION_POST,
			PatchPassFlags
		);

		FBinningData PatchBinning = AddPass_Binning(
			DispatchContext,
			HardwarePath,
			ClusterOffsetSWHW,
			VisiblePatches,
			VisiblePatchesArgs,
			SplitWorkQueue,
			bMainPass,
			PatchPassFlags
		);

		if (PatchBinning.DataBuffer == nullptr)
		{
			PatchBinning.DataBuffer = DummyBuffer8;
		}

		if (PatchBinning.MetaBuffer == nullptr)
		{
			PatchBinning.MetaBuffer = DummyBufferRasterMeta;
		}

		const FRasterizePassParameters* PatchPassParameters = CreatePassParameters(PatchBinning, true /* Patches */);

		FRDGPass* SWPatchesPass = GraphBuilder.AddPass(
			RDG_EVENT_NAME("SW Rasterize (Patches)"),
			PatchPassParameters,
			PatchPassFlags,
			[PatchPassParameters, &DispatchContext, &SceneView = SceneView, RenderFlags = RenderFlags, PSOCollectorIndex](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
			{
				DispatchContext.DispatchSW(
					RHICmdList,
					DispatchContext.Dispatches_SW_Tessellated,
					SceneView,
					PSOCollectorIndex,
					*PatchPassParameters,
					true /* Patches */
				);
			}
		);

		GraphBuilder.SetPassWorkload(SWPatchesPass, PassWorkload);
	}

	return ClusterBinning;
}

BEGIN_SHADER_PARAMETER_STRUCT(FClearVisiblePatchesUAVParameters, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, VisiblePatchesArgsUAV)
END_SHADER_PARAMETER_STRUCT()

void FRenderer::AddPass_PatchSplit(
	const FDispatchContext& DispatchContext,
	const FGlobalWorkQueueParameters& SplitWorkQueue,
	const FGlobalWorkQueueParameters& OccludedPatches,
	FRDGBufferRef VisiblePatches,
	FRDGBufferRef VisiblePatchesArgs,
	uint32 CullingPass,
	ERDGPassFlags PassFlags
)
{
	if (!UseNaniteTessellation())
	{
		return;
	}

	// Clear visible patches args
	{
		FRDGBufferUAVRef VisiblePatchesArgsUAV = GraphBuilder.CreateUAV(VisiblePatchesArgs);

		FClearVisiblePatchesUAVParameters* Parameters = GraphBuilder.AllocParameters<FClearVisiblePatchesUAVParameters>();
		Parameters->VisiblePatchesArgsUAV = VisiblePatchesArgsUAV;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ClearVisiblePatchesArgs"),
			Parameters,
			PassFlags,
			[Parameters, &DispatchContext, VisiblePatchesArgsUAV](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
			{
				if (DispatchContext.HasTessellated())
				{
					RHICmdList.ClearUAVUint(VisiblePatchesArgsUAV->GetRHI(), FUintVector4(0u, 0u, 0u, 0u));
					VisiblePatchesArgsUAV->MarkResourceAsUsed();
				}
			}
		);
	}

	{
		FPatchSplitCS::FParameters Parameters;

		Parameters.View							= SceneView.ViewUniformBuffer;
		Parameters.NaniteRaster					= DispatchContext.RasterUniformBuffer;
		Parameters.ClusterPageData				= GStreamingManager.GetClusterPageDataSRV( GraphBuilder );
		Parameters.HierarchyBuffer				= GStreamingManager.GetHierarchySRV( GraphBuilder );
		Parameters.Scene						= SceneUniformBuffer;
		Parameters.CullingParameters			= CullingParameters;
		Parameters.SplitWorkQueue				= SplitWorkQueue;
		Parameters.OccludedPatches				= OccludedPatches;

		Parameters.VisibleClustersSWHW			= GraphBuilder.CreateSRV( VisibleClustersSWHW );

		Parameters.TessellationTable_Offsets			= GTessellationTable.Offsets.SRV;
		Parameters.TessellationTable_VertsAndIndexes	= GTessellationTable.VertsAndIndexes.SRV;

		Parameters.RWVisiblePatches			= GraphBuilder.CreateUAV( VisiblePatches );
		Parameters.RWVisiblePatchesArgs		= GraphBuilder.CreateUAV( VisiblePatchesArgs );
		Parameters.VisiblePatchesSize		= VisiblePatches->GetSize() / 16;

		Parameters.OutStatsBuffer			= GNaniteShowStats != 0u ? StatsBufferSkipBarrierUAV : nullptr;

		if (VirtualShadowMapArray)
		{
			Parameters.VirtualShadowMap		= VirtualTargetParameters;
		}

		FPatchSplitCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FPatchSplitCS::FCullingPassDim >( CullingPass );
		PermutationVector.Set< FPatchSplitCS::FMultiViewDim >( bMultiView );
		PermutationVector.Set< FPatchSplitCS::FVirtualTextureTargetDim >( VirtualShadowMapArray != nullptr );
		PermutationVector.Set< FPatchSplitCS::FSplineDeformDim >( NaniteSplineMeshesSupported() );
		PermutationVector.Set< FPatchSplitCS::FSkinningDim >( NaniteSkinnedMeshesSupported() );
		PermutationVector.Set< FPatchSplitCS::FWriteStatsDim >( GNaniteShowStats != 0u );

		auto ComputeShader = SharedContext.ShaderMap->GetShader< FPatchSplitCS >( PermutationVector );
		
		FRDGBufferRef PatchSplitArgs0 = GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateIndirectDesc( ( NANITE_TESSELLATION_MAX_PATCH_SPLIT_LEVELS + 1 ) * NANITE_NODE_CULLING_ARG_COUNT ), TEXT("Nanite.PatchSplitArgs0") );
		FRDGBufferRef PatchSplitArgs1 = GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateIndirectDesc( ( NANITE_TESSELLATION_MAX_PATCH_SPLIT_LEVELS + 1 ) * NANITE_NODE_CULLING_ARG_COUNT ), TEXT("Nanite.PatchSplitArgs1") );

		{
			RDG_EVENT_SCOPE( GraphBuilder, "PatchSplit" );

			{
				FInitPatchSplitArgs_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitPatchSplitArgs_CS::FParameters >();

				PassParameters->NaniteRaster = DispatchContext.RasterUniformBuffer;
				PassParameters->SplitWorkQueue = SplitWorkQueue;
				PassParameters->OutPatchSplitArgs0 = GraphBuilder.CreateUAV( PatchSplitArgs0 );
				PassParameters->OutPatchSplitArgs1 = GraphBuilder.CreateUAV( PatchSplitArgs1 );
			
				auto InitComputeShader = SharedContext.ShaderMap->GetShader< FInitPatchSplitArgs_CS >();
				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("InitPatchSplitArgs"),
					InitComputeShader,
					PassParameters,
					FIntVector(2, 1, 1)
				);
			}

			for( uint32 Level = 0; Level < NANITE_TESSELLATION_MAX_PATCH_SPLIT_LEVELS; Level++ )
			{
				FPatchSplitCS::FParameters* PassParameters = GraphBuilder.AllocParameters< FPatchSplitCS::FParameters >();
				*PassParameters = Parameters;

				FRDGBufferRef CurrentIndirectArgs	= ( Level & 1 ) ? PatchSplitArgs1 : PatchSplitArgs0;
				FRDGBufferRef NextIndirectArgs		= ( Level & 1 ) ? PatchSplitArgs0 : PatchSplitArgs1;

				PassParameters->Level				= Level;
				PassParameters->CurrentIndirectArgs = GraphBuilder.CreateSRV( CurrentIndirectArgs );
				PassParameters->NextIndirectArgs	= GraphBuilder.CreateUAV( NextIndirectArgs );
				PassParameters->IndirectArgs		= CurrentIndirectArgs;

				ClearUnusedGraphResources( ComputeShader, PassParameters );

				GraphBuilder.AddPass(
					RDG_EVENT_NAME( "PatchSplit_%d", Level ),
					PassParameters,
					PassFlags,
					[ PassParameters, &DispatchContext, ComputeShader, Level ]( FRDGAsyncTask, FRHIComputeCommandList& RHICmdList )
					{
						if (DispatchContext.HasTessellated())
						{
							FComputeShaderUtils::DispatchIndirect(
								RHICmdList,
								ComputeShader,
								*PassParameters,
								PassParameters->IndirectArgs->GetIndirectRHICallBuffer(),
								Level * NANITE_NODE_CULLING_ARG_COUNT * sizeof(uint32)
								);
						}
					}
				);
			}
		}
	}

	{
		FInitVisiblePatchesArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitVisiblePatchesArgsCS::FParameters >();

		PassParameters->RWVisiblePatchesArgs	= GraphBuilder.CreateUAV( VisiblePatchesArgs );
		PassParameters->MaxVisiblePatches		= FGlobalResources::GetMaxVisiblePatches();
		
		auto ComputeShader = SharedContext.ShaderMap->GetShader< FInitVisiblePatchesArgsCS >();
		ClearUnusedGraphResources(ComputeShader, PassParameters);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("InitVisiblePatchesArgs"),
			PassParameters,
			PassFlags,
			[PassParameters, &DispatchContext, ComputeShader](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
			{
				if (DispatchContext.HasTessellated())
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, FIntVector(1, 1, 1));
				}
			}
		);
	}
}

void AddClearVisBufferPass(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const EPixelFormat PixelFormat64,
	const FRasterContext& RasterContext,
	const FIntRect& TextureRect,
	bool bClearTarget,
	FRDGBufferSRVRef RectMinMaxBufferSRV,
	uint32 NumRects,
	FRDGTextureRef ExternalDepthBuffer)
{
	if (!bClearTarget)
	{
		return;
	}

	const bool bUseFastClear = CVarNaniteFastVisBufferClear.GetValueOnRenderThread() != 0 && (RectMinMaxBufferSRV == nullptr && NumRects == 0 && ExternalDepthBuffer == nullptr);
	if (bUseFastClear)
	{
		// TODO: Don't currently support offset views.
		checkf(TextureRect.Min.X == 0 && TextureRect.Min.Y == 0, TEXT("Viewport offset support is not implemented."));

		const bool bTiled = (CVarNaniteFastVisBufferClear.GetValueOnRenderThread() == 2);

		FRasterClearCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRasterClearCS::FParameters>();
		PassParameters->ClearRect = FUint32Vector4((uint32)TextureRect.Min.X, (uint32)TextureRect.Min.Y, (uint32)TextureRect.Max.X, (uint32)TextureRect.Max.Y);
		PassParameters->RasterParameters = RasterContext.Parameters;

		FRasterClearCS::FPermutationDomain PermutationVectorCS;
		PermutationVectorCS.Set<FRasterClearCS::FClearDepthDim>(RasterContext.RasterMode == EOutputBufferMode::DepthOnly);
		PermutationVectorCS.Set<FRasterClearCS::FClearDebugDim>(RasterContext.VisualizeActive);
		PermutationVectorCS.Set<FRasterClearCS::FClearTiledDim>(bTiled);
		auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterClearCS>(PermutationVectorCS);

		const FIntPoint ClearSize(TextureRect.Width(), TextureRect.Height());
		const FIntVector DispatchDim = FComputeShaderUtils::GetGroupCount(ClearSize, bTiled ? 32 : 8);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("RasterClear"),
			ComputeShader,
			PassParameters,
			DispatchDim
		);
	}
	else
	{
		const uint32 ClearValue[4] = { 0, 0, 0, 0 };

		TArray<FRDGTextureUAVRef, TInlineAllocator<3>> BufferClearList;
		if (RasterContext.RasterMode == EOutputBufferMode::DepthOnly)
		{
			BufferClearList.Add(RasterContext.Parameters.OutDepthBuffer);
		}
		else
		{
			BufferClearList.Add(RasterContext.Parameters.OutVisBuffer64);

			if (RasterContext.VisualizeActive)
			{
				BufferClearList.Add(RasterContext.Parameters.OutDbgBuffer64);
				BufferClearList.Add(RasterContext.Parameters.OutDbgBuffer32);
			}
		}

		for (FRDGTextureUAVRef UAVRef : BufferClearList)
		{
			AddClearUAVPass(GraphBuilder, SharedContext.FeatureLevel, UAVRef, ClearValue, RectMinMaxBufferSRV, NumRects);
		}
	}
}

FRasterContext InitRasterContext(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const FViewFamilyInfo& ViewFamily,
	FIntPoint TextureSize,
	FIntRect TextureRect,
	EOutputBufferMode RasterMode,
	bool bClearTarget,
	bool bAsyncCompute,
	FRDGBufferSRVRef RectMinMaxBufferSRV,
	uint32 NumRects,
	FRDGTextureRef ExternalDepthBuffer,
	bool bCustomPass,
	bool bVisualize,
	bool bVisualizeOverdraw
)
{
	// If an external depth buffer is provided, it must match the context size
	check( ExternalDepthBuffer == nullptr || ExternalDepthBuffer->Desc.Extent == TextureSize );
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::InitContext");

	FRasterContext RasterContext{};

	RasterContext.bCustomPass = bCustomPass;
	RasterContext.VisualizeActive = bVisualize;
	RasterContext.VisualizeModeOverdraw = bVisualize && bVisualizeOverdraw;
	RasterContext.TextureSize = TextureSize;

	// Set rasterizer scheduling based on config and platform capabilities.
	if (CVarNaniteComputeRasterization.GetValueOnRenderThread() != 0)
	{
		bAsyncCompute = bAsyncCompute
			&& GSupportsEfficientAsyncCompute
			&& (CVarNaniteEnableAsyncRasterization.GetValueOnRenderThread() != 0) 
			&& EnumHasAnyFlags(GRHIMultiPipelineMergeableAccessMask, ERHIAccess::UAVMask)
			&& !(bCustomPass && !UseAsyncComputeForCustomPass(ViewFamily));
		
		RasterContext.RasterScheduling = bAsyncCompute ? ERasterScheduling::HardwareAndSoftwareOverlap : ERasterScheduling::HardwareThenSoftware;
	}
	else
	{
		// Force hardware-only rasterization.
		RasterContext.RasterScheduling = ERasterScheduling::HardwareOnly;
	}

	RasterContext.RasterMode = RasterMode;

	const EPixelFormat PixelFormat64 = GPixelFormats[PF_R64_UINT].Supported ? PF_R64_UINT : PF_R32G32_UINT;

	RasterContext.DepthBuffer	= ExternalDepthBuffer ? ExternalDepthBuffer :
								  GraphBuilder.CreateTexture( FRDGTextureDesc::Create2D(RasterContext.TextureSize, PF_R32_UINT, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_AtomicCompatible), TEXT("Nanite.DepthBuffer32") );
	RasterContext.VisBuffer64	= GraphBuilder.CreateTexture( FRDGTextureDesc::Create2D(RasterContext.TextureSize, PixelFormat64, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV | ETextureCreateFlags::Atomic64Compatible), TEXT("Nanite.VisBuffer64") );
	RasterContext.DbgBuffer64	= GraphBuilder.CreateTexture( FRDGTextureDesc::Create2D(RasterContext.TextureSize, PixelFormat64, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV | ETextureCreateFlags::Atomic64Compatible), TEXT("Nanite.DbgBuffer64") );
	RasterContext.DbgBuffer32	= GraphBuilder.CreateTexture( FRDGTextureDesc::Create2D(RasterContext.TextureSize, PF_R32_UINT, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_AtomicCompatible), TEXT("Nanite.DbgBuffer32") );

	if (RasterContext.RasterMode == EOutputBufferMode::DepthOnly)
	{
		if (!UseAsyncComputeForShadowMaps(ViewFamily) && RasterContext.RasterScheduling == ERasterScheduling::HardwareAndSoftwareOverlap)
		{
			RasterContext.RasterScheduling = ERasterScheduling::HardwareThenSoftware;
		}

		if (RasterContext.DepthBuffer->Desc.Dimension == ETextureDimension::Texture2DArray)
		{
			RasterContext.Parameters.OutDepthBufferArray = GraphBuilder.CreateUAV(RasterContext.DepthBuffer);
			check(!bClearTarget); // Clearing is not required; this path is only used with VSMs.
		}
		else
		{
			RasterContext.Parameters.OutDepthBuffer = GraphBuilder.CreateUAV(RasterContext.DepthBuffer);
		}
	}
	else
	{
		RasterContext.Parameters.OutVisBuffer64 = GraphBuilder.CreateUAV(RasterContext.VisBuffer64);
		
		if (RasterContext.VisualizeActive)
		{
			RasterContext.Parameters.OutDbgBuffer64 = GraphBuilder.CreateUAV(RasterContext.DbgBuffer64);
			RasterContext.Parameters.OutDbgBuffer32 = GraphBuilder.CreateUAV(RasterContext.DbgBuffer32);
		}
	}

	AddClearVisBufferPass(
		GraphBuilder,
		SharedContext,
		PixelFormat64,
		RasterContext,
		TextureRect,
		bClearTarget,
		RectMinMaxBufferSRV,
		NumRects,
		ExternalDepthBuffer
	);

	return RasterContext;
}

template< typename FInit >
static FRDGBufferRef CreateBufferOnce( FRDGBuilder& GraphBuilder, TRefCountPtr<FRDGPooledBuffer>& Buffer, const FRDGBufferDesc& Desc, const TCHAR* Name, FInit Init )
{
	FRDGBufferRef BufferRDG;
	if( Buffer.IsValid() && Buffer->Desc == Desc )
	{
		BufferRDG = GraphBuilder.RegisterExternalBuffer( Buffer, Name );
	}
	else
	{
		RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());
		BufferRDG = GraphBuilder.CreateBuffer( Desc, Name );
		Buffer = GraphBuilder.ConvertToExternalBuffer( BufferRDG );
		Init( BufferRDG );
	}

	return BufferRDG;
}

static FRDGBufferRef CreateBufferOnce( FRDGBuilder& GraphBuilder, TRefCountPtr<FRDGPooledBuffer>& Buffer, const FRDGBufferDesc& Desc, const TCHAR* Name, uint32 ClearValue )
{
	return CreateBufferOnce( GraphBuilder, Buffer, Desc, Name,
		[ &GraphBuilder, ClearValue ]( FRDGBufferRef Buffer )
		{
			AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( Buffer ), ClearValue );
		} );
}

// Helper to upload CPU view array
void FRenderer::DrawGeometry(
	FNaniteRasterPipelines& RasterPipelines,
	const FNaniteVisibilityQuery* VisibilityQuery,
	const FPackedViewArray& ViewArray,
	FSceneInstanceCullingQuery* OptionalSceneInstanceCullingQuery,
	const TConstArrayView<FInstanceDraw>* OptionalInstanceDraws)
{
	check(ViewArray.NumViews > 0);

	if (ViewArray.NumViews > NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS)
	{
		UE_LOG(LogRenderer, Warning, TEXT("Nanite view overflow detected: %d / %d."), ViewArray.NumViews, NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS);
	}

	const uint32 ViewsBufferElements = FMath::RoundUpToPowerOfTwo(ViewArray.NumViews);
	FRDGBufferRef ViewsBufferUpload = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("Nanite.Views"),
		sizeof(FPackedView),
		[ViewsBufferElements] { return ViewsBufferElements; },
		[&ViewArray] { return ViewArray.GetViews().GetData(); },
		[&ViewArray] { return ViewArray.GetViews().Num() * sizeof(FPackedView); }
	);

	FRDGBufferRef ViewDrawRanges = nullptr;
	if (OptionalSceneInstanceCullingQuery)
	{
		ViewDrawRanges = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("Nanite.ViewDrawRanges"),
			OptionalSceneInstanceCullingQuery->GetViewDrawGroups());
	}

	DrawGeometry(RasterPipelines,
		VisibilityQuery,
		ViewsBufferUpload,
		ViewDrawRanges,
		ViewArray.NumViews,
		OptionalSceneInstanceCullingQuery,
		OptionalInstanceDraws);
}

void FRenderer::DrawGeometry(
	FNaniteRasterPipelines& RasterPipelines,
	const FNaniteVisibilityQuery* VisibilityQuery,
	FRDGBufferRef InViewsBuffer,
	FRDGBufferRef InViewDrawRanges,
	int32 NumViews,
	FSceneInstanceCullingQuery* SceneInstanceCullingQuery,
	const TConstArrayView<FInstanceDraw>* OptionalInstanceDraws
)
{
	LLM_SCOPE_BYTAG(Nanite);
		
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::DrawGeometry");

	// Use multiview path unless we know for sure it's a single CPU-provided view
	bMultiView = (NumViews != 1);

	check(!Nanite::GStreamingManager.IsAsyncUpdateInProgress());
	// It is not possible to drive rendering from both an explicit list and instance culling at the same time.
	check(!(SceneInstanceCullingQuery != nullptr && OptionalInstanceDraws != nullptr));
	// Calling CullRasterize more than once is illegal unless bSupportsMultiplePasses is enabled.
	check(DrawPassIndex == 0 || Configuration.bSupportsMultiplePasses);
	// VSMs should always be using the multiview path
	check(!IsUsingVirtualShadowMap() || bMultiView);

	const bool bTessellationEnabled = UseNaniteTessellation() && (RenderFlags & NANITE_RENDER_FLAG_DISABLE_PROGRAMMABLE) == 0u;

	ViewsBuffer = InViewsBuffer;

	if (OptionalInstanceDraws)
	{
		const uint32 InstanceDrawsBufferElements = FMath::RoundUpToPowerOfTwo(OptionalInstanceDraws->Num());
		InstanceDrawsBuffer = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("Nanite.InstanceDraws"),
			OptionalInstanceDraws->GetTypeSize(),
			InstanceDrawsBufferElements,
			OptionalInstanceDraws->GetData(),
			OptionalInstanceDraws->Num() * OptionalInstanceDraws->GetTypeSize()
		);
		NumInstancesPreCull = OptionalInstanceDraws->Num();
	}
	else
	{
		NumInstancesPreCull = Scene.GPUScene.GetInstanceIdUpperBoundGPU();
	}

	{
		CullingParameters.InViews						= GraphBuilder.CreateSRV(ViewsBuffer);
		CullingParameters.NumViews						= NumViews;		// See above - not used in most paths
		CullingParameters.HZBTexture					= RegisterExternalTextureWithFallback(GraphBuilder, PrevHZB, GSystemTextures.BlackDummy);
		CullingParameters.HZBSize						= PrevHZB ? PrevHZB->GetDesc().Extent : FVector2f(0.0f);
		CullingParameters.HZBSampler					= TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI();
		CullingParameters.PageConstants					= PageConstants;
		CullingParameters.MaxCandidateClusters			= Nanite::FGlobalResources::GetMaxCandidateClusters();
		CullingParameters.MaxVisibleClusters			= Nanite::FGlobalResources::GetMaxVisibleClusters();
		CullingParameters.RenderFlags					= RenderFlags;
		CullingParameters.DebugFlags					= DebugFlags;
	}

	if (VirtualShadowMapArray != nullptr)
	{
		VirtualTargetParameters.VirtualShadowMap = VirtualShadowMapArray->GetUniformBuffer(0); // This pass does not require per-view VSM data
		
		// HZB (if provided) comes from the previous frame, so we need last frame's page table
		// Dummy data, but matches the expected format
		auto HZBPageTableRDG		= VirtualShadowMapArray->PageTableRDG;
		auto HZBPageRectBoundsRDG	= VirtualShadowMapArray->UncachedPageRectBoundsRDG;
		auto HZBPageFlagsRDG		= VirtualShadowMapArray->PageFlagsRDG;

		if (PrevHZB)
		{
			check( VirtualShadowMapArray->CacheManager );
			const FVirtualShadowMapArrayFrameData& PrevBuffers = VirtualShadowMapArray->CacheManager->GetPrevBuffers();
			HZBPageTableRDG			= GraphBuilder.RegisterExternalTexture( PrevBuffers.PageTable,				TEXT("Shadow.Virtual.HZBPageTable") );
			HZBPageRectBoundsRDG	= GraphBuilder.RegisterExternalBuffer( PrevBuffers.UncachedPageRectBounds,	TEXT("Shadow.Virtual.HZBPageRectBounds") );
			HZBPageFlagsRDG			= GraphBuilder.RegisterExternalTexture( PrevBuffers.PageFlags,				TEXT("Shadow.Virtual.HZBPageFlags") );
		}
		CullingParameters.HZBTextureArray = RegisterExternalTextureWithFallback(GraphBuilder, PrevHZB, GSystemTextures.BlackArrayDummy);
		VirtualTargetParameters.HZBPageTable		= HZBPageTableRDG;
		VirtualTargetParameters.HZBPageRectBounds	= GraphBuilder.CreateSRV( HZBPageRectBoundsRDG );
		VirtualTargetParameters.HZBPageFlags		= HZBPageFlagsRDG;

		VirtualTargetParameters.OutDirtyPageFlags				= GraphBuilder.CreateUAV(VirtualShadowMapArray->DirtyPageFlagsRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
	}

	InstanceHierarchyDriver.Init(GraphBuilder, true, Configuration.bTwoPassOcclusion, SharedContext.ShaderMap, SceneInstanceCullingQuery, InViewDrawRanges);

	{
		FNaniteStats Stats;
		FMemory::Memzero(Stats);
		// The main pass instances are produced on the GPU if the hierarchy is active.
		if (IsDebuggingEnabled() && !InstanceHierarchyDriver.IsEnabled())
		{
			Stats.NumMainInstancesPreCull =  NumInstancesPreCull;
		}

		StatsBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("Nanite.StatsBuffer"), sizeof(FNaniteStats), 1, &Stats, sizeof(FNaniteStats));
		StatsBufferSkipBarrierUAV = GraphBuilder.CreateUAV(StatsBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
	}

	{
		FInitArgs_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitArgs_CS::FParameters >();

		PassParameters->RenderFlags = CullingParameters.RenderFlags;

		PassParameters->OutQueueState						= GraphBuilder.CreateUAV( QueueState );
		PassParameters->InOutMainPassRasterizeArgsSWHW		= GraphBuilder.CreateUAV( MainRasterizeArgsSWHW );

		uint32 ClampedDrawPassIndex = FMath::Min(DrawPassIndex, 2u);

		if (Configuration.bTwoPassOcclusion)
		{
			PassParameters->OutOccludedInstancesArgs		= GraphBuilder.CreateUAV( OccludedInstancesArgs );
			PassParameters->InOutPostPassRasterizeArgsSWHW	= GraphBuilder.CreateUAV( PostRasterizeArgsSWHW );
		}
		
		check(DrawPassIndex == 0 || RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA); // sanity check
		if (RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA)
		{
			PassParameters->InOutTotalPrevDrawClusters = GraphBuilder.CreateUAV(TotalPrevDrawClustersBuffer);
		}
		else
		{
			// Use any UAV just to keep render graph happy that something is bound, but the shader doesn't actually touch this.
			PassParameters->InOutTotalPrevDrawClusters = PassParameters->OutQueueState;
		}

		FInitArgs_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FInitArgs_CS::FOcclusionCullingDim>(Configuration.bTwoPassOcclusion);
		PermutationVector.Set<FInitArgs_CS::FDrawPassIndexDim>( ClampedDrawPassIndex );
		
		auto ComputeShader = SharedContext.ShaderMap->GetShader< FInitArgs_CS >( PermutationVector );

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME( "InitArgs" ),
			ComputeShader,
			PassParameters,
			FIntVector( 1, 1, 1 )
		);
	}

	// Initialize node and cluster batch arrays.
	{
		const uint32 MaxNodes				=	Nanite::FGlobalResources::GetMaxNodes();
		const uint32 MaxClusterBatches		=	Nanite::FGlobalResources::GetMaxClusterBatches();

		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(4, MaxClusterBatches * 2 + MaxNodes * (2 + 3));
		Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_ByteAddressBuffer);

		const TCHAR* BufferName = TEXT("Nanite.MainAndPostNodesAndClusterBatchesBuffer");
		if(CVarNanitePersistentThreadsCulling.GetValueOnRenderThread())
		{
			// They only have to be initialized once as the culling code reverts nodes/batches to their cleared state after they have been consumed.
			MainAndPostNodesAndClusterBatchesBuffer = CreateBufferOnce( GraphBuilder, GGlobalResources.MainAndPostNodesAndClusterBatchesBuffer.Buffer, Desc, BufferName,
				[&]( FRDGBufferRef Buffer )
				{
					AddPassInitNodesAndClusterBatchesUAV( GraphBuilder, SharedContext.ShaderMap, GraphBuilder.CreateUAV( Buffer ) );
	
					GGlobalResources.MainAndPostNodesAndClusterBatchesBuffer.NumNodes			= MaxNodes;
					GGlobalResources.MainAndPostNodesAndClusterBatchesBuffer.NumClusterBatches	= MaxClusterBatches;
				} );
		}
		else
		{
			// Clear any persistent buffer and allocate a temporary one
			GGlobalResources.MainAndPostNodesAndClusterBatchesBuffer = FNodesAndClusterBatchesBuffer();
			MainAndPostNodesAndClusterBatchesBuffer = GraphBuilder.CreateBuffer(Desc, BufferName);
		}
	}

	// Allocate candidate cluster buffer. Lifetime only duration of DrawGeometry
	MainAndPostCandidateClustersBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateByteAddressDesc(
			Nanite::FGlobalResources::GetMaxCandidateClusters() * NANITE_CANDIDATE_CLUSTER_SIZE_DWORDS * 4	
		),
		TEXT("Nanite.MainAndPostCandidateClustersBuffer")
	);

	FGlobalWorkQueueParameters SplitWorkQueue;
	FGlobalWorkQueueParameters OccludedPatches;

	FRDGBufferRef VisiblePatches = nullptr;
	FRDGBufferRef VisiblePatchesMainArgs = nullptr;
	FRDGBufferRef VisiblePatchesPostArgs = nullptr;

	// Tessellation
	if (bTessellationEnabled)
	{
		FRDGBufferDesc CandidateDesc = FRDGBufferDesc::CreateByteAddressDesc( 16 * FGlobalResources::GetMaxCandidatePatches() );
		FRDGBufferDesc VisibleDesc   = FRDGBufferDesc::CreateByteAddressDesc( 16 * FGlobalResources::GetMaxVisiblePatches() );

		FRDGBufferRef SplitWorkQueue_DataBuffer  = GraphBuilder.CreateBuffer( CandidateDesc, TEXT("Nanite.SplitWorkQueue.DataBuffer") );
		FRDGBufferRef OccludedPatches_DataBuffer = GraphBuilder.CreateBuffer( CandidateDesc, TEXT("Nanite.OccludedPatches.DataBuffer") );

		FRDGBufferRef SplitWorkQueue_StateBuffer	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateStructuredDesc( 3 * sizeof(uint32), 1 ), TEXT("Nanite.SplitWorkQueue.StateBuffer") );
		FRDGBufferRef OccludedPatches_StateBuffer	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateStructuredDesc( 3 * sizeof(uint32), 1 ), TEXT("Nanite.OccludedPatches.StateBuffer") );

		SplitWorkQueue.DataBuffer	= GraphBuilder.CreateUAV( SplitWorkQueue_DataBuffer );
		SplitWorkQueue.StateBuffer	= GraphBuilder.CreateUAV( SplitWorkQueue_StateBuffer );

		OccludedPatches.DataBuffer	= GraphBuilder.CreateUAV( OccludedPatches_DataBuffer );
		OccludedPatches.StateBuffer	= GraphBuilder.CreateUAV( OccludedPatches_StateBuffer );

		AddClearUAVPass( GraphBuilder, SplitWorkQueue.StateBuffer, 0 );
		AddClearUAVPass( GraphBuilder, OccludedPatches.StateBuffer, 0 );

		VisiblePatches			= GraphBuilder.CreateBuffer( VisibleDesc,							TEXT("Nanite.VisiblePatches") );
		VisiblePatchesMainArgs	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateIndirectDesc(4),	TEXT("Nanite.VisiblePatchesMainArgs") );
		VisiblePatchesPostArgs	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateIndirectDesc(4),	TEXT("Nanite.VisiblePatchesPostArgs") );
	}

	// Per-view primitive filtering
	AddPass_PrimitiveFilter();
	
	FBinningData MainPassBinning{};
	FBinningData PostPassBinning{};

	FDispatchContext& DispatchContext = *GraphBuilder.AllocObject<FDispatchContext>();
	PrepareRasterizerPasses(
		DispatchContext,
		GetRasterHardwarePath(Scene.GetShaderPlatform(), SharedContext.Pipeline),
		Scene.GetFeatureLevel(),
		RasterPipelines,
		VisibilityQuery,
		RasterContext.bCustomPass,
		Configuration.bIsLumenCapture
	);

	// NaniteRaster Uniform Buffer
	{
		FNaniteRasterUniformParameters* UniformParameters	= GraphBuilder.AllocParameters<FNaniteRasterUniformParameters>();
		UniformParameters->PageConstants					= PageConstants;
		UniformParameters->MaxNodes							= Nanite::FGlobalResources::GetMaxNodes();
		UniformParameters->MaxVisibleClusters				= Nanite::FGlobalResources::GetMaxVisibleClusters();
		UniformParameters->MaxCandidatePatches				= Nanite::FGlobalResources::GetMaxCandidatePatches();
		UniformParameters->InvDiceRate						= CVarNaniteMaxPixelsPerEdge.GetValueOnRenderThread() / CVarNaniteDicingRate.GetValueOnRenderThread();
		UniformParameters->MaxPatchesPerGroup				= GetMaxPatchesPerGroup();
		UniformParameters->MeshPass							= GetMeshPass(Configuration);
		UniformParameters->RenderFlags						= RenderFlags;
		UniformParameters->DebugFlags						= DebugFlags;
		DispatchContext.RasterUniformBuffer					= GraphBuilder.CreateUniformBuffer(UniformParameters);
	}

	// No Occlusion Pass / Occlusion Main Pass
	{
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, !Configuration.bTwoPassOcclusion, "NoOcclusionPass");
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Configuration.bTwoPassOcclusion, "MainPass");

		AddPass_InstanceHierarchyAndClusterCull( Configuration.bTwoPassOcclusion ? CULLING_PASS_OCCLUSION_MAIN : CULLING_PASS_NO_OCCLUSION );

		MainPassBinning = AddPass_Rasterize(
			DispatchContext,
			SafeMainRasterizeArgsSWHW,
			VisiblePatches,
			VisiblePatchesMainArgs,
			SplitWorkQueue,
			OccludedPatches,
			true
		);
	}
	
	// Occlusion post pass. Retest instances and clusters that were not visible last frame. If they are visible now, render them.
	if (Configuration.bTwoPassOcclusion)
	{
		// Build a closest HZB with previous frame occluders to test remainder occluders against.
		if (VirtualShadowMapArray)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "BuildPreviousOccluderHZB(VSM)");
			VirtualShadowMapArray->UpdateHZB(GraphBuilder);
			CullingParameters.HZBTextureArray = VirtualShadowMapArray->HZBPhysicalArrayRDG;
			CullingParameters.HZBSize = CullingParameters.HZBTexture->Desc.Extent;

			VirtualTargetParameters.HZBPageTable		= VirtualShadowMapArray->PageTableRDG;
			VirtualTargetParameters.HZBPageRectBounds	= GraphBuilder.CreateSRV( VirtualShadowMapArray->UncachedPageRectBoundsRDG );
			VirtualTargetParameters.HZBPageFlags		= VirtualShadowMapArray->PageFlagsRDG;
		}
		else
		{
			RDG_EVENT_SCOPE(GraphBuilder, "BuildPreviousOccluderHZB");
			
			FSceneTextureParameters SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneView);

			FRDGTextureRef SceneDepth = SceneTextures.SceneDepthTexture;
			FRDGTextureRef RasterizedDepth = RasterContext.VisBuffer64;

			if (RasterContext.RasterMode == EOutputBufferMode::DepthOnly)
			{
				SceneDepth = GraphBuilder.RegisterExternalTexture( GSystemTextures.BlackDummy );
				RasterizedDepth = RasterContext.DepthBuffer;
			}

			FRDGTextureRef OutFurthestHZBTexture;

			BuildHZBFurthest(
				GraphBuilder,
				SceneDepth,
				RasterizedDepth,
				HZBBuildViewRect,
				Scene.GetFeatureLevel(),
				Scene.GetShaderPlatform(),
				TEXT("Nanite.PreviousOccluderHZB"),
				/* OutFurthestHZBTexture = */ &OutFurthestHZBTexture);

			CullingParameters.HZBTexture = OutFurthestHZBTexture;
			CullingParameters.HZBSize = CullingParameters.HZBTexture->Desc.Extent;
		}

		SplitWorkQueue = OccludedPatches;

		RDG_EVENT_SCOPE(GraphBuilder, "PostPass");
		// Post Pass
		AddPass_InstanceHierarchyAndClusterCull( CULLING_PASS_OCCLUSION_POST );

		// Render post pass
		PostPassBinning = AddPass_Rasterize(
			DispatchContext,
			SafePostRasterizeArgsSWHW,
			VisiblePatches,
			VisiblePatchesPostArgs,
			SplitWorkQueue,
			OccludedPatches,
			false
		);
	}

	if (RasterContext.RasterMode != EOutputBufferMode::DepthOnly)
	{
		// Pass index and number of clusters rendered in previous passes are irrelevant for depth-only rendering.
		DrawPassIndex++;
		RenderFlags |= NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA;
	}

	if (VirtualShadowMapArray != nullptr && Configuration.bExtractVSMPerformanceFeedback)
	{
		ExtractVSMPerformanceFeedback();
	}

	if( Configuration.bExtractStats )
	{
		ExtractStats( MainPassBinning, PostPassBinning );
	}

	RasterBinMetaBuffer = DispatchContext.MetaBuffer;

	FeedbackStatus();
}


void FRenderer::ExtractResults( FRasterResults& RasterResults )
{
	LLM_SCOPE_BYTAG(Nanite);

	RasterResults.PageConstants			= PageConstants;
	RasterResults.MaxVisibleClusters	= Nanite::FGlobalResources::GetMaxVisibleClusters();
	RasterResults.MaxCandidatePatches	= Nanite::FGlobalResources::GetMaxCandidatePatches();
	RasterResults.MaxNodes				= Nanite::FGlobalResources::GetMaxNodes();
	RasterResults.RenderFlags			= RenderFlags;
	RasterResults.DebugFlags			= DebugFlags;

	RasterResults.InvDiceRate			= CVarNaniteMaxPixelsPerEdge.GetValueOnRenderThread() / CVarNaniteDicingRate.GetValueOnRenderThread();
	RasterResults.MaxPatchesPerGroup	= GetMaxPatchesPerGroup();
	RasterResults.MeshPass				= GetMeshPass(Configuration);

	RasterResults.ViewsBuffer			= ViewsBuffer;
	RasterResults.VisibleClustersSWHW	= VisibleClustersSWHW;
	RasterResults.VisBuffer64			= RasterContext.VisBuffer64;
	RasterResults.RasterBinMeta 		= RasterBinMetaBuffer;
	
	if (RasterContext.VisualizeActive)
	{
		RasterResults.DbgBuffer64	= RasterContext.DbgBuffer64;
		RasterResults.DbgBuffer32	= RasterContext.DbgBuffer32;
	}
}

class FExtractVSMPerformanceFeedbackCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FExtractVSMPerformanceFeedbackCS);
	SHADER_USE_PARAMETER_STRUCT(FExtractVSMPerformanceFeedbackCS, FNaniteGlobalShader);

	class FTwoPassCullingDim : SHADER_PERMUTATION_BOOL("TWO_PASS_CULLING");
	class FMaterialCacheDim : SHADER_PERMUTATION_BOOL("MATERIAL_CACHE");
	using FPermutationDomain = TShaderPermutationDomain<FTwoPassCullingDim, FMaterialCacheDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("VIRTUAL_TEXTURE_TARGET"), 1); // Always true, because this only runs for VSMs
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER(uint32, MaxVisibleClusters)
		SHADER_PARAMETER(uint32, RenderFlags)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPackedView>, InViews)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer,	ClusterPageData)

		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, InClusterStats)
		RDG_BUFFER_ACCESS(ClusterIndirectArgs, ERHIAccess::IndirectArgs)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutPerformanceFeedbackBuffer)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FExtractVSMPerformanceFeedbackCS, "/Engine/Private/Nanite/NanitePrintStats.usf", "ExtractVSMPerformanceFeedback", SF_Compute);

void FRenderer::ExtractVSMPerformanceFeedback()
{
	if (!ClusterIndirectArgsBuffer)
	{
		CalculateClusterIndirectArgsBuffer();
		check(ClusterIndirectArgsBuffer);
		check(ClusterStatsBuffer);
	}

	FExtractVSMPerformanceFeedbackCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FExtractVSMPerformanceFeedbackCS::FParameters>();

	PassParameters->InViews					= GraphBuilder.CreateSRV(ViewsBuffer);
	PassParameters->PageConstants			= PageConstants;
	PassParameters->MaxVisibleClusters		= Nanite::FGlobalResources::GetMaxVisibleClusters();
	PassParameters->RenderFlags				= RenderFlags;

	PassParameters->ClusterPageData			= GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	PassParameters->VisibleClustersSWHW		= GraphBuilder.CreateSRV(VisibleClustersSWHW);

	PassParameters->ClusterIndirectArgs = ClusterIndirectArgsBuffer;
	PassParameters->InClusterStats = GraphBuilder.CreateSRV(ClusterStatsBuffer);

	check(VirtualShadowMapArray);
	check(VirtualShadowMapArray->NanitePerformanceFeedbackRDG);
	PassParameters->OutPerformanceFeedbackBuffer = GraphBuilder.CreateUAV(VirtualShadowMapArray->NanitePerformanceFeedbackRDG);

	FExtractVSMPerformanceFeedbackCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FExtractVSMPerformanceFeedbackCS::FTwoPassCullingDim>(Configuration.bTwoPassOcclusion);
	auto ComputeShader = SharedContext.ShaderMap->GetShader<FExtractVSMPerformanceFeedbackCS>(PermutationVector);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ExtractVSMPerformanceFeedback"),
		ComputeShader,
		PassParameters,
		ClusterIndirectArgsBuffer,
		0
	);
}

// Build dispatch indirect buffer for per-cluster stats
class FCalculateClusterIndirectArgsCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateClusterIndirectArgsCS);
	SHADER_USE_PARAMETER_STRUCT(FCalculateClusterIndirectArgsCS, FNaniteGlobalShader);

	class FTwoPassCullingDim : SHADER_PERMUTATION_BOOL( "TWO_PASS_CULLING" );
	using FPermutationDomain = TShaderPermutationDomain<FTwoPassCullingDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER(uint32, RenderFlags)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutClusterStatsArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutClusterStats)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, MainPassRasterizeArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, PostPassRasterizeArgsSWHW)
		END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCalculateClusterIndirectArgsCS, "/Engine/Private/Nanite/NanitePrintStats.usf", "CalculateClusterIndirectArgs", SF_Compute);

void FRenderer::CalculateClusterIndirectArgsBuffer()
{
	FRDGBufferRef OutputIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(4), TEXT("Nanite.ClusterIndirectArgs"));
	FRDGBufferRef OutputClusterStatsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 2), TEXT("Nanite.ClusterStats"));

	FCalculateClusterIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCalculateClusterIndirectArgsCS::FParameters>();

	PassParameters->RenderFlags = RenderFlags;

	PassParameters->OutClusterStatsArgs			= GraphBuilder.CreateUAV(OutputIndirectArgsBuffer);
	PassParameters->OutClusterStats				= GraphBuilder.CreateUAV(OutputClusterStatsBuffer);

	PassParameters->MainPassRasterizeArgsSWHW	= GraphBuilder.CreateSRV(MainRasterizeArgsSWHW);

	if (Configuration.bTwoPassOcclusion)
	{
		check(PostRasterizeArgsSWHW);
		PassParameters->PostPassRasterizeArgsSWHW = GraphBuilder.CreateSRV(PostRasterizeArgsSWHW);
	}

	FCalculateClusterIndirectArgsCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FCalculateClusterIndirectArgsCS::FTwoPassCullingDim>(Configuration.bTwoPassOcclusion);
	auto ComputeShader = SharedContext.ShaderMap->GetShader<FCalculateClusterIndirectArgsCS>( PermutationVector );

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("CalculateClusterIndirectArgs"),
		ComputeShader,
		PassParameters,
		FIntVector(1, 1, 1)
	);

	ClusterIndirectArgsBuffer = OutputIndirectArgsBuffer;
	ClusterStatsBuffer = OutputClusterStatsBuffer;
}

// Gather raster stats
class FCalculateRasterStatsCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateRasterStatsCS);
	SHADER_USE_PARAMETER_STRUCT(FCalculateRasterStatsCS, FNaniteGlobalShader);

	class FTwoPassCullingDim : SHADER_PERMUTATION_BOOL( "TWO_PASS_CULLING" );
	using FPermutationDomain = TShaderPermutationDomain<FTwoPassCullingDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CALCULATE_STATS"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER(uint32, RenderFlags)
		SHADER_PARAMETER(uint32, NumMainPassRasterBins)
		SHADER_PARAMETER(uint32, NumPostPassRasterBins)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteStats>, OutStatsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutClusterStatsArgs)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FQueueState >, QueueState)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, MainPassRasterizeArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, PostPassRasterizeArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FNaniteRasterBinMeta>, MainPassRasterBinMeta)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FNaniteRasterBinMeta>, PostPassRasterBinMeta)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCalculateRasterStatsCS, "/Engine/Private/Nanite/NanitePrintStats.usf", "CalculateRasterStats", SF_Compute);

// Calculates and accumulates per-cluster stats
class FCalculateClusterStatsCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateClusterStatsCS);
	SHADER_USE_PARAMETER_STRUCT(FCalculateClusterStatsCS, FNaniteGlobalShader);

	class FTwoPassCullingDim : SHADER_PERMUTATION_BOOL("TWO_PASS_CULLING");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FMaterialCacheDim : SHADER_PERMUTATION_BOOL("MATERIAL_CACHE");
	using FPermutationDomain = TShaderPermutationDomain<FTwoPassCullingDim, FVirtualTextureTargetDim, FMaterialCacheDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CALCULATE_CLUSTER_STATS"), 1); 
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER( FIntVector4, PageConstants )
		SHADER_PARAMETER( uint32, MaxVisibleClusters )
		SHADER_PARAMETER( uint32, RenderFlags )

		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,	ClusterPageData )

		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, VisibleClustersSWHW )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer<FNaniteStats>, OutStatsBuffer)

		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >, MainPassRasterizeArgsSWHW )
		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >, PostPassRasterizeArgsSWHW )
		RDG_BUFFER_ACCESS(StatsArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCalculateClusterStatsCS, "/Engine/Private/Nanite/NanitePrintStats.usf", "CalculateClusterStats", SF_Compute);

void FRenderer::ExtractStats( const FBinningData& MainPassBinning, const FBinningData& PostPassBinning )
{
	LLM_SCOPE_BYTAG(Nanite);

	if ((RenderFlags & NANITE_RENDER_FLAG_WRITE_STATS) != 0u && StatsBuffer != nullptr)
	{
		{
			FCalculateRasterStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCalculateRasterStatsCS::FParameters>();

			PassParameters->RenderFlags = RenderFlags;

			PassParameters->OutStatsBuffer				= GraphBuilder.CreateUAV(StatsBuffer);

			PassParameters->QueueState					= GraphBuilder.CreateSRV(QueueState);

			PassParameters->NumMainPassRasterBins = MainPassBinning.BinCount;
			PassParameters->MainPassRasterBinMeta = GraphBuilder.CreateSRV(MainPassBinning.MetaBuffer);

			if (Configuration.bTwoPassOcclusion)
			{
				check(PostPassBinning.MetaBuffer);

				PassParameters->NumPostPassRasterBins = PostPassBinning.BinCount;
				PassParameters->PostPassRasterBinMeta = GraphBuilder.CreateSRV(PostPassBinning.MetaBuffer);
			}
			else
			{
				PassParameters->NumPostPassRasterBins = 0;
			}

			FCalculateRasterStatsCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FCalculateRasterStatsCS::FTwoPassCullingDim>(Configuration.bTwoPassOcclusion);
			auto ComputeShader = SharedContext.ShaderMap->GetShader<FCalculateRasterStatsCS>( PermutationVector );

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("CalculateRasterStatsArgs"),
				ComputeShader,
				PassParameters,
				FIntVector(1, 1, 1)
			);
		}

		if (!ClusterIndirectArgsBuffer)
		{
			CalculateClusterIndirectArgsBuffer();
			check(ClusterIndirectArgsBuffer);
		}

		{
			FCalculateClusterStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCalculateClusterStatsCS::FParameters>();

			PassParameters->PageConstants			= PageConstants;
			PassParameters->MaxVisibleClusters		= Nanite::FGlobalResources::GetMaxVisibleClusters();
			PassParameters->RenderFlags				= RenderFlags;

			PassParameters->ClusterPageData			= GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
			PassParameters->VisibleClustersSWHW		= GraphBuilder.CreateSRV(VisibleClustersSWHW);
			PassParameters->OutStatsBuffer			= GraphBuilder.CreateUAV(StatsBuffer);

			PassParameters->MainPassRasterizeArgsSWHW = GraphBuilder.CreateSRV(MainRasterizeArgsSWHW);
			if (Configuration.bTwoPassOcclusion)
			{
				check(PostRasterizeArgsSWHW != nullptr);
				PassParameters->PostPassRasterizeArgsSWHW = GraphBuilder.CreateSRV( PostRasterizeArgsSWHW );
			}
			PassParameters->StatsArgs = ClusterIndirectArgsBuffer;

			FCalculateClusterStatsCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FCalculateClusterStatsCS::FTwoPassCullingDim>(Configuration.bTwoPassOcclusion);
			PermutationVector.Set<FCalculateClusterStatsCS::FVirtualTextureTargetDim>( VirtualShadowMapArray != nullptr );
			auto ComputeShader = SharedContext.ShaderMap->GetShader<FCalculateClusterStatsCS>( PermutationVector );

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("CalculateStats"),
				ComputeShader,
				PassParameters,
				ClusterIndirectArgsBuffer,
				0
			);
		}

		// Extract main pass buffers
		{
			auto& MainPassBuffers = Nanite::GGlobalResources.GetMainPassBuffers();
			MainPassBuffers.StatsRasterizeArgsSWHWBuffer = GraphBuilder.ConvertToExternalBuffer(MainRasterizeArgsSWHW);
		}

		// Extract post pass buffers
		auto& PostPassBuffers = Nanite::GGlobalResources.GetPostPassBuffers();
		PostPassBuffers.StatsRasterizeArgsSWHWBuffer = nullptr;
		if (Configuration.bTwoPassOcclusion)
		{
			check( PostRasterizeArgsSWHW != nullptr );
			PostPassBuffers.StatsRasterizeArgsSWHWBuffer = GraphBuilder.ConvertToExternalBuffer(PostRasterizeArgsSWHW);
		}

		// Extract calculated stats (so VisibleClustersSWHW isn't needed later)
		{
			Nanite::GGlobalResources.GetStatsBufferRef() = GraphBuilder.ConvertToExternalBuffer(StatsBuffer);
		}

		// Save out current render and debug flags.
		Nanite::GGlobalResources.StatsRenderFlags = RenderFlags;
		Nanite::GGlobalResources.StatsDebugFlags = DebugFlags;
	}
}

class FNaniteFeedbackStatusCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNaniteFeedbackStatusCS);
	SHADER_USE_PARAMETER_STRUCT(FNaniteFeedbackStatusCS, FNaniteGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FQueueState>, OutQueueState)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InMainRasterizerArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InPostRasterizerArgsSWHW)

		SHADER_PARAMETER_STRUCT_INCLUDE(GPUMessage::FParameters, GPUMessageParams)
		SHADER_PARAMETER(uint32, StatusMessageId)
		SHADER_PARAMETER(uint32, RenderFlags)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FNaniteFeedbackStatusCS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "FeedbackStatus", SF_Compute);

void FRenderer::FeedbackStatus()
{
#if !UE_BUILD_SHIPPING
	FNaniteFeedbackStatusCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FNaniteFeedbackStatusCS::FParameters>();
	PassParameters->OutQueueState = GraphBuilder.CreateUAV(QueueState);
	PassParameters->InMainRasterizerArgsSWHW = GraphBuilder.CreateSRV(MainRasterizeArgsSWHW);
	PassParameters->InPostRasterizerArgsSWHW = GraphBuilder.CreateSRV(Configuration.bTwoPassOcclusion ? PostRasterizeArgsSWHW : MainRasterizeArgsSWHW);	// Avoid permutation by doing Post=Main for single pass
	PassParameters->GPUMessageParams = GPUMessage::GetShaderParameters(GraphBuilder);
	PassParameters->StatusMessageId = GGlobalResources.GetFeedbackManager()->GetStatusMessageId();
	PassParameters->RenderFlags = RenderFlags;

	auto ComputeShader = SharedContext.ShaderMap->GetShader<FNaniteFeedbackStatusCS>();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("NaniteFeedbackStatus"),
		ComputeShader,
		PassParameters,
		FIntVector(1, 1, 1)
	);
#endif
}

void FConfiguration::SetViewFlags(const FViewInfo& View)
{
	bIsGameView							= View.bIsGameView;
	bIsSceneCapture						= View.bIsSceneCapture;
	bIsReflectionCapture				= View.bIsReflectionCapture;
	bGameShowFlag						= !!View.Family->EngineShowFlags.Game;
	bEditorShowFlag						= !!View.Family->EngineShowFlags.Editor;
	bDrawOnlyRootGeometry				= !View.Family->EngineShowFlags.NaniteStreamingGeometry;
}

void FInstanceHierarchyDriver::Init(FRDGBuilder& GraphBuilder, bool bInIsEnabled, bool bTwoPassOcclusion, const FGlobalShaderMap* ShaderMap, FSceneInstanceCullingQuery* SceneInstanceCullingQuery, FRDGBufferRef InViewDrawRanges)
{
	bIsEnabled = bInIsEnabled && SceneInstanceCullingQuery != nullptr;
	bAllowStaticGeometryPath = CVarNaniteAllowStaticGeometryPath.GetValueOnRenderThread() ? 1 : 0;
	GroupWorkArgsMaxCount = uint32(FMath::Max(32, CVarNaniteInstanceHierarchyArgsMaxWorkGroups.GetValueOnRenderThread()));

	if (bIsEnabled)
	{
		check(InViewDrawRanges);
		ViewDrawRangesRDG = InViewDrawRanges;

		DeferredSetupContext = GraphBuilder.AllocObject<FDeferredSetupContext>();
		DeferredSetupContext->SceneInstanceCullingQuery = SceneInstanceCullingQuery;
		DeferredSetupContext->SceneInstanceCullResult = SceneInstanceCullingQuery->GetResultAsync();

		ChunkDrawViewGroupIdsRDG = CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.CellChunkDraws"), [DeferredSetupContext = DeferredSetupContext]() -> const typename FSceneInstanceCullResult::FChunkCullViewGroupIds& { DeferredSetupContext->Sync(); return DeferredSetupContext->SceneInstanceCullResult->ChunkCullViewGroupIds; });

		InstanceWorkArgs[0] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(4 * 2), TEXT("Nanite.InstanceHierarhcy.InstanceWorkArgs[0]"));
		if (bTwoPassOcclusion)
		{
			// Note: 4 element indirect args buffer to enable using the 4th to store the count of singular items (to handle fractional work groups)
			OccludedChunkArgsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(4), TEXT("Nanite.InstanceHierarhcy.OccludedChunkArgs"));
			InstanceWorkArgs[1] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(4 * 2), TEXT("Nanite.InstanceHierarhcy.InstanceWorkArgs[1]"));
			OccludedChunkDrawsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FOccludedChunkDraw), 1u/*temp*/), TEXT("Nanite.InstanceHierarhcy.OccludedChunkDraws"), [DeferredSetupContext = DeferredSetupContext]() { DeferredSetupContext->Sync(); return DeferredSetupContext->MaxOccludedChunkDrawsPOT;});
		}

		// Instance work, this is what has passed cell culling and needs to enter instance culling.
		InstanceWorkGroupsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FInstanceCullingGroupWork), 1), TEXT("Nanite.InstanceHierarhcy.InstanceWorkGroups"), [DeferredSetupContext = DeferredSetupContext]() 	{ DeferredSetupContext->Sync(); return DeferredSetupContext->GetMaxInstanceWorkGroups(); });

		// Note: This is the sync point for the setup since this is where we demand the shader parameters and thus must have produced the uploaded stuff.
		ShaderParameters = SceneInstanceCullingQuery->GetSceneCullingRenderer().GetShaderParameters(GraphBuilder);

		// These are not known at this time.
		{
			FInitInstanceHierarchyArgs_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitInstanceHierarchyArgs_CS::FParameters >();

			PassParameters->OutInstanceWorkArgs0 = GraphBuilder.CreateUAV( InstanceWorkArgs[0] );

			if (bTwoPassOcclusion)
			{
				PassParameters->OutInstanceWorkArgs1 = GraphBuilder.CreateUAV( InstanceWorkArgs[1] );
				PassParameters->OutOccludedChunkArgs = GraphBuilder.CreateUAV( OccludedChunkArgsRDG );
			}

			FInitInstanceHierarchyArgs_CS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FInitInstanceHierarchyArgs_CS::FOcclusionCullingDim>(bTwoPassOcclusion);
		
			auto ComputeShader = ShaderMap->GetShader< FInitInstanceHierarchyArgs_CS >( PermutationVector );

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME( "InitArgs" ),
				ComputeShader,
				PassParameters,
				FIntVector( 1, 1, 1 )
			);
		}
	}	
};

FInstanceWorkGroupParameters FInstanceHierarchyDriver::DispatchCullingPass(FRDGBuilder& GraphBuilder, uint32 CullingPass, const FRenderer& Renderer)
{
	// Double buffer because the post pass buffer is used as output to in the main pass instance cull (and then in the post pass hierachy cull) so both must exist at the same time
	FRDGBuffer* PassInstanceWorkArgs = InstanceWorkArgs[CullingPass == CULLING_PASS_OCCLUSION_POST];

	FRDGBufferUAVRef OutInstanceWorkGroupsUAV = GraphBuilder.CreateUAV(InstanceWorkGroupsRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
	FRDGBufferUAVRef OutInstanceWorkArgsUAV = GraphBuilder.CreateUAV(PassInstanceWorkArgs, PF_R32_UINT, ERDGUnorderedAccessViewFlags::SkipBarrier );

	// deferred: SHADER_PARAMETER(uint32, MaxInstanceWorkGroups)
	{
		FInstanceHierarchyCullShader::FCommonParameters CommonParameters;
		CommonParameters.Scene = Renderer.SceneUniformBuffer;
		CommonParameters.CullingParameters = Renderer.CullingParameters;
		CommonParameters.VirtualShadowMap = Renderer.VirtualTargetParameters;
		CommonParameters.InstanceHierarchyParameters = ShaderParameters;
		CommonParameters.InViewDrawRanges = GraphBuilder.CreateSRV(ViewDrawRangesRDG);
		CommonParameters.OutInstanceWorkGroups = OutInstanceWorkGroupsUAV;
		CommonParameters.OutInstanceWorkArgs = OutInstanceWorkArgsUAV;
		CommonParameters.OutOccludedChunkArgs = nullptr;
		CommonParameters.bAllowStaticGeometryPath = bAllowStaticGeometryPath ? 1 : 0;

		if( CullingPass == CULLING_PASS_OCCLUSION_POST )
		{
			CommonParameters.InOccludedChunkArgs = GraphBuilder.CreateSRV(OccludedChunkArgsRDG);
			CommonParameters.IndirectArgs = OccludedChunkArgsRDG;
		}
		else
		{
			CommonParameters.InOccludedChunkArgs = nullptr;
			CommonParameters.OutOccludedChunkDraws = nullptr;
			if (Renderer.Configuration.bTwoPassOcclusion)
			{
				CommonParameters.OutOccludedChunkArgs = GraphBuilder.CreateUAV(OccludedChunkArgsRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
				CommonParameters.OutOccludedChunkDraws = GraphBuilder.CreateUAV(OccludedChunkDrawsRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
			}
			CommonParameters.IndirectArgs = nullptr;
		}
		
		if (Renderer.StatsBuffer)
		{
			CommonParameters.OutStatsBuffer = Renderer.StatsBufferSkipBarrierUAV;
		}


		FInstanceHierarchyCullShader::FPermutationDomain PermutationVector;
		PermutationVector.Set<FInstanceHierarchyCullShader::FCullingPassDim>(CullingPass);
		PermutationVector.Set<FInstanceHierarchyCullShader::FDebugFlagsDim>(Renderer.IsDebuggingEnabled());
		PermutationVector.Set<FInstanceHierarchyCullShader::FVirtualTextureTargetDim>(Renderer.IsUsingVirtualShadowMap());

		{
			if( CullingPass == CULLING_PASS_OCCLUSION_POST )
			{
				{
					FInstanceHierarchyChunkCull_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInstanceHierarchyChunkCull_CS::FParameters >();
					PassParameters->CommonParameters = CommonParameters;
					PassParameters->InGroupIds = nullptr;
					PassParameters->NumGroupIds = 0;
					PassParameters->InOccludedChunkDraws = GraphBuilder.CreateSRV(OccludedChunkDrawsRDG);

					auto ComputeShader = Renderer.SharedContext.ShaderMap->GetShader<FInstanceHierarchyChunkCull_CS>(PermutationVector);
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME( "HierarchyChunkCull" ),
						ComputeShader,
						PassParameters,
						OccludedChunkArgsRDG,
						0u,
						[DeferredSetupContext = DeferredSetupContext, PassParameters]() 
						{
							DeferredSetupContext->Sync();
							PassParameters->CommonParameters.MaxInstanceWorkGroups = DeferredSetupContext->GetMaxInstanceWorkGroups();
						}
					);
				}
			}
			else
			{
				{
					FInstanceHierarchyCellChunkCull_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInstanceHierarchyCellChunkCull_CS::FParameters >();
					PassParameters->CommonParameters = CommonParameters;
					DeferredSetupContext->SceneInstanceCullResult->CellChunkDraws.GetParametersAsync(GraphBuilder, PassParameters->CellChunkDraws);

					auto ComputeShader = Renderer.SharedContext.ShaderMap->GetShader<FInstanceHierarchyCellChunkCull_CS>(PermutationVector);
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME( "HierarchyCellChunkCull" ),
						ComputeShader,
						PassParameters,
						[DeferredSetupContext = DeferredSetupContext, PassParameters]() 
						{
							DeferredSetupContext->Sync();
							PassParameters->CommonParameters.MaxInstanceWorkGroups = DeferredSetupContext->GetMaxInstanceWorkGroups();
							DeferredSetupContext->SceneInstanceCullResult->CellChunkDraws.FinalizeParametersAsync(PassParameters->CellChunkDraws);
							return DeferredSetupContext->SceneInstanceCullResult->CellChunkDraws.GetWrappedCsGroupCount();
						}
					);
				}
				{
					FInstanceHierarchyChunkCull_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInstanceHierarchyChunkCull_CS::FParameters >();
					PassParameters->CommonParameters = CommonParameters;

					PassParameters->InGroupIds = GraphBuilder.CreateSRV(ChunkDrawViewGroupIdsRDG);
					PassParameters->NumGroupIds = 0; // fixed up in deferred callback below
					PassParameters->InOccludedChunkDraws = nullptr;

					auto ComputeShader = Renderer.SharedContext.ShaderMap->GetShader<FInstanceHierarchyChunkCull_CS>(PermutationVector);
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME( "HierarchyChunkCull" ),
						ComputeShader,
						PassParameters,
						[DeferredSetupContext = DeferredSetupContext, PassParameters]() 
						{
							DeferredSetupContext->Sync();
							check(DeferredSetupContext->NumChunkViewGroups < ~0u);
							check(DeferredSetupContext->NumAllocatedChunks < ~0u);
							PassParameters->CommonParameters.MaxInstanceWorkGroups = DeferredSetupContext->GetMaxInstanceWorkGroups();
							PassParameters->NumGroupIds = DeferredSetupContext->NumChunkViewGroups;
							PassParameters->NumAllocatedChunks = DeferredSetupContext->NumAllocatedChunks;
					
							//return FComputeShaderUtils::GetGroupCountWrapped(DeferredSetupContext->NumChunkViewGroups, 64);
							// TODO: we'll run into the dispatch dimension issue here possibly.
							return FIntVector(FMath::DivideAndRoundUp(int32(DeferredSetupContext->NumAllocatedChunks), 64), DeferredSetupContext->NumChunkViewGroups, 1);
						}
					);
				}
			}
		}
	}
	// Run pass to append the uncullable
	if (CullingPass != CULLING_PASS_OCCLUSION_POST)
	{
		FInstanceHierarchyAppendUncullable_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInstanceHierarchyAppendUncullable_CS::FParameters >();

		PassParameters->InstanceHierarchyParameters = ShaderParameters;
		PassParameters->InViewDrawRanges = GraphBuilder.CreateSRV(ViewDrawRangesRDG);
		PassParameters->OutInstanceWorkGroups = OutInstanceWorkGroupsUAV;
		PassParameters->OutInstanceWorkArgs = OutInstanceWorkArgsUAV;
		PassParameters->bAllowStaticGeometryPath = bAllowStaticGeometryPath ? 1 : 0;
		
		if (Renderer.StatsBuffer)
		{
			PassParameters->OutStatsBuffer = Renderer.StatsBufferSkipBarrierUAV;
		}

		FInstanceHierarchyAppendUncullable_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FInstanceHierarchyAppendUncullable_CS::FDebugFlagsDim>(Renderer.IsDebuggingEnabled());

		auto ComputeShader = Renderer.SharedContext.ShaderMap->GetShader<FInstanceHierarchyAppendUncullable_CS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME( "InstanceHierarchyAppendUncullable" ),
			ComputeShader,
			PassParameters,
			[DeferredSetupContext = DeferredSetupContext, PassParameters]() 
			{
				DeferredSetupContext->Sync();
				PassParameters->MaxInstanceWorkGroups = DeferredSetupContext->GetMaxInstanceWorkGroups();
				PassParameters->NumViewDrawGroups = DeferredSetupContext->SceneInstanceCullingQuery->GetViewDrawGroups().Num();
				PassParameters->UncullableItemChunksOffset = DeferredSetupContext->SceneInstanceCullResult->UncullableItemChunksOffset;
				PassParameters->UncullableNumItemChunks = DeferredSetupContext->SceneInstanceCullResult->UncullableNumItemChunks;
			
				FIntVector GroupCount;
				// One thread per chunk, in the X dimension.
				GroupCount.X = FMath::DivideAndRoundUp(PassParameters->UncullableNumItemChunks, 64u);
				// One row of threads in the Y dimension.
				GroupCount.Y = DeferredSetupContext->SceneInstanceCullingQuery->GetViewDrawGroups().Num();
				GroupCount.Z = 1;

				return GroupCount;
			}
		);
	}

	{
		FInstanceHierarchySanitizeInstanceArgs_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInstanceHierarchySanitizeInstanceArgs_CS::FParameters >();
		// Note: important to create new UAV _with_ barrier
		PassParameters->InOutInstanceWorkArgs = GraphBuilder.CreateUAV(PassInstanceWorkArgs, PF_R32_UINT);
		// Clear the arg to something that will be conservative, it is set before dispatch in the argument count callback below.
		PassParameters->GroupWorkArgsMaxCount = 0u;
		
		if (Renderer.StatsBuffer)
		{
			PassParameters->OutStatsBuffer = Renderer.StatsBufferSkipBarrierUAV;
		}

		FInstanceHierarchySanitizeInstanceArgs_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FInstanceHierarchySanitizeInstanceArgs_CS::FDebugFlagsDim>(Renderer.IsDebuggingEnabled());

		auto ComputeShader = Renderer.SharedContext.ShaderMap->GetShader<FInstanceHierarchySanitizeInstanceArgs_CS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME( "InstanceHierarchySanitizeInstanceArgs" ),
			ComputeShader,
			PassParameters,
			[DeferredSetupContext = DeferredSetupContext, GroupWorkArgsMaxCount = GroupWorkArgsMaxCount, PassParameters]() 
			{
				DeferredSetupContext->Sync();
				PassParameters->GroupWorkArgsMaxCount = FMath::Min(GroupWorkArgsMaxCount, DeferredSetupContext->GetMaxInstanceWorkGroups());
				PassParameters->MaxInstanceWorkGroups = DeferredSetupContext->GetMaxInstanceWorkGroups();
				return FIntVector(1,1,1);
			}
		);
	}

	// Set up parameters for the following instance cull pass
	FInstanceWorkGroupParameters InstanceWorkGroupParameters;
	InstanceWorkGroupParameters.InInstanceWorkArgs = GraphBuilder.CreateSRV(PassInstanceWorkArgs, PF_R32_UINT);
	InstanceWorkGroupParameters.InInstanceWorkGroups = GraphBuilder.CreateSRV(InstanceWorkGroupsRDG);
	InstanceWorkGroupParameters.InstanceIds = ShaderParameters.InstanceIds;
	InstanceWorkGroupParameters.InViewDrawRanges = GraphBuilder.CreateSRV(ViewDrawRangesRDG);

	return InstanceWorkGroupParameters;
}

} // namespace Nanite