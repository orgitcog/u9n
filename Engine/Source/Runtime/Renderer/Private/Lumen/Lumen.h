// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineDefines.h"
#include "HAL/LowLevelMemTracker.h"
#include "Math/Vector.h"
#include "RHIShaderPlatform.h"

class FScene;
class FSceneView;
class FSceneViewFamily;
class FViewFamilyInfo;
class FViewInfo;
struct FEngineShowFlags;
enum class EDiffuseIndirectMethod;
struct FScreenMessageWriter;
enum EPixelFormat : uint8;

extern bool ShouldRenderLumenDiffuseGI(const FScene* Scene, const FSceneView& View, bool bSkipTracingDataCheck = false, bool bSkipProjectCheck = false);
extern bool ShouldRenderLumenReflections(const FSceneView& View, bool bSkipTracingDataCheck = false, bool bSkipProjectCheck = false, bool bIncludeStandalone = true);
extern bool ShouldRenderLumenReflectionsWater(const FViewInfo& View, bool bSkipTracingDataCheck = false, bool bSkipProjectCheck = false);
extern bool ShouldRenderLumenDirectLighting(const FScene* Scene, const FSceneView& View);
extern bool ShouldRenderAOWithLumenGI();
extern bool ShouldUseStereoLumenOptimizations();


class FLumenSceneData;

inline double BoxSurfaceArea(FVector Extent)
{
	return 2.0 * (Extent.X * Extent.Y + Extent.Y * Extent.Z + Extent.Z * Extent.X);
}

namespace Lumen
{
	// Must match usf
	constexpr uint32 PhysicalPageSize = 128;
	constexpr uint32 VirtualPageSize = PhysicalPageSize - 1; // 0.5 texel border around page
	constexpr uint32 MinCardResolution = 8;
	constexpr uint32 MinResLevel = 3; // 2^3 = MinCardResolution
	constexpr uint32 MaxResLevel = 11; // 2^11 = 2048 texels
	constexpr uint32 SubAllocationResLevel = 7; // log2(PHYSICAL_PAGE_SIZE)
	constexpr uint32 NumResLevels = MaxResLevel - MinResLevel + 1;
	constexpr uint32 CardTileSize = 8;
	constexpr uint32 CardTileShadowDownsampleFactorDwords = 8;
	constexpr uint32 NumDistanceBuckets = 16;

	constexpr float MaxTraceDistance = 0.5f * UE_OLD_WORLD_MAX;

	enum class ETracingPermutation
	{
		Cards,
		VoxelsAfterCards,
		Voxels,
		MAX
	};

	void DebugResetSurfaceCache();

	float GetMaxTraceDistance(const FViewInfo& View);
	bool IsLumenFeatureAllowedForView(const FScene* Scene, const FSceneView& View, bool bSkipTracingDataCheck = false, bool bSkipProjectCheck = false);
	bool ShouldVisualizeScene(const FEngineShowFlags& ShowFlags);
	bool ShouldVisualizeHardwareRayTracing(const FSceneViewFamily& ViewFamily);
	bool ShouldHandleSkyLight(const FScene* Scene, const FSceneViewFamily& ViewFamily);

	bool ShouldUpdateLumenSceneViewOrigin();
	FVector GetLumenSceneViewOrigin(const FViewInfo& View, int32 ClipmapIndex);

	// Global Distance Field
	int32 GetGlobalDFResolution();
	float GetGlobalDFClipmapExtent(int32 ClipmapIndex);
	int32 GetNumGlobalDFClipmaps(const FSceneView& View);

	// Features
	bool UseAsyncCompute(const FViewFamilyInfo& ViewFamily);
	bool UseWaveOps(EShaderPlatform ShaderPlatform);
	bool UseThreadGroupSize32();
	EPixelFormat GetLightingDataFormat();
	FVector3f GetLightingQuantizationError();
	float GetCachedLightingPreExposure();

	// Surface cache
	bool IsSurfaceCacheFrozen();
	bool IsSurfaceCacheUpdateFrameFrozen();

	// Software ray tracing
	bool IsSoftwareRayTracingSupported();
	bool UseMeshSDFTracing(const FEngineShowFlags& EngineShowFlags);
	bool UseGlobalSDFTracing(const FEngineShowFlags& EngineShowFlags);
	bool UseGlobalSDFSimpleCoverageBasedExpand();
	bool UseGlobalSDFObjectGrid(const FSceneViewFamily& ViewFamily);
	bool UseHeightfieldTracing(const FSceneViewFamily& ViewFamily, const FLumenSceneData& LumenSceneData);
	bool UseHeightfieldTracingForVoxelLighting(const FLumenSceneData& LumenSceneData);
	int32 GetHeightfieldMaxTracingSteps();
	bool IsUsingGlobalSDF(const FSceneViewFamily& ViewFamily);
	bool IsUsingDistanceFieldRepresentationBit(const FViewInfo& View);

	// Hardware ray tracing
	bool AnyLumenHardwareRayTracingPassEnabled(const FScene* Scene, const FViewInfo& View);
	bool AnyLumenHardwareInlineRayTracingPassEnabled(const FScene* Scene, const FViewInfo& View);
	bool UseHardwareRayTracing(const FSceneViewFamily& ViewFamily);
	bool UseHardwareRayTracedSceneLighting(const FSceneViewFamily& ViewFamily);
	bool UseHardwareRayTracedDirectLighting(const FSceneViewFamily& ViewFamily);
	bool UseHardwareRayTracedReflections(const FSceneViewFamily& ViewFamily);
	bool UseReSTIRGather(const FSceneViewFamily& ViewFamily, EShaderPlatform ShaderPlatform);
	bool UseHardwareRayTracedScreenProbeGather(const FSceneViewFamily& ViewFamily);
	bool UseHardwareRayTracedShortRangeAO(const FSceneViewFamily& ViewFamily);
	bool UseHardwareRayTracedRadianceCache(const FSceneViewFamily& ViewFamily);
	bool UseHardwareRayTracedRadiosity(const FSceneViewFamily& ViewFamily);
	bool UseHardwareRayTracedVisualize(const FSceneViewFamily& ViewFamily);
	bool IsUsingRayTracingLightingGrid(const FSceneViewFamily& ViewFamily, const FViewInfo& View, EDiffuseIndirectMethod DiffuseIndirectMethod);

	bool ShouldRenderRadiosityHardwareRayTracing(const FSceneViewFamily& ViewFamily);
	bool ShouldVisualizeHardwareRayTracing(const FSceneViewFamily& ViewFamily);

	bool ShouldPrecachePSOs(EShaderPlatform Platform);
	
	bool UseHardwareInlineRayTracing(const FSceneViewFamily& ViewFamily);

	enum class ESurfaceCacheSampling
	{
		AlwaysResidentPagesWithoutFeedback,
		AlwaysResidentPages,
		HighResPages,
	};

	float GetHardwareRayTracingPullbackBias();

	bool UseFarField(const FSceneViewFamily& ViewFamily);
	bool UseFarFieldOcclusionOnly();
	float GetFarFieldMaxTraceDistance();
	float GetNearFieldMaxTraceDistanceDitherScale(bool bUseFarField);
	float GetNearFieldSceneRadius(const FViewInfo& View, bool bUseFarField);

	uint32 GetMeshCardDistanceBin(float Distance);

	float GetHeightfieldReceiverBias();
	void Shutdown();

	bool WriteWarnings(const FScene* Scene, const FEngineShowFlags& ShowFlags, const TArray<FViewInfo>& Views, FScreenMessageWriter* Writer);

	bool SupportsMultipleClosureEvaluation(EShaderPlatform ShaderPlatform);
	bool SupportsMultipleClosureEvaluation(const FViewInfo& View);
};

extern int32 GLumenFastCameraMode;

LLM_DECLARE_TAG(Lumen);
