// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TranslucentLighting.cpp: Translucent lighting implementation.
=============================================================================*/

#include "TranslucentLighting.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "EngineDefines.h"
#include "RHI.h"
#include "RenderResource.h"
#include "HitProxies.h"
#include "FinalPostProcessSettings.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "PrimitiveViewRelevance.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PrimitiveDrawingUtils.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Components/LightComponent.h"
#include "Materials/Material.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightSceneInfo.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShadowRendering.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "TranslucentRendering.h"
#include "ClearQuad.h"
#include "ScenePrivate.h"
#include "OneColorShader.h"
#include "LightFunctionRendering.h"
#include "LightRendering.h"
#include "ScreenRendering.h"
#include "AmbientCubemapParameters.h"
#include "VolumeRendering.h"
#include "VolumeLighting.h"
#include "PipelineStateCache.h"
#include "VisualizeTexture.h"
#include "MeshPassProcessor.inl"
#include "SkyAtmosphereRendering.h"
#include "VolumetricCloudRendering.h"
#include "TranslucentLightingViewState.h"
#include "RenderCore.h"
#include "StaticMeshBatch.h"
#include "LightFunctionAtlas.h"
#include "HeterogeneousVolumes/HeterogeneousVolumes.h"
#include "DeferredShadingRenderer.h"
#include "BasePassRendering.h"
#include "LightFunctionAtlas.h"
#include "BlueNoise.h"

using namespace LightFunctionAtlas;

class FMaterial;

/** Whether to allow rendering translucency shadow depths. */
bool GUseTranslucencyShadowDepths = true;

DECLARE_GPU_STAT_NAMED(TranslucentLighting, TEXT("Translucent Lighting"));
 
int32 GUseTranslucentLightingVolumes = 1;
FAutoConsoleVariableRef CVarUseTranslucentLightingVolumes(
	TEXT("r.TranslucentLightingVolume"),
	GUseTranslucentLightingVolumes,
	TEXT("Whether to allow updating the translucent lighting volumes.\n")
	TEXT("0:off, otherwise on, default is 1"),
	ECVF_RenderThreadSafe | ECVF_Scalability
	);

float GTranslucentVolumeMinFOV = 45;
static FAutoConsoleVariableRef CVarTranslucentVolumeMinFOV(
	TEXT("r.TranslucentVolumeMinFOV"),
	GTranslucentVolumeMinFOV,
	TEXT("Minimum FOV for translucent lighting volume. Prevents popping in lighting when zooming in."),
	ECVF_RenderThreadSafe | ECVF_Scalability
	);

float GTranslucentVolumeFOVSnapFactor = 10;
static FAutoConsoleVariableRef CTranslucentVolumeFOVSnapFactor(
	TEXT("r.TranslucentVolumeFOVSnapFactor"),
	GTranslucentVolumeFOVSnapFactor,
	TEXT("FOV will be snapped to a factor of this before computing volume bounds."),
	ECVF_RenderThreadSafe | ECVF_Scalability
	);

int32 GUseTranslucencyVolumeBlur = 1;
FAutoConsoleVariableRef CVarUseTranslucentLightingVolumeBlur(
	TEXT("r.TranslucencyVolumeBlur"),
	GUseTranslucencyVolumeBlur,
	TEXT("Whether to blur the translucent lighting volumes.\n")
	TEXT("0:off, otherwise on, default is 1"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyLightingVolumeDim = 64;
FAutoConsoleVariableRef CVarTranslucencyLightingVolumeDim(
	TEXT("r.TranslucencyLightingVolumeDim"),
	GTranslucencyLightingVolumeDim,
	TEXT("Dimensions of the volume textures used for translucency lighting.  Larger textures result in higher resolution but lower performance."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarTranslucencyLightingVolumeInnerDistance(
	TEXT("r.TranslucencyLightingVolumeInnerDistance"),
	1500.0f,
	TEXT("Distance from the camera that the first volume cascade should end"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarTranslucencyLightingVolumeOuterDistance(
	TEXT("r.TranslucencyLightingVolumeOuterDistance"),
	5000.0f,
	TEXT("Distance from the camera that the second volume cascade should end"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarTranslucencyLightingVolumePositionOffsetRadius(
	TEXT("r.TranslucencyLightingVolume.PositionOffsetRadius"),
	0.0f,
	TEXT("Radius of per-pixel offset applied to position when sampling translucency lighting volume."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CVarTranslucencyLightingVolumeTemporal(
	TEXT("r.TranslucencyLightingVolume.Temporal"),
	false,
	TEXT("Whether to use temporal accumulation instead of spatial filter when updating the translucency lighting volume."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarTranslucencyLightingVolumeHistoryWeight(
	TEXT("r.TranslucencyLightingVolume.Temporal.HistoryWeight"),
	0.9,
	TEXT("How much the history value should be weighted each frame.  This is a tradeoff between visible jittering and responsiveness."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarTranslucencyLightingVolumeMarkVoxelsSupported(
	TEXT("r.TranslucencyLightingVolume.MarkVoxels.Supported"),
	true,
	TEXT("Whether marking used voxels is supported by the project. Avoids compiling some shaders when disabled.\n")
	TEXT("This setting can't be changed at runtime since it affects cooking."),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<bool> CVarTranslucencyLightingVolumeMarkVoxels(
	TEXT("r.TranslucencyLightingVolume.MarkVoxels"),
	false,
	TEXT("Whether to mark which volume voxels are sampled during rendering and only update those that are."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyLightingVolumeBatch(
	TEXT("r.TranslucencyLightingVolume.Batch"),
	1,
	TEXT("When enabled, batches supported lights into a single draw call for efficiency"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarTranslucencyLightingVolumeAccurateRectLights(
	TEXT("r.TranslucencyLightingVolume.AccurateRectLights"),
	1,
	TEXT("When disabled rect lights are approximated as spot lights in the translucency volume.\n")
	TEXT("Only accurate rect lights are included in batching, so it is recommended to enable this when batching is used."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarTranslucencyLightingVolumeInjectDirectionalLightCSM(
	TEXT("r.TranslucencyLightingVolume.InjectDirectionalLightCSM"),
	1,
	TEXT("Enable sampling of the directional light CSM.\n"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

// Adaptation to camera angle is now disabled by default to avoid change on lighting when zoomin in or out.
// The volume remains around the camera anyway, so the camera angle should not matter when the setup mostly depends on start and end distance for each cascade.
int32 GTranslucencyLightingVolumeAdaptToPerspective = 0;
static FAutoConsoleVariableRef CVarTranslucencyLightingVolumeAdaptToPerspective(
	TEXT("r.TranslucencyLightingVolume.AdaptToPerspective"),
	GTranslucencyLightingVolumeAdaptToPerspective,
	TEXT("The translucent volume will adapt to the camera perspective when zooming. This can can result in pops for extreme zoom-in so it can be disabled if needed."),
	ECVF_RenderThreadSafe
);

int32 GTranslucencyLightingVolumeDebug = 0;
static FAutoConsoleVariableRef CVarTranslucencyLightingVolumeDebug(
	TEXT("r.TranslucencyLightingVolume.Debug"),
	GTranslucencyLightingVolumeDebug,
	TEXT("Debug information for the translucency lighting volume."),
	ECVF_RenderThreadSafe
);

static int32 GTranslucencyLightingVolumeMaterialPSOPrecache = 1;
static FAutoConsoleVariableRef CVarGTranslucencyLightingVolumeMaterialPSOPrecache(
	TEXT("r.PSOPrecache.TranslucencyLightingVolumeMaterial"), 
	GTranslucencyLightingVolumeMaterialPSOPrecache, 
	TEXT("Precache all possible required Translucency Lighting Volume PSOs for loaded LightMaterials."),
	ECVF_ReadOnly);

static const TCHAR* TranslucentLightingMaterialPSOCollectorName = TEXT("TranslucentLightingMaterialPSOCollector");

float GetTranslucencyLightingVolumePositionOffsetRadius()
{
	return FMath::Max(0.0f, CVarTranslucencyLightingVolumePositionOffsetRadius.GetValueOnAnyThread());
}

/** Function returning current translucency lighting volume dimensions. */
int32 GetTranslucencyLightingVolumeDim()
{
	return FMath::Clamp(GTranslucencyLightingVolumeDim, 4, 2048);
}

void FViewInfo::CalcTranslucencyLightingVolumeBounds(FBox* InOutCascadeBoundsArray, int32 NumCascades) const
{
	for (int32 CascadeIndex = 0; CascadeIndex < NumCascades; CascadeIndex++)
	{
		double InnerDistance = CVarTranslucencyLightingVolumeInnerDistance.GetValueOnRenderThread();
		double OuterDistance = CVarTranslucencyLightingVolumeOuterDistance.GetValueOnRenderThread();

		const double FrustumStartDistance = CascadeIndex == 0 ? 0 : InnerDistance;
		const double FrustumEndDistance = CascadeIndex == 0 ? InnerDistance : OuterDistance;

		double FieldOfView = DOUBLE_PI / 4.0;
		double AspectRatio = 1.0;

		const FViewMatrices& LocalShadowViewMatrices = GetShadowViewMatrices();

		if (IsPerspectiveProjection() && GTranslucencyLightingVolumeAdaptToPerspective > 0)
		{
			// Derive FOV and aspect ratio from the perspective projection matrix
			FieldOfView = FMath::Atan(1.0 / LocalShadowViewMatrices.GetProjectionMatrix().M[0][0]);
			// Clamp to prevent shimmering when zooming in
			FieldOfView = FMath::Max(FieldOfView, GTranslucentVolumeMinFOV * DOUBLE_PI / 180.0);
			const double RoundFactorRadians = GTranslucentVolumeFOVSnapFactor * DOUBLE_PI / 180.0;
			// Round up to a fixed factor
			// This causes the volume lighting to make discreet jumps as the FOV animates, instead of slowly crawling over a long period
			FieldOfView = FieldOfView + RoundFactorRadians - FMath::Fmod(FieldOfView, RoundFactorRadians);
			AspectRatio = LocalShadowViewMatrices.GetProjectionMatrix().M[1][1] / LocalShadowViewMatrices.GetProjectionMatrix().M[0][0];
		}

		// Tan of field of view can explode when FieldOfView is close to 180 degree, when using the adaptation to Fov above.
		// Close to this edge case, the volume fitted on the frustum vertices can explode to infinity.
		// In order to fix that, we clamp the value to the length of the diagonal of a cube of size OuterDistance.
		const double MaxTanFieldOfViewLength = FMath::Sqrt(FMath::Max(1.0f, OuterDistance * OuterDistance + OuterDistance * OuterDistance + OuterDistance * OuterDistance));
		const double TanFieldOfView = FMath::Tan(FieldOfView);

		const double StartHorizontalLength = FMath::Min(MaxTanFieldOfViewLength, FrustumStartDistance * TanFieldOfView);
		const FVector StartCameraRightOffset = LocalShadowViewMatrices.GetViewMatrix().GetColumn(0) * StartHorizontalLength;
		const double StartVerticalLength = StartHorizontalLength / AspectRatio;
		const FVector StartCameraUpOffset = LocalShadowViewMatrices.GetViewMatrix().GetColumn(1) * StartVerticalLength;

		const double EndHorizontalLength = FMath::Min(MaxTanFieldOfViewLength, FrustumEndDistance * TanFieldOfView);
		const FVector EndCameraRightOffset = LocalShadowViewMatrices.GetViewMatrix().GetColumn(0) * EndHorizontalLength;
		const double EndVerticalLength = EndHorizontalLength / AspectRatio;
		const FVector EndCameraUpOffset = LocalShadowViewMatrices.GetViewMatrix().GetColumn(1) * EndVerticalLength;

		FVector SplitVertices[8];
		const FVector ShadowViewOrigin = LocalShadowViewMatrices.GetViewOrigin();

		SplitVertices[0] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance + StartCameraRightOffset + StartCameraUpOffset;
		SplitVertices[1] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance + StartCameraRightOffset - StartCameraUpOffset;
		SplitVertices[2] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance - StartCameraRightOffset + StartCameraUpOffset;
		SplitVertices[3] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance - StartCameraRightOffset - StartCameraUpOffset;

		SplitVertices[4] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance + EndCameraRightOffset + EndCameraUpOffset;
		SplitVertices[5] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance + EndCameraRightOffset - EndCameraUpOffset;
		SplitVertices[6] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance - EndCameraRightOffset + EndCameraUpOffset;
		SplitVertices[7] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance - EndCameraRightOffset - EndCameraUpOffset;

		FVector Center(0,0,0);
		// Weight the far vertices more so that the bounding sphere will be further from the camera
		// This minimizes wasted shadowmap space behind the viewer
		const double FarVertexWeightScale = 10.0;
		for (int32 VertexIndex = 0; VertexIndex < 8; VertexIndex++)
		{
			const double Weight = VertexIndex > 3 ? 1 / (4.0 + 4.0 / FarVertexWeightScale) : 1 / (4.0 + 4.0 * FarVertexWeightScale);
			Center += SplitVertices[VertexIndex] * Weight;
		}

		double RadiusSquared = 0;
		for (int32 VertexIndex = 0; VertexIndex < 8; VertexIndex++)
		{
			RadiusSquared = FMath::Max(RadiusSquared, (Center - SplitVertices[VertexIndex]).SizeSquared());
		}

		if (RadiusSquared > 0) // Avoid issues with bad cvar usage, e.g. r.TranslucencyLightingVolumeInnerDistance.
		{
			FSphere SphereBounds(Center, FMath::Sqrt(RadiusSquared));

			// Snap the center to a multiple of the volume dimension for stability
			const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();
			SphereBounds.Center.X = SphereBounds.Center.X - FMath::Fmod(SphereBounds.Center.X, SphereBounds.W * 2 / TranslucencyLightingVolumeDim);
			SphereBounds.Center.Y = SphereBounds.Center.Y - FMath::Fmod(SphereBounds.Center.Y, SphereBounds.W * 2 / TranslucencyLightingVolumeDim);
			SphereBounds.Center.Z = SphereBounds.Center.Z - FMath::Fmod(SphereBounds.Center.Z, SphereBounds.W * 2 / TranslucencyLightingVolumeDim);

			InOutCascadeBoundsArray[CascadeIndex] = FBox(SphereBounds);
		}
		else
		{
			InOutCascadeBoundsArray[CascadeIndex] = FBox(Center, Center);
		}
	}
}

class FTranslucencyDepthShaderElementData : public FMeshMaterialShaderElementData
{
public:
	float TranslucentShadowStartOffset;
};

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FTranslucencyDepthPassUniformParameters,)
	SHADER_PARAMETER_STRUCT(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
	SHADER_PARAMETER(float, bClampToNearPlane)
	SHADER_PARAMETER(float, InvMaxSubjectDepth)
	SHADER_PARAMETER_STRUCT(FTranslucentSelfShadowUniformParameters, TranslucentSelfShadow)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FTranslucencyDepthPassUniformParameters, "TranslucentDepthPass", SceneTextures);

void SetupTranslucencyDepthPassUniformBuffer(
	const FProjectedShadowInfo* ShadowInfo,
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FTranslucencyDepthPassUniformParameters& TranslucencyDepthPassParameters)
{
	// Note - scene depth can be bound by the material for use in depth fades
	// This is incorrect when rendering a shadowmap as it's not from the camera's POV
	// Set the scene depth texture to something safe when rendering shadow depths
	SetupSceneTextureUniformParameters(GraphBuilder, View.GetSceneTexturesChecked(), View.FeatureLevel, ESceneTextureSetupMode::None, TranslucencyDepthPassParameters.SceneTextures);

	TranslucencyDepthPassParameters.ProjectionMatrix = FTranslationMatrix44f(FVector3f(ShadowInfo->PreShadowTranslation - View.ViewMatrices.GetPreViewTranslation())) * ShadowInfo->TranslatedWorldToClipInnerMatrix;

	// Only clamp vertices to the near plane when rendering whole scene directional light shadow depths or preshadows from directional lights
	const bool bClampToNearPlaneValue = ShadowInfo->IsWholeSceneDirectionalShadow() || (ShadowInfo->bPreShadow && ShadowInfo->bDirectionalLight);
	TranslucencyDepthPassParameters.bClampToNearPlane = bClampToNearPlaneValue ? 1.0f : 0.0f;

	TranslucencyDepthPassParameters.InvMaxSubjectDepth = ShadowInfo->InvMaxSubjectDepth;

	SetupTranslucentSelfShadowUniformParameters(ShadowInfo, TranslucencyDepthPassParameters.TranslucentSelfShadow);
}

/**
* Vertex shader used to render shadow maps for translucency.
*/
class FTranslucencyShadowDepthVS : public FMeshMaterialShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FTranslucencyShadowDepthVS, NonVirtual);
public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return AllowTranslucencyPerObjectShadows(Parameters.Platform) && IsTranslucentBlendMode(Parameters.MaterialParameters);
	}

	FTranslucencyShadowDepthVS() {}
	FTranslucencyShadowDepthVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{}
};

enum ETranslucencyShadowDepthShaderMode
{
	TranslucencyShadowDepth_PerspectiveCorrect,
	TranslucencyShadowDepth_Standard,
};

template <ETranslucencyShadowDepthShaderMode ShaderMode>
class TTranslucencyShadowDepthVS : public FTranslucencyShadowDepthVS
{
	DECLARE_SHADER_TYPE(TTranslucencyShadowDepthVS, MeshMaterial);
public:

	TTranslucencyShadowDepthVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FTranslucencyShadowDepthVS(Initializer)
	{}

	TTranslucencyShadowDepthVS() {}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTranslucencyShadowDepthVS::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PERSPECTIVE_CORRECT_DEPTH"), (uint32)(ShaderMode == TranslucencyShadowDepth_PerspectiveCorrect ? 1 : 0));
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthVS<TranslucencyShadowDepth_PerspectiveCorrect>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainVS"),SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthVS<TranslucencyShadowDepth_Standard>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainVS"),SF_Vertex);

/**
 * Pixel shader used for accumulating translucency layer densities
 */
class FTranslucencyShadowDepthPS : public FMeshMaterialShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FTranslucencyShadowDepthPS, NonVirtual);
public:
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return AllowTranslucencyPerObjectShadows(Parameters.Platform) && IsTranslucentBlendMode(Parameters.MaterialParameters);
	}

	FTranslucencyShadowDepthPS() = default;
	FTranslucencyShadowDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
		TranslucentShadowStartOffset.Bind(Initializer.ParameterMap, TEXT("TranslucentShadowStartOffset"));
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FTranslucencyDepthShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);

		ShaderBindings.Add(TranslucentShadowStartOffset, ShaderElementData.TranslucentShadowStartOffset);
	}

private:
	LAYOUT_FIELD(FShaderParameter, TranslucentShadowStartOffset);
};

template <ETranslucencyShadowDepthShaderMode ShaderMode>
class TTranslucencyShadowDepthPS : public FTranslucencyShadowDepthPS
{
public:
	DECLARE_SHADER_TYPE(TTranslucencyShadowDepthPS, MeshMaterial);

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTranslucencyShadowDepthPS::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PERSPECTIVE_CORRECT_DEPTH"), (uint32)(ShaderMode == TranslucencyShadowDepth_PerspectiveCorrect ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
	}

	TTranslucencyShadowDepthPS() = default;
	TTranslucencyShadowDepthPS(const ShaderMetaType::CompiledShaderInitializerType & Initializer) :
		FTranslucencyShadowDepthPS(Initializer)
	{}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthPS<TranslucencyShadowDepth_PerspectiveCorrect>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainOpacityPS"),SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthPS<TranslucencyShadowDepth_Standard>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainOpacityPS"),SF_Pixel);

class FTranslucencyDepthPassMeshProcessor : public FMeshPassProcessor
{
public:
	FTranslucencyDepthPassMeshProcessor(const FScene* Scene,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		const FProjectedShadowInfo* InShadowInfo,
		FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch, 
		uint64 BatchElementMask, 
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, 
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material);

	template<ETranslucencyShadowDepthShaderMode ShaderMode>
	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		float MaterialTranslucentShadowStartOffset,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	FMeshPassProcessorRenderState PassDrawRenderState;
	const FProjectedShadowInfo* ShadowInfo;
	const bool bDirectionalLight;
};

FTranslucencyDepthPassMeshProcessor::FTranslucencyDepthPassMeshProcessor(const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	const FProjectedShadowInfo* InShadowInfo,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::Num, Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InPassDrawRenderState)
	, ShadowInfo(InShadowInfo)
	, bDirectionalLight(InShadowInfo->bDirectionalLight)
{
}

bool FTranslucencyDepthPassMeshProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch, 
	uint64 BatchElementMask, 
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, 
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material)
{
	// Determine the mesh's material and blend mode.
	const float MaterialTranslucentShadowStartOffset = Material.GetTranslucentShadowStartOffset();
	const bool MaterialCastDynamicShadowAsMasked = Material.GetCastDynamicShadowAsMasked();
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);
	const bool bIsTranslucent = IsTranslucentBlendMode(Material);

	// Only render translucent meshes into the Fourier opacity maps
	if (bIsTranslucent && ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain()) && !MaterialCastDynamicShadowAsMasked)
	{
		if (bDirectionalLight)
		{
			return Process<TranslucencyShadowDepth_Standard>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, MaterialTranslucentShadowStartOffset, MeshFillMode, MeshCullMode);
		}
		else
		{
			return Process<TranslucencyShadowDepth_PerspectiveCorrect>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, MaterialTranslucentShadowStartOffset, MeshFillMode, MeshCullMode);
		}
	}

	return true;
}

template<ETranslucencyShadowDepthShaderMode ShaderMode>
bool FTranslucencyDepthPassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	float MaterialTranslucentShadowStartOffset,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		TTranslucencyShadowDepthVS<ShaderMode>,
		TTranslucencyShadowDepthPS<ShaderMode>> PassShaders;

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<TTranslucencyShadowDepthVS<ShaderMode>>();
	ShaderTypes.AddShaderType<TTranslucencyShadowDepthPS<ShaderMode>>();

	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(PassShaders.VertexShader);
	Shaders.TryGetPixelShader(PassShaders.PixelShader);

	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	FTranslucencyDepthShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const float LocalToWorldScale = ShadowInfo->GetParentSceneInfo()->Proxy->GetLocalToWorld().GetScaleVector().GetMax();
	const float TranslucentShadowStartOffsetValue = MaterialTranslucentShadowStartOffset * LocalToWorldScale;
	ShaderElementData.TranslucentShadowStartOffset = TranslucentShadowStartOffsetValue / (ShadowInfo->MaxSubjectZ - ShadowInfo->MinSubjectZ);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);

	return true;
}

void FTranslucencyDepthPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.CastShadow)
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxy)
		{
			const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			if (Material && Material->GetRenderingThreadShaderMap())
			{
				if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
				{
					break;
				}
			}

			MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
		}
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FTranslucencyDepthPassParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FTranslucencyDepthPassUniformParameters, PassUniformBuffer)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void FProjectedShadowInfo::RenderTranslucencyDepths(FRDGBuilder& GraphBuilder, FSceneRenderer* SceneRenderer, const FRenderTargetBindingSlots& InRenderTargets, FInstanceCullingManager& InstanceCullingManager)
{
	check(IsInRenderingThread());
	checkSlow(!bWholeSceneShadow);
	SCOPE_CYCLE_COUNTER(STAT_RenderPerObjectShadowDepthsTime);

	BeginRenderView(GraphBuilder, SceneRenderer->Scene);

	auto* TranslucencyDepthPassParameters = GraphBuilder.AllocParameters<FTranslucencyDepthPassUniformParameters>();
	SetupTranslucencyDepthPassUniformBuffer(this, GraphBuilder, *ShadowDepthView, *TranslucencyDepthPassParameters);
	TRDGUniformBufferRef<FTranslucencyDepthPassUniformParameters> PassUniformBuffer = GraphBuilder.CreateUniformBuffer(TranslucencyDepthPassParameters);

	auto* PassParameters = GraphBuilder.AllocParameters<FTranslucencyDepthPassParameters>();
	PassParameters->View = ShadowDepthView->ViewUniformBuffer;
	PassParameters->PassUniformBuffer = PassUniformBuffer;
	PassParameters->RenderTargets = InRenderTargets;

	FSimpleMeshDrawCommandPass* SimpleMeshDrawCommandPass = GraphBuilder.AllocObject<FSimpleMeshDrawCommandPass>(*ShadowDepthView, &InstanceCullingManager);

	FMeshPassProcessorRenderState DrawRenderState;
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	DrawRenderState.SetBlendState(TStaticBlendState<
		CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One,
		CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());


	FTranslucencyDepthPassMeshProcessor TranslucencyDepthPassMeshProcessor(
		SceneRenderer->Scene,
		ShadowDepthView,
		DrawRenderState,
		this,
		SimpleMeshDrawCommandPass->GetDynamicPassMeshDrawListContext());

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicSubjectTranslucentMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshAndRelevance = DynamicSubjectTranslucentMeshElements[MeshBatchIndex];
		const uint64 BatchElementMask = ~0ull;
		TranslucencyDepthPassMeshProcessor.AddMeshBatch(*MeshAndRelevance.Mesh, BatchElementMask, MeshAndRelevance.PrimitiveSceneProxy);
	}

	for (int32 PrimitiveIndex = 0; PrimitiveIndex < SubjectTranslucentPrimitives.Num(); PrimitiveIndex++)
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = SubjectTranslucentPrimitives[PrimitiveIndex];
		int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
		FPrimitiveViewRelevance ViewRelevance = ShadowDepthView->PrimitiveViewRelevanceMap[PrimitiveId];

		if (!ViewRelevance.bInitializedThisFrame)
		{
			// Compute the subject primitive's view relevance since it wasn't cached
			ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(ShadowDepthView);
		}

		if (ViewRelevance.bDrawRelevance && ViewRelevance.bStaticRelevance)
		{
			int8 MinLOD, MaxLOD;
			PrimitiveSceneInfo->GetStaticMeshesLODRange(MinLOD, MaxLOD);
			// For any primitive, we only render LOD0 meshes since we do not have FSceneView available to use ComputeLODForMeshes.
			for (int32 MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
			{
				const FStaticMeshBatch& StaticMeshBatch = PrimitiveSceneInfo->StaticMeshes[MeshIndex];
				if (StaticMeshBatch.LODIndex != MinLOD)
				{
					continue;
				}
				const uint64 DefaultBatchElementMask = ~0ul;
				TranslucencyDepthPassMeshProcessor.AddMeshBatch(StaticMeshBatch, DefaultBatchElementMask, StaticMeshBatch.PrimitiveSceneInfo->Proxy, StaticMeshBatch.Id);
			}
		}
	}

	SimpleMeshDrawCommandPass->BuildRenderingCommands(GraphBuilder, *ShadowDepthView, SceneRenderer->Scene->GPUScene, PassParameters->InstanceCullingDrawParams);


	FString EventName;
#if WANTS_DRAW_MESH_EVENTS
	if (GetEmitDrawEvents())
	{
		GetShadowTypeNameForDrawEvent(EventName);
	}
#endif

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("%s", *EventName),
		PassParameters,
		ERDGPassFlags::Raster,
		[this, SimpleMeshDrawCommandPass, PassParameters](FRDGAsyncTask, FRHICommandList& RHICmdList)
	{
		FMeshPassProcessorRenderState DrawRenderState;

		// Clear the shadow and its border
		RHICmdList.SetViewport(
			X,
			Y,
			0.0f,
			(X + BorderSize * 2 + ResolutionX),
			(Y + BorderSize * 2 + ResolutionY),
			1.0f
		);

		FLinearColor ClearColors[2] = { FLinearColor(0,0,0,0), FLinearColor(0,0,0,0) };
		DrawClearQuadMRT(RHICmdList, true, UE_ARRAY_COUNT(ClearColors), ClearColors, false, 1.0f, false, 0);

		// Set the viewport for the shadow.
		RHICmdList.SetViewport(
			(X + BorderSize),
			(Y + BorderSize),
			0.0f,
			(X + BorderSize + ResolutionX),
			(Y + BorderSize + ResolutionY),
			1.0f
		);
		SimpleMeshDrawCommandPass->SubmitDraw(RHICmdList, PassParameters->InstanceCullingDrawParams);
	});
}

class FGatherMarkedVoxelsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGatherMarkedVoxelsCS);
	SHADER_USE_PARAMETER_STRUCT(FGatherMarkedVoxelsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, RWVoxelAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, RWVoxelData)

		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, VolumeMarkTexture)

		SHADER_PARAMETER(FIntVector, VolumeSize)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static FIntVector GetGroupSize()
	{
		return FIntVector(4, 4, 4);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FIntVector GroupSize = GetGroupSize();

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), GroupSize.X);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), GroupSize.Y);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Z"), GroupSize.Z);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGatherMarkedVoxelsCS, "/Engine/Private/TranslucentLightingShaders.usf", "GatherMarkedVoxelsCS", SF_Compute);

class FInitIndirectArgsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FInitIndirectArgsCS);
	SHADER_USE_PARAMETER_STRUCT(FInitIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, RWIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, VoxelAllocator)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitIndirectArgsCS, "/Engine/Private/TranslucentLightingShaders.usf", "InitIndirectArgsCS", SF_Compute);

/** Compute shader used to filter a single volume lighting cascade. */
class FFilterTranslucentVolumeCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFilterTranslucentVolumeCS);
	SHADER_USE_PARAMETER_STRUCT(FFilterTranslucentVolumeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, RWTranslucencyLightingVolumeAmbient)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, RWTranslucencyLightingVolumeDirectional)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyLightingVolumeAmbient)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyLightingVolumeDirectional)
		SHADER_PARAMETER_SAMPLER(SamplerState, TranslucencyLightingVolumeAmbientSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, TranslucencyLightingVolumeDirectionalSampler)

		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, HistoryAmbient)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, HistoryDirectional)
		SHADER_PARAMETER_SAMPLER(SamplerState, HistoryAmbientSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, HistoryDirectionalSampler)

		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, HistoryMark)

		SHADER_PARAMETER(FIntVector, VolumeSize)
		SHADER_PARAMETER(float, TexelSize)
		SHADER_PARAMETER(uint32, VolumeCascadeIndex)

		SHADER_PARAMETER(FVector4f, PrevTranslucencyLightingVolumeMin)
		SHADER_PARAMETER(FVector4f, PrevTranslucencyLightingVolumeInvSize)

		SHADER_PARAMETER(FVector3f, HistoryTextureBilinearUVMin)
		SHADER_PARAMETER(FVector3f, HistoryTextureBilinearUVMax)

		SHADER_PARAMETER(float, HistoryWeight)
	END_SHADER_PARAMETER_STRUCT()

	class FUseTemporalReprojection : SHADER_PERMUTATION_BOOL("USE_TEMPORAL_REPROJECTION");
	class FCheckHistoryMark : SHADER_PERMUTATION_BOOL("CHECK_HISTORY_MARK");
	using FPermutationDomain = TShaderPermutationDomain<FUseTemporalReprojection, FCheckHistoryMark>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static FIntVector GetGroupSize()
	{
		return FIntVector(4, 4, 4);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FIntVector GroupSize = GetGroupSize();

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), GroupSize.X);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), GroupSize.Y);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Z"), GroupSize.Z);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFilterTranslucentVolumeCS, "/Engine/Private/TranslucentLightingShaders.usf", "FilterTranslucentVolumeCS", SF_Compute);

/** Shader that adds direct lighting contribution from the given light to the current volume lighting cascade. */
class FTranslucentLightingInjectPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FTranslucentLightingInjectPS, Material);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrint)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FDeferredLightUniformStruct, DeferredLight)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeShadowingShaderParameters, VolumeShadowingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, VirtualShadowMapSamplingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLightCloudTransmittanceParameters, LightCloudTransmittanceParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FAdaptiveVolumetricShadowMapUniformBufferParameters, AVSM)
		SHADER_PARAMETER(FMatrix44f, LightFunctionTranslatedWorldToLight)
		SHADER_PARAMETER(FVector4f, LightFunctionParameters)
		SHADER_PARAMETER(FVector3f, CameraRelativeLightPosition)
		SHADER_PARAMETER(float, SpotlightMask)
		SHADER_PARAMETER(uint32, VolumeCascadeIndex)
		SHADER_PARAMETER(int32, VirtualShadowMapId)
		SHADER_PARAMETER(uint32, AtmospherePerPixelTransmittanceEnabled)
		SHADER_PARAMETER(uint32, VolumetricCloudShadowEnabled)
	END_SHADER_PARAMETER_STRUCT()

	class FRadialAttenuation	: SHADER_PERMUTATION_BOOL("RADIAL_ATTENUATION");
	class FDynamicallyShadowed	: SHADER_PERMUTATION_BOOL("DYNAMICALLY_SHADOWED");
	class FLightFunction		: SHADER_PERMUTATION_BOOL("APPLY_LIGHT_FUNCTION");
	class FVirtualShadowMap		: SHADER_PERMUTATION_BOOL("VIRTUAL_SHADOW_MAP");
	class FAdaptiveVolumetricShadowMap : SHADER_PERMUTATION_BOOL("ADAPTIVE_VOLUMETRIC_SHADOW_MAP");

	using FPermutationDomain = TShaderPermutationDomain<
		FRadialAttenuation,
		FDynamicallyShadowed,
		FLightFunction,
		FVirtualShadowMap,
		FAdaptiveVolumetricShadowMap >;

public:

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INJECTION_PIXEL_SHADER"), 1);
	}

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (!DoesPlatformSupportVirtualShadowMaps(Parameters.Platform) && PermutationVector.Get<FVirtualShadowMap>() != 0)
		{
			return false;
		}

		if (!DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform) && PermutationVector.Get<FAdaptiveVolumetricShadowMap>() != 0)
		{
			return false;
		}

		return (Parameters.MaterialParameters.MaterialDomain == MD_LightFunction || Parameters.MaterialParameters.bIsSpecialEngineMaterial) &&
			(IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			(RHISupportsGeometryShaders(Parameters.Platform) || RHISupportsVertexShaderLayer(Parameters.Platform)));
	}

	FTranslucentLightingInjectPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMaterialShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters( 
			this, 
			Initializer.PermutationId, 
			Initializer.ParameterMap,
			*FParameters::FTypeInfo::GetStructMetadata(), 
			// Don't require full bindings, we use FMaterialShader::SetParameters
			false); 
	}

	FTranslucentLightingInjectPS() {}

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		const FViewInfo& View, 
		const FMaterialRenderProxy* MaterialProxy)
	{
		const FMaterial& Material = MaterialProxy->GetMaterialWithFallback(View.GetFeatureLevel(), MaterialProxy);
		FMaterialShader::SetParameters(BatchedParameters, MaterialProxy, Material, View);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FTranslucentLightingInjectPS, TEXT("/Engine/Private/TranslucentLightInjectionShaders.usf"), TEXT("InjectMainPS"), SF_Pixel);

/** Shader that adds direct lighting contribution from multiple lights. */
class FTranslucentLightingInjectBatchCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTranslucentLightingInjectBatchCS);
	SHADER_USE_PARAMETER_STRUCT(FTranslucentLightingInjectBatchCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightUniformParameters, ForwardLightStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, VirtualShadowMapSamplingParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLightFunctionAtlasGlobalParameters, LightFunctionAtlas)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BatchedLocalLights)
		SHADER_PARAMETER(uint32, MaxBatchedLocalLights)
		SHADER_PARAMETER(uint32, VolumeCascadeIndex)
		SHADER_PARAMETER(FIntVector, VolumeSize)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, RWTranslucencyLightingVolumeAmbient)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, RWTranslucencyLightingVolumeDirectional)

		// used when UAV typed loads are not supported
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyLightingVolumeAmbient)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyLightingVolumeDirectional)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, VoxelAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, VoxelData)

		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FVirtualShadowMap : SHADER_PERMUTATION_BOOL("VIRTUAL_SHADOW_MAP");
	class FUseLightFunctionAtlas : SHADER_PERMUTATION_BOOL("USE_LIGHT_FUNCTION_ATLAS");
	class FIndirectVoxelDispatch : SHADER_PERMUTATION_BOOL("INDIRECT_VOXEL_DISPATCH");
	class FUseUAVTypedLoad : SHADER_PERMUTATION_BOOL("USE_UAV_TYPED_LOAD");
	using FPermutationDomain = TShaderPermutationDomain<FVirtualShadowMap, FUseLightFunctionAtlas, FIndirectVoxelDispatch, FUseUAVTypedLoad>;

public:

	static FIntVector GetGroupSize()
	{
		return FIntVector(4, 4, 4);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);

		const FIntVector GroupSize = GetGroupSize();
		
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FIndirectVoxelDispatch>())
		{
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), GroupSize.X * GroupSize.Y * GroupSize.Z);
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 1);
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Z"), 1);
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), GroupSize.X);
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), GroupSize.Y);
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Z"), GroupSize.Z);
		}

		// This shader must support typed UAV load and we are testing if it is supported at runtime
		// using UE::PixelFormat::HasCapabilities(..., EPixelFormatCapabilities::TypedUAVLoad)
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (!DoesPlatformSupportVirtualShadowMaps(Parameters.Platform) && PermutationVector.Get<FVirtualShadowMap>() != 0)
		{
			return false;
		}

		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTranslucentLightingInjectBatchCS, "/Engine/Private/TranslucentLightInjectionShaders.usf", "InjectBatchMainCS", SF_Compute);



class FClearTranslucentLightingVolumeCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClearTranslucentLightingVolumeCS);
	SHADER_USE_PARAMETER_STRUCT(FClearTranslucentLightingVolumeCS, FGlobalShader)

	static const int32 CLEAR_BLOCK_SIZE = 4;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWAmbient0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWDirectional0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWAmbient1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWDirectional1)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CLEAR_COMPUTE_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("CLEAR_BLOCK_SIZE"), CLEAR_BLOCK_SIZE);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearTranslucentLightingVolumeCS, "/Engine/Private/TranslucentLightInjectionShaders.usf", "ClearTranslucentLightingVolumeCS", SF_Compute);

void FTranslucencyLightingVolumeTextures::GetTextureFormatAndCreationFlags(EPixelFormat& OutPixelFormat, ETextureCreateFlags& OutCreationFlags)
{
	OutPixelFormat = PF_FloatRGBA;

	// TODO: We can skip the and TLV allocations when rendering in forward shading mode
	OutCreationFlags = TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_ReduceMemoryWithTilingMode | TexCreate_UAV;
}


int32 FTranslucencyLightingVolumeTextures::GetIndex(const FViewInfo& View, int32 CascadeIndex) const
{
	// if we only have one view or one stereo pair we can just use primary index
	if (Directional.Num() == TVC_MAX)
	{
		return (View.PrimaryViewIndex * TVC_MAX) + CascadeIndex;
	}
	else
	{
		// support uncommon but possible (in theory) situations, like a stereo pair and also multiple views
		return (ViewsToTexturePairs[View.PrimaryViewIndex] * TVC_MAX) + CascadeIndex;
	}
}

void FTranslucencyLightingVolumeTextures::Init(FRDGBuilder& GraphBuilder, TArrayView<const FViewInfo> Views, ERDGPassFlags PassFlags)
{
	// Skip init/clear if disabled
	// GetTranslucencyLightingVolumeParameters will return black system textures
	if (!GUseTranslucentLightingVolumes)
	{
		return;
	}

	check(PassFlags == ERDGPassFlags::Compute || PassFlags == ERDGPassFlags::AsyncCompute);

	RDG_EVENT_SCOPE_STAT(GraphBuilder, TranslucentLighting, "InitTranslucencyLightingVolumeTextures");
	RDG_GPU_STAT_SCOPE(GraphBuilder, TranslucentLighting);

	VolumeDim = GetTranslucencyLightingVolumeDim();
	const FIntVector TranslucencyLightingVolumeDim(VolumeDim);

	EPixelFormat TranslucencyPixelFormat;
	ETextureCreateFlags TranslucencyTargetFlags;
	GetTextureFormatAndCreationFlags(TranslucencyPixelFormat, TranslucencyTargetFlags);

	// calculate the number of textures needed given that for each stereo pair the primary view's textures will be shared between the "eyes"
	const int32 ViewCount = Views.Num();
	uint32 NumViewsWithTextures = 0;
	ViewsToTexturePairs.SetNumZeroed(Views.Num());
	for (int32 ViewIndex = 0, NumViews = Views.Num(); ViewIndex < NumViews; ++ViewIndex)
	{
		ViewsToTexturePairs[ViewIndex] = NumViewsWithTextures;
		NumViewsWithTextures += (ViewIndex == Views[ViewIndex].PrimaryViewIndex) ? 1 : 0;	// this will add 0 for those views who aren't primary
	}
	check(NumViewsWithTextures > 0);
	{
		Ambient.SetNum(NumViewsWithTextures * TVC_MAX);
		Directional.SetNum(NumViewsWithTextures * TVC_MAX);

		for (int32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			for (int32 CascadeIndex = 0; CascadeIndex < TVC_MAX; ++CascadeIndex)
			{
				const uint32 TextureIndex = FTranslucencyLightingVolumeTextures::GetIndex(View, CascadeIndex);
				check(TextureIndex <= NumViewsWithTextures * TVC_MAX);

				FRDGTextureRef AmbientTexture = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create3D(
						TranslucencyLightingVolumeDim,
						TranslucencyPixelFormat,
						FClearValueBinding::Transparent,
						TranslucencyTargetFlags),
					TEXT("TranslucentVolumeAmbient"));

				FRDGTextureRef DirectionalTexture = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create3D(
						TranslucencyLightingVolumeDim,
						TranslucencyPixelFormat,
						FClearValueBinding::Transparent,
						TranslucencyTargetFlags),
					TEXT("TranslucentVolumeDirectional"));

				Ambient[TextureIndex] = AmbientTexture;
				Directional[TextureIndex] = DirectionalTexture;
			}
		}
	}

	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(TranslucencyLightingVolumeDim, FClearTranslucentLightingVolumeCS::CLEAR_BLOCK_SIZE);

	TShaderMapRef<FClearTranslucentLightingVolumeCS> ComputeShader(Views[0].ShaderMap);

	for (uint32 TexturePairIndex = 0; TexturePairIndex < NumViewsWithTextures; ++TexturePairIndex)
	{
		auto* PassParameters = GraphBuilder.AllocParameters<FClearTranslucentLightingVolumeCS::FParameters>();
		PassParameters->RWAmbient0 = GraphBuilder.CreateUAV(Ambient[TexturePairIndex * TVC_MAX]);
		PassParameters->RWAmbient1 = GraphBuilder.CreateUAV(Ambient[TexturePairIndex * TVC_MAX + 1]);
		PassParameters->RWDirectional0 = GraphBuilder.CreateUAV(Directional[TexturePairIndex * TVC_MAX]);
		PassParameters->RWDirectional1 = GraphBuilder.CreateUAV(Directional[TexturePairIndex * TVC_MAX + 1]);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClearTranslucencyLightingVolumeCompute %d", VolumeDim),
			PassFlags,
			ComputeShader,
			PassParameters,
			GroupCount);
	}
}

FTranslucencyLightingVolumeParameters GetTranslucencyLightingVolumeParameters(FRDGBuilder& GraphBuilder, const FTranslucencyLightingVolumeTextures& Textures, const FViewInfo& View)
{
	FTranslucencyLightingVolumeParameters Parameters;
	if (Textures.IsValid())
	{
		const uint32 InnerIndex = Textures.GetIndex(View, TVC_Inner);
		const uint32 OuterIndex = Textures.GetIndex(View, TVC_Outer);

		Parameters.TranslucencyLightingVolumeAmbientInner = Textures.Ambient[InnerIndex];
		Parameters.TranslucencyLightingVolumeAmbientOuter = Textures.Ambient[OuterIndex];
		Parameters.TranslucencyLightingVolumeDirectionalInner = Textures.Directional[InnerIndex];
		Parameters.TranslucencyLightingVolumeDirectionalOuter = Textures.Directional[OuterIndex];
	}
	else
	{
		const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
		Parameters.TranslucencyLightingVolumeAmbientInner = SystemTextures.VolumetricBlack;
		Parameters.TranslucencyLightingVolumeAmbientOuter = SystemTextures.VolumetricBlack;
		Parameters.TranslucencyLightingVolumeDirectionalInner = SystemTextures.VolumetricBlack;
		Parameters.TranslucencyLightingVolumeDirectionalOuter = SystemTextures.VolumetricBlack;
	}

	Parameters.TranslucencyLightingRandomPositionOffsetRadius = GetTranslucencyLightingVolumePositionOffsetRadius();

	return Parameters;
}

bool IsTranslucencyLightingVolumeUsingVoxelMarkingSupported()
{
	return CVarTranslucencyLightingVolumeMarkVoxelsSupported.GetValueOnAnyThread();
}

bool IsTranslucencyLightingVolumeUsingVoxelMarking()
{
	return GUseTranslucentLightingVolumes && IsTranslucencyLightingVolumeUsingVoxelMarkingSupported() && CVarTranslucencyLightingVolumeMarkVoxels.GetValueOnAnyThread();
}

bool IsTranslucencyLightingVolumeUsingBlueNoise()
{
	return GUseTranslucentLightingVolumes && GetTranslucencyLightingVolumePositionOffsetRadius();
}

class FInjectAmbientCubemapPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FInjectAmbientCubemapPS);
	SHADER_USE_PARAMETER_STRUCT(FInjectAmbientCubemapPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FAmbientCubemapParameters, AmbientCubemap)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FInjectAmbientCubemapPS, "/Engine/Private/TranslucentLightingShaders.usf", "InjectAmbientCubemapMainPS", SF_Pixel);

void InjectTranslucencyLightingVolumeAmbientCubemap(
	FRDGBuilder& GraphBuilder,
	const TArrayView<const FViewInfo> Views,
	const FTranslucencyLightingVolumeTextures& Textures)
{
	if (!GUseTranslucentLightingVolumes || Views.Num() == 0 || !RHISupportsVolumeTextureRendering(Views[0].GetShaderPlatform()))
	{
		return;
	}

	RDG_EVENT_SCOPE_STAT(GraphBuilder, TranslucentLighting, "InjectAmbientCubemapTranslucentVolumeLighting");
	RDG_GPU_STAT_SCOPE(GraphBuilder, TranslucentLighting);

	const int32 TranslucencyLightingVolumeDim = Textures.VolumeDim;
	const FVolumeBounds VolumeBounds(TranslucencyLightingVolumeDim);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

		for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; ++VolumeCascadeIndex)
		{
			FRDGTextureRef VolumeAmbientTexture = Textures.GetAmbientTexture(View, VolumeCascadeIndex);

			for (const FFinalPostProcessSettings::FCubemapEntry& CubemapEntry : View.FinalPostProcessSettings.ContributingCubemaps)
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FInjectAmbientCubemapPS::FParameters>();
				SetupAmbientCubemapParameters(CubemapEntry, &PassParameters->AmbientCubemap);
				PassParameters->RenderTargets[0] = FRenderTargetBinding(VolumeAmbientTexture, ERenderTargetLoadAction::ELoad);
				PassParameters->View = View.ViewUniformBuffer;

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("Cascade %d", VolumeCascadeIndex),
					PassParameters,
					ERDGPassFlags::Raster,
					[&View, PassParameters, VolumeBounds, TranslucencyLightingVolumeDim](FRDGAsyncTask, FRHICommandList& RHICmdList)
				{
					TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
					TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);
					TShaderMapRef<FInjectAmbientCubemapPS> PixelShader(View.ShaderMap);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParametersLegacyVS(RHICmdList, VertexShader, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
					if (GeometryShader.IsValid())
					{
						SetShaderParametersLegacyGS(RHICmdList, GeometryShader, VolumeBounds.MinZ);
					}
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

					RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
				});
			}
		}
	}
}

class FInjectMegaLightsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FInjectMegaLightsCS)
	SHADER_USE_PARAMETER_STRUCT(FInjectMegaLightsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, RWTranslucencyLightingVolumeAmbient)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, RWTranslucencyLightingVolumeDirectional)

		// used when UAV typed loads are not supported
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyLightingVolumeAmbient)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyLightingVolumeDirectional)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, MegaLightsAmbient)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, MegaLightsDirectional)

		SHADER_PARAMETER(FIntVector, VolumeSize)
		SHADER_PARAMETER(uint32, VolumeCascadeIndex)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, VoxelAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, VoxelData)

		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FIndirectVoxelDispatch : SHADER_PERMUTATION_BOOL("INDIRECT_VOXEL_DISPATCH");
	class FUseUAVTypedLoad : SHADER_PERMUTATION_BOOL("USE_UAV_TYPED_LOAD");
	using FPermutationDomain = TShaderPermutationDomain<FIndirectVoxelDispatch, FUseUAVTypedLoad>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static FIntVector GetGroupSize()
	{
		return FIntVector(4, 4, 4);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FIntVector GroupSize = GetGroupSize();

		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FIndirectVoxelDispatch>())
		{
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), GroupSize.X * GroupSize.Y * GroupSize.Z);
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 1);
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Z"), 1);
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), GroupSize.X);
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), GroupSize.Y);
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Z"), GroupSize.Z);
		}

		// This shader must support typed UAV load and we are testing if it is supported at runtime
		// using UE::PixelFormat::HasCapabilities(..., EPixelFormatCapabilities::TypedUAVLoad)
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FInjectMegaLightsCS, "/Engine/Private/TranslucentLightInjectionShaders.usf", "InjectMegaLightsCS", SF_Compute);

void InjectTranslucencyLightingVolumeMegaLights(
	FRDGBuilder& GraphBuilder,
	const TArrayView<const FViewInfo> Views,
	const FTranslucencyLightingVolumeTextures& Textures)
{
	if (!GUseTranslucentLightingVolumes || Views.Num() == 0 || !MegaLights::IsEnabled(*Views[0].Family) || !MegaLights::UseTranslucencyVolume())
	{
		return;
	}

	RDG_EVENT_SCOPE_STAT(GraphBuilder, TranslucentLighting, "InjectTranslucencyLightingVolumeMegaLights");
	RDG_GPU_STAT_SCOPE(GraphBuilder, TranslucentLighting);

	const FIntVector VolumeSize = FIntVector(GetTranslucencyLightingVolumeDim());

	EPixelFormat TranslucencyPixelFormat;
	ETextureCreateFlags TranslucencyTargetFlags;
	FTranslucencyLightingVolumeTextures::GetTextureFormatAndCreationFlags(TranslucencyPixelFormat, TranslucencyTargetFlags);

	const bool bUseUAVTypedLoad = UE::PixelFormat::HasCapabilities(TranslucencyPixelFormat, EPixelFormatCapabilities::TypedUAVLoad);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

		for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; ++VolumeCascadeIndex)
		{
			const bool bUseVolumeMarkTexture = IsTranslucencyLightingVolumeUsingVoxelMarking() && View.TranslucencyVolumeMarkData[VolumeCascadeIndex].MarkTexture != nullptr;

			// for stereo case, using PrimaryViewIndex essentially shares the lighting volume textures
			const uint32 TextureIndex = Textures.GetIndex(View, VolumeCascadeIndex);
			FRDGTextureRef VolumeAmbientTexture = Textures.Ambient[TextureIndex];
			FRDGTextureRef VolumeDirectionalTexture = Textures.Directional[TextureIndex];

			FRDGTextureRef MegaLightsAmbient = View.GetMegaLightsVolume().TranslucencyAmbient[VolumeCascadeIndex];
			FRDGTextureRef MegaLightsDirectional = View.GetMegaLightsVolume().TranslucencyDirectional[VolumeCascadeIndex];

			auto* PassParameters = GraphBuilder.AllocParameters<FInjectMegaLightsCS::FParameters>();

			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->VolumeSize = VolumeSize;
			PassParameters->VolumeCascadeIndex = VolumeCascadeIndex;

			PassParameters->MegaLightsAmbient = MegaLightsAmbient ? GraphBuilder.CreateSRV(MegaLightsAmbient) : nullptr;
			PassParameters->MegaLightsDirectional = MegaLightsDirectional ? GraphBuilder.CreateSRV(MegaLightsDirectional) : nullptr;

			if (bUseUAVTypedLoad)
			{
				PassParameters->RWTranslucencyLightingVolumeAmbient = GraphBuilder.CreateUAV(VolumeAmbientTexture);
				PassParameters->RWTranslucencyLightingVolumeDirectional = GraphBuilder.CreateUAV(VolumeDirectionalTexture);
			}
			else
			{
				// need to output to auxiliary textures to be able to read from existing volumes

				FRDGTextureRef OutputVolumeAmbientTexture = GraphBuilder.CreateTexture(VolumeAmbientTexture->Desc, VolumeAmbientTexture->Name);
				FRDGTextureRef OutputVolumeDirectionalTexture = GraphBuilder.CreateTexture(VolumeDirectionalTexture->Desc, VolumeDirectionalTexture->Name);

				PassParameters->RWTranslucencyLightingVolumeAmbient = GraphBuilder.CreateUAV(OutputVolumeAmbientTexture);
				PassParameters->RWTranslucencyLightingVolumeDirectional = GraphBuilder.CreateUAV(OutputVolumeDirectionalTexture);

				PassParameters->TranslucencyLightingVolumeAmbient = VolumeAmbientTexture;
				PassParameters->TranslucencyLightingVolumeDirectional = VolumeDirectionalTexture;

				VolumeAmbientTexture = OutputVolumeAmbientTexture;
				VolumeDirectionalTexture = OutputVolumeDirectionalTexture;
			}

			FInjectMegaLightsCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FInjectMegaLightsCS::FIndirectVoxelDispatch>(bUseVolumeMarkTexture);
			PermutationVector.Set<FInjectMegaLightsCS::FUseUAVTypedLoad>(bUseUAVTypedLoad);

			auto ComputeShader = View.ShaderMap->GetShader<FInjectMegaLightsCS>(PermutationVector);

			if (bUseVolumeMarkTexture)
			{
				PassParameters->VoxelAllocator = GraphBuilder.CreateSRV(View.TranslucencyVolumeMarkData[VolumeCascadeIndex].VoxelAllocator);
				PassParameters->VoxelData = GraphBuilder.CreateSRV(View.TranslucencyVolumeMarkData[VolumeCascadeIndex].VoxelData);
				PassParameters->IndirectArgs = View.TranslucencyVolumeMarkData[VolumeCascadeIndex].VoxelIndirectArgs;

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("InjectMegaLights(VolumeCascade=%d)", VolumeCascadeIndex),
					ComputeShader,
					PassParameters,
					View.TranslucencyVolumeMarkData[VolumeCascadeIndex].VoxelIndirectArgs,
					0);
			}
			else
			{
				const FIntVector NumGroups = FComputeShaderUtils::GetGroupCount(VolumeSize, FInjectMegaLightsCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("InjectMegaLights(VolumeCascade=%d)", VolumeCascadeIndex),
					ComputeShader,
					PassParameters,
					NumGroups);
			}
		}
	}
}

/** Calculates volume texture bounds for the given light in the given translucent lighting volume cascade. */
FVolumeBounds CalculateLightVolumeBounds(const FSphere& LightBounds, const FViewInfo& View, uint32 VolumeCascadeIndex, bool bDirectionalLight)
{
	const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

	FVolumeBounds VolumeBounds;

	if (bDirectionalLight)
	{
		VolumeBounds = FVolumeBounds(TranslucencyLightingVolumeDim);
	}
	else
	{
		// Determine extents in the volume texture
		const FVector MinPosition = (LightBounds.Center - LightBounds.W - View.TranslucencyLightingVolumeMin[VolumeCascadeIndex]) / View.TranslucencyVolumeVoxelSize[VolumeCascadeIndex];
		const FVector MaxPosition = (LightBounds.Center + LightBounds.W - View.TranslucencyLightingVolumeMin[VolumeCascadeIndex]) / View.TranslucencyVolumeVoxelSize[VolumeCascadeIndex];

		VolumeBounds.MinX = FMath::Max(FMath::TruncToInt32(MinPosition.X), 0);
		VolumeBounds.MinY = FMath::Max(FMath::TruncToInt32(MinPosition.Y), 0);
		VolumeBounds.MinZ = FMath::Max(FMath::TruncToInt32(MinPosition.Z), 0);

		VolumeBounds.MaxX = FMath::Min(FMath::TruncToInt32(MaxPosition.X) + 1, TranslucencyLightingVolumeDim);
		VolumeBounds.MaxY = FMath::Min(FMath::TruncToInt32(MaxPosition.Y) + 1, TranslucencyLightingVolumeDim);
		VolumeBounds.MaxZ = FMath::Min(FMath::TruncToInt32(MaxPosition.Z) + 1, TranslucencyLightingVolumeDim);
	}

	return VolumeBounds;
}

FTranslucentLightInjectionCollector::FTranslucentLightInjectionCollector(
	FRDGBuilder& GraphBuilder,
	TArrayView<const FViewInfo> Views,
	bool bAreLightsInLightGrid)
	// NOTE: This data is directly referenced inside the render pass lamba, so must be allocated in the graph
	: InjectionDataPerView(*GraphBuilder.AllocObject<TArray<FPerViewData, SceneRenderingAllocator>>())
{
	InjectionDataPerView.SetNum(Views.Num());

	// Static conditions for supporting batching
	bCollectorSupportsBatching =
		CVarTranslucencyLightingVolumeBatch.GetValueOnRenderThread() != 0 &&
		bAreLightsInLightGrid;
}

/**
 * Adds a light to LightInjectionData if it should be injected into the translucent volume, and caches relevant information in a FTranslucentLightInjectionData.
 * @param InProjectedShadowInfo is 0 for unshadowed lights
 */
void FTranslucentLightInjectionCollector::AddLightForInjection(
	const FViewInfo& View,
	const uint32 ViewIndex,
	TArrayView<const FVisibleLightInfo> VisibleLightInfos,
	const FLightSceneInfo& LightSceneInfo,
	const FProjectedShadowInfo* InProjectedShadowInfo)
{
	if (LightSceneInfo.Proxy->AffectsTranslucentLighting())
	{
		const uint8 LightType = LightSceneInfo.Proxy->GetLightType();

		FVolumeBounds VolumeBounds[TVC_MAX];
		bool bAnyBoundsValid = false;
		for (uint32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; ++VolumeCascadeIndex)
		{
			VolumeBounds[VolumeCascadeIndex] = CalculateLightVolumeBounds(LightSceneInfo.Proxy->GetBoundingSphere(), View, VolumeCascadeIndex, LightType == LightType_Directional);
			bAnyBoundsValid = bAnyBoundsValid || VolumeBounds[VolumeCascadeIndex].IsValid();
		}
		if (!bAnyBoundsValid)
		{
			return;
		}

		const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo.Id];
		const ERHIFeatureLevel::Type FeatureLevel = View.FeatureLevel;

		bool bLightFunctionUsesAtlas = false;
		bool bApplyLightFunction = false;
		const FMaterialRenderProxy* LightFunctionMaterialRenderProxy = LightSceneInfo.Proxy->GetLightFunctionMaterial();
		if (LightFunctionMaterialRenderProxy && View.Family->EngineShowFlags.LightFunctions)
		{
			const FMaterial& LightFunctionMaterial	= LightFunctionMaterialRenderProxy->GetIncompleteMaterialWithFallback(FeatureLevel);

			bApplyLightFunction						= LightFunctionMaterial.IsLightFunction();
			bLightFunctionUsesAtlas					= bApplyLightFunction && LightFunctionMaterial.MaterialIsLightFunctionAtlasCompatible_RenderThread();
		}

		const int32 VirtualShadowMapId = VisibleLightInfo.GetVirtualShadowMapId(&View);
		
		const bool bUseAdaptiveVolumetricShadowMap =
			LightSceneInfo.Proxy->CastsVolumetricShadow() &&
			ShouldRenderHeterogeneousVolumesForView(View) &&
			ShouldHeterogeneousVolumesCastShadows();

		const bool bStaticShadowing =
			LightSceneInfo.Proxy->GetStaticShadowDepthMap() &&
			LightSceneInfo.Proxy->GetStaticShadowDepthMap()->Data;

		// We only support the "accurate" rect light model through this path since the LightGrid stores them that way
		// Thus if the spot light approximation is used we have to send them through the unbatched path.
		const bool bSupportRectLights = CVarTranslucencyLightingVolumeAccurateRectLights.GetValueOnRenderThread() != 0;

		// Lights without certain features can be batched into a single draw (loop in shader) which is more efficient
		bool bSupportsBatching =
			bCollectorSupportsBatching &&
			LightType != LightType_Directional &&
			(bSupportRectLights || LightType != LightType_Rect) &&
			!bStaticShadowing &&
			InProjectedShadowInfo == nullptr &&
			(!bApplyLightFunction || bLightFunctionUsesAtlas) &&
			!bUseAdaptiveVolumetricShadowMap;

		// If it would otherwise be supported, see if we can find the local light index in the light grid
		if (bSupportsBatching && View.ForwardLightingResources.ForwardLightUniformBuffer && View.ViewState)
		{
			const int32* Value = View.ViewState->LightSceneIdToForwardLightIndex.Find(LightSceneInfo.Id);
			if (Value)
			{
				const int32 ForwardLightIndex = *Value;

				FPerViewData& InjectionData = InjectionDataPerView[ViewIndex];
				// There shouldn't be any duplication here
				++InjectionData.BatchedLocalLightCount;

				for (uint32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; ++VolumeCascadeIndex)
				{
					if (VolumeBounds[VolumeCascadeIndex].IsValid())
					{
						TBitArray<SceneRenderingAllocator>& Bits = InjectionData.BatchedLocalLights[VolumeCascadeIndex];
						Bits.PadToNum(ForwardLightIndex + 1, false);
						Bits[ForwardLightIndex] = true;
					}
				}

				if (VirtualShadowMapId != INDEX_NONE)
				{
					// Note if there are any batched lights with VSM for permutation selection later
					InjectionData.bAnyBatchedLightsWithVirtualShadowMaps = true;
				}
			}
			else
			{
				bSupportsBatching = false;
			}
		}
		
		if (!bSupportsBatching)
		{
			// Skip rendering if the DefaultLightFunctionMaterial isn't compiled yet
			const FMaterialRenderProxy* MaterialProxy = bApplyLightFunction ?
				LightSceneInfo.Proxy->GetLightFunctionMaterial() :
				UMaterial::GetDefaultMaterial(MD_LightFunction)->GetRenderProxy();

			if (MaterialProxy->GetIncompleteMaterialWithFallback(FeatureLevel).IsLightFunction())
			{
				FTranslucentLightInjectionCollector::FInjectionData Data;
				Data.LightSceneInfo = &LightSceneInfo;
				Data.ProjectedShadowInfo = InProjectedShadowInfo;
				Data.bApplyLightFunction = bApplyLightFunction;
				Data.LightFunctionMaterialProxy = MaterialProxy;
				Data.VirtualShadowMapId = VirtualShadowMapId;
				Data.bUseAdaptiveVolumetricShadowMap = bUseAdaptiveVolumetricShadowMap;
				for (uint32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; ++VolumeCascadeIndex)
				{
					Data.VolumeBounds[VolumeCascadeIndex] = VolumeBounds[VolumeCascadeIndex];
				}
				InjectionDataPerView[ViewIndex].Unbatched.Add(Data);
			}
		}
	}
}

static FRDGTextureRef GetSkyTransmittanceLutTexture(FRDGBuilder& GraphBuilder, const FScene* Scene, const FViewInfo& View)
{
	FRDGTextureRef TransmittanceLutTexture = nullptr;
	if (ShouldRenderSkyAtmosphere(Scene, View.Family->EngineShowFlags))
	{
		if (const FSkyAtmosphereRenderSceneInfo* SkyInfo = Scene->GetSkyAtmosphereSceneInfo())
		{
			TransmittanceLutTexture = SkyInfo->GetTransmittanceLutTexture(GraphBuilder);
		}
	}
	return TransmittanceLutTexture;
}

static void SetupPSOStateForVolumeInjection(
	TShaderMapRef<FWriteToSliceVS> VertexShader,
	TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader,
	FRHIPixelShader* PixelShaderRHI,
	bool bDirectionalLight,
	FGraphicsPipelineStateInitializer &OutGraphicsPSOInit)
{
	OutGraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	OutGraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	OutGraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

	// Accumulate the contribution of multiple lights
	if (bDirectionalLight)
	{		
		// Directional lights write their shadowing into alpha of the ambient texture
		OutGraphicsPSOInit.BlendState = TStaticBlendState<
			CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One,
		CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
	}
	else
	{
		OutGraphicsPSOInit.BlendState = TStaticBlendState<
			CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One,
		CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();
	}

	OutGraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
	OutGraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	OutGraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
	OutGraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShaderRHI;
}

static void SetPSOStateForVolumeInjection(
	TShaderMapRef<FWriteToSliceVS> VertexShader,
	TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader,
	FRHIPixelShader* PixelShaderRHI,
	FVolumeBounds VolumeBounds,
	bool bDirectionalLight,
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer &OutGraphicsPSOInit)
{
	RHICmdList.ApplyCachedRenderTargets(OutGraphicsPSOInit);

	SetupPSOStateForVolumeInjection(VertexShader, GeometryShader, PixelShaderRHI, bDirectionalLight, OutGraphicsPSOInit);

	SetGraphicsPipelineState(RHICmdList, OutGraphicsPSOInit, 0);

	const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

	SetShaderParametersLegacyVS(RHICmdList, VertexShader, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
	if (GeometryShader.IsValid())
	{
		SetShaderParametersLegacyGS(RHICmdList, GeometryShader, VolumeBounds.MinZ);
	}
}


static void InjectTranslucencyLightingVolumeBatch(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const uint32 ViewIndex,
	uint32 MaxBatchedLocalLights,
	FRDGBufferRef BatchedLocalLightsRDG,
	bool bSupportVirtualShadowMaps,
	uint32 VolumeCascadeIndex,
	FRDGTextureRef& VolumeAmbientTexture,
	FRDGTextureRef& VolumeDirectionalTexture,
	const FSceneRenderer& Renderer)
{
	check(MaxBatchedLocalLights > 0);
	check(BatchedLocalLightsRDG);

	const FIntVector VolumeSize = FIntVector(GetTranslucencyLightingVolumeDim());

	const FViewInfo::FTranslucencyVolumeMarkData& VolumeMarkData = View.TranslucencyVolumeMarkData[VolumeCascadeIndex];
	const bool bUseVolumeMarkTexture = IsTranslucencyLightingVolumeUsingVoxelMarking() && VolumeMarkData.MarkTexture != nullptr;

	EPixelFormat TranslucencyPixelFormat;
	ETextureCreateFlags TranslucencyTargetFlags;
	FTranslucencyLightingVolumeTextures::GetTextureFormatAndCreationFlags(TranslucencyPixelFormat, TranslucencyTargetFlags);

	const bool bUseLightFunctionAtlas = LightFunctionAtlas::IsEnabled(View, ELightFunctionAtlasSystem::DeferredLighting);
	const bool bUseUAVTypedLoad = UE::PixelFormat::HasCapabilities(TranslucencyPixelFormat, EPixelFormatCapabilities::TypedUAVLoad);

	auto* PassParameters = GraphBuilder.AllocParameters<FTranslucentLightingInjectBatchCS::FParameters>();
	if (bSupportVirtualShadowMaps)
	{
		PassParameters->VirtualShadowMapSamplingParameters = Renderer.VirtualShadowMapArray.GetSamplingParameters(GraphBuilder, ViewIndex);
	}
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->ForwardLightStruct = View.ForwardLightingResources.ForwardLightUniformBuffer;
	PassParameters->BatchedLocalLights = GraphBuilder.CreateSRV(BatchedLocalLightsRDG);
	PassParameters->LightFunctionAtlas = LightFunctionAtlas::BindGlobalParameters(GraphBuilder, View);;
	PassParameters->MaxBatchedLocalLights = MaxBatchedLocalLights;
	PassParameters->VolumeCascadeIndex = VolumeCascadeIndex;
	PassParameters->VolumeSize = VolumeSize;

	if (bUseUAVTypedLoad)
	{
		PassParameters->RWTranslucencyLightingVolumeAmbient = GraphBuilder.CreateUAV(VolumeAmbientTexture);
		PassParameters->RWTranslucencyLightingVolumeDirectional = GraphBuilder.CreateUAV(VolumeDirectionalTexture);
	}
	else
	{
		// need to output to auxiliary textures to be able to read from existing volumes

		FRDGTextureRef OutputVolumeAmbientTexture = GraphBuilder.CreateTexture(VolumeAmbientTexture->Desc, VolumeAmbientTexture->Name);
		FRDGTextureRef OutputVolumeDirectionalTexture = GraphBuilder.CreateTexture(VolumeDirectionalTexture->Desc, VolumeDirectionalTexture->Name);

		PassParameters->RWTranslucencyLightingVolumeAmbient = GraphBuilder.CreateUAV(OutputVolumeAmbientTexture);
		PassParameters->RWTranslucencyLightingVolumeDirectional = GraphBuilder.CreateUAV(OutputVolumeDirectionalTexture);

		PassParameters->TranslucencyLightingVolumeAmbient = VolumeAmbientTexture;
		PassParameters->TranslucencyLightingVolumeDirectional = VolumeDirectionalTexture;

		VolumeAmbientTexture = OutputVolumeAmbientTexture;
		VolumeDirectionalTexture = OutputVolumeDirectionalTexture;
	}

	FTranslucentLightingInjectBatchCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FTranslucentLightingInjectBatchCS::FVirtualShadowMap>(bSupportVirtualShadowMaps);
	PermutationVector.Set<FTranslucentLightingInjectBatchCS::FUseLightFunctionAtlas>(bUseLightFunctionAtlas);
	PermutationVector.Set<FTranslucentLightingInjectBatchCS::FIndirectVoxelDispatch>(bUseVolumeMarkTexture);
	PermutationVector.Set<FTranslucentLightingInjectBatchCS::FUseUAVTypedLoad>(bUseUAVTypedLoad);

	auto ComputeShader = View.ShaderMap->GetShader<FTranslucentLightingInjectBatchCS>(PermutationVector);

	if (bUseVolumeMarkTexture)
	{
		PassParameters->VoxelAllocator = GraphBuilder.CreateSRV(VolumeMarkData.VoxelAllocator);
		PassParameters->VoxelData = GraphBuilder.CreateSRV(VolumeMarkData.VoxelData);
		PassParameters->IndirectArgs = VolumeMarkData.VoxelIndirectArgs;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InjectTranslucencyLightingVolumeBatch(VolumeCascade=%d,Max=%d%s)",
				VolumeCascadeIndex, MaxBatchedLocalLights,
				bSupportVirtualShadowMaps ? TEXT(",VirtualShadowMap") : TEXT("")),
			ComputeShader,
			PassParameters,
			VolumeMarkData.VoxelIndirectArgs,
			0);
	}
	else
	{
		FIntVector NumGroups = FComputeShaderUtils::GetGroupCount(VolumeSize, FTranslucentLightingInjectBatchCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InjectTranslucencyLightingVolumeBatch(VolumeCascade=%d,Max=%d%s)",
				VolumeCascadeIndex, MaxBatchedLocalLights,
				bSupportVirtualShadowMaps ? TEXT(",VirtualShadowMap") : TEXT("")),
			ComputeShader,
			PassParameters,
			NumGroups);
	}
}


BEGIN_SHADER_PARAMETER_STRUCT(FInjectTranslucentLightArrayParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FTranslucentLightingInjectPS::FParameters, PS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FVolumetricCloudShadowAOParameters, CloudShadowAO)
	RDG_TEXTURE_ACCESS(TransmittanceLutTexture, ERHIAccess::SRVGraphics)
	RDG_TEXTURE_ACCESS(ShadowDepthTexture, ERHIAccess::SRVGraphics)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/** Injects all the lights in LightInjectionData into the translucent lighting volume textures. */
void InjectTranslucencyLightingVolume(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const uint32 ViewIndex,
	const FScene* Scene,
	const FSceneRenderer& Renderer,
	const FTranslucentLightInjectionCollector& Collector,
	FTranslucencyLightingVolumeTextures& Textures)
{
	if (!GUseTranslucentLightingVolumes || !RHISupportsVolumeTextureRendering(View.GetShaderPlatform()))
	{
		return;
	}

	const FTranslucentLightInjectionCollector::FPerViewData& LightInjectionData = Collector.InjectionDataPerView[ViewIndex];

	INC_DWORD_STAT_BY(STAT_NumLightsInjectedIntoTranslucencyBatched, LightInjectionData.BatchedLocalLightCount);
	INC_DWORD_STAT_BY(STAT_NumLightsInjectedIntoTranslucency, LightInjectionData.Unbatched.Num());

	const FVolumetricCloudShadowAOParameters CloudShadowAOParameters = GetCloudShadowAOParameters(GraphBuilder, View, Scene->GetVolumetricCloudSceneInfo());
	const bool bUseLightFunctionAtlas = View.LightFunctionAtlasViewData.UsesLightFunctionAtlas(LightFunctionAtlas::ELightFunctionAtlasSystem::DeferredLighting);

	FRDGTextureRef TransmittanceLutTexture = GetSkyTransmittanceLutTexture(GraphBuilder, Scene, View);

	// When accurate rect lights is disabled we approximate rect lights as spotlights
	uint32 DeferredLightParameterFlags = CVarTranslucencyLightingVolumeAccurateRectLights.GetValueOnRenderThread() != 0 ?
		0U : ELightShaderParameterFlags::RectAsSpotLight;

	// Inject into each volume cascade. Operate on one cascade at a time to reduce render target switches.
	for (uint32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
	{
		// for stereo case, using PrimaryViewIndex essentially shares the lighting volume textures
		const uint32 TextureIndex = Textures.GetIndex(View, VolumeCascadeIndex);
		FRDGTextureRef VolumeAmbientTexture = Textures.Ambient[TextureIndex];
		FRDGTextureRef VolumeDirectionalTexture = Textures.Directional[TextureIndex];

		// Batched lights
		{
			TBitArray<SceneRenderingAllocator>& BatchedLocalLights = Collector.InjectionDataPerView[ViewIndex].BatchedLocalLights[VolumeCascadeIndex];
			if (BatchedLocalLights.Num() > 0)
			{
				const uint32 NumUint32Elements = FMath::DivideAndRoundUp(BatchedLocalLights.Num(), 32);
				const uint32 InitialDataSize = NumUint32Elements * sizeof(uint32);
				BatchedLocalLights.PadToNum(NumUint32Elements * 32, false);

				FRDGBufferRef BatchedLocalLightsRDG = CreateStructuredBuffer(
					GraphBuilder,
					TEXT("TranslucencyLightingVolume.BatchedLocalLights"),
					sizeof(uint32),
					NumUint32Elements,
					BatchedLocalLights.GetData(),
					InitialDataSize
				);

				InjectTranslucencyLightingVolumeBatch(GraphBuilder, View, ViewIndex,
					BatchedLocalLights.Num(), BatchedLocalLightsRDG,
					LightInjectionData.bAnyBatchedLightsWithVirtualShadowMaps,
					VolumeCascadeIndex, VolumeAmbientTexture, VolumeDirectionalTexture,
					Renderer);
			}
		}

		// Unbatched lights
		for (int32 LightIndex = 0; LightIndex < LightInjectionData.Unbatched.Num(); LightIndex++)
		{
			const FTranslucentLightInjectionCollector::FInjectionData& InjectionData = LightInjectionData.Unbatched[LightIndex];
			const FLightSceneInfo* const LightSceneInfo = InjectionData.LightSceneInfo;
			const FVisibleLightInfo& VisibleLightInfo = Renderer.VisibleLightInfos[LightSceneInfo->Id];
			const bool bInverseSquared = LightSceneInfo->Proxy->IsInverseSquared();
			const bool bDirectionalLight = LightSceneInfo->Proxy->GetLightType() == LightType_Directional;

			const FVolumeBounds VolumeBounds = InjectionData.VolumeBounds[VolumeCascadeIndex];
			if (VolumeBounds.IsValid())
			{
				TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
				TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);

				FRDGTextureRef ShadowDepthTexture = nullptr;

				if (InjectionData.ProjectedShadowInfo)
				{
					ShadowDepthTexture = TryRegisterExternalTexture(GraphBuilder, InjectionData.ProjectedShadowInfo->RenderTargets.DepthTarget);
				}

				auto* PassParameters = GraphBuilder.AllocParameters< FInjectTranslucentLightArrayParameters >();
				PassParameters->TransmittanceLutTexture = TransmittanceLutTexture;
				PassParameters->ShadowDepthTexture = ShadowDepthTexture;
				PassParameters->CloudShadowAO = CloudShadowAOParameters;
				PassParameters->PS.VirtualShadowMapSamplingParameters = Renderer.VirtualShadowMapArray.GetSamplingParameters(GraphBuilder, ViewIndex);
				PassParameters->RenderTargets[0] = FRenderTargetBinding(VolumeAmbientTexture, ERenderTargetLoadAction::ELoad);
				PassParameters->RenderTargets[1] = FRenderTargetBinding(VolumeDirectionalTexture, ERenderTargetLoadAction::ELoad);

				PassParameters->PS.ViewUniformBuffer = View.ViewUniformBuffer;

				FDeferredLightUniformStruct* DeferredLightStruct = GraphBuilder.AllocParameters<FDeferredLightUniformStruct>();
				*DeferredLightStruct = GetDeferredLightParameters(View, *LightSceneInfo, bUseLightFunctionAtlas, DeferredLightParameterFlags);
				PassParameters->PS.DeferredLight = GraphBuilder.CreateUniformBuffer(DeferredLightStruct);

				GetVolumeShadowingShaderParameters(GraphBuilder, View, LightSceneInfo, InjectionData.ProjectedShadowInfo, PassParameters->PS.VolumeShadowingParameters);

				PassParameters->PS.VirtualShadowMapId = InjectionData.VirtualShadowMapId;
				PassParameters->PS.LightFunctionParameters = FLightFunctionSharedParameters::GetLightFunctionSharedParameters(LightSceneInfo, 1.0f);
				PassParameters->PS.VolumeCascadeIndex = VolumeCascadeIndex;
				PassParameters->PS.AVSM = HeterogeneousVolumes::GetAdaptiveVolumetricShadowMapUniformBuffer(GraphBuilder, View.ViewState, LightSceneInfo);

				bool bIsSpotlight = LightSceneInfo->Proxy->GetLightType() == LightType_Spot;
				PassParameters->PS.SpotlightMask = bIsSpotlight ? 1.0f : 0.0f; //@todo - needs to be a permutation to reduce shadow filtering work

				{
					const FVector Scale = LightSceneInfo->Proxy->GetLightFunctionScale();
					// Switch x and z so that z of the user specified scale affects the distance along the light direction
					const FVector InverseScale = FVector(1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X);
					const FMatrix WorldToLight = LightSceneInfo->Proxy->GetWorldToLight() * FScaleMatrix(InverseScale);
					const FMatrix TranslatedWorldToWorld = FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation());

					PassParameters->PS.LightFunctionTranslatedWorldToLight = FMatrix44f(TranslatedWorldToWorld * WorldToLight);
				}

				const bool bCloudShadowEnabled = SetupLightCloudTransmittanceParameters(GraphBuilder, Scene, View, LightSceneInfo, PassParameters->PS.LightCloudTransmittanceParameters);
				PassParameters->PS.VolumetricCloudShadowEnabled = bCloudShadowEnabled ? 1 : 0;

				PassParameters->PS.AtmospherePerPixelTransmittanceEnabled = IsLightAtmospherePerPixelTransmittanceEnabled(Scene, View, LightSceneInfo);

				PassParameters->PS.CameraRelativeLightPosition = GetCamRelativeLightPosition(View.ViewMatrices, *LightSceneInfo);

			#if 0 // Enable this to be able to debug using DEBUG_ONE_VOXEL
				ShaderPrint::SetEnabled(true);
				ShaderPrint::RequestSpaceForLines(64u);
				ShaderPrint::RequestSpaceForCharacters(64u);
				ShaderPrint::RequestSpaceForTriangles(64u);
				ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->PS.ShaderPrint);
			#endif

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("InjectTranslucencyLightingVolume(VolumeCascade=%d%s%s%s)",
						VolumeCascadeIndex,
						InjectionData.VirtualShadowMapId != INDEX_NONE ? TEXT(",VirtualShadowMap") : TEXT(""),
						InjectionData.ProjectedShadowInfo != nullptr ? TEXT(",ShadowMap") : TEXT(""),
						InjectionData.bApplyLightFunction ? TEXT(",LightFunction") : TEXT("")),
					PassParameters,
					ERDGPassFlags::Raster,
					[PassParameters, VertexShader, GeometryShader, &View, &Renderer, &InjectionData, LightSceneInfo, bDirectionalLight, VolumeBounds, VolumeCascadeIndex](FRDGAsyncTask, FRHICommandList& RHICmdList)
				{
					FGraphicsPipelineStateInitializer GraphicsPSOInit;

					const FMaterialRenderProxy* MaterialProxy = InjectionData.LightFunctionMaterialProxy;
					const FMaterial& Material = MaterialProxy->GetMaterialWithFallback( View.GetFeatureLevel(), MaterialProxy );
					const FMaterialShaderMap* MaterialShaderMap = Material.GetRenderingThreadShaderMap();

					const bool bUseVSM = InjectionData.VirtualShadowMapId != INDEX_NONE;
					bool bDynamicShadow = InjectionData.ProjectedShadowInfo != nullptr;

					if (CVarTranslucencyLightingVolumeInjectDirectionalLightCSM.GetValueOnRenderThread() <= 0 && bDirectionalLight)
					{
						bDynamicShadow = false;
					}

					FTranslucentLightingInjectPS::FPermutationDomain PermutationVector;
					PermutationVector.Set< FTranslucentLightingInjectPS::FRadialAttenuation >(!bDirectionalLight);
					PermutationVector.Set< FTranslucentLightingInjectPS::FDynamicallyShadowed >(bDynamicShadow);
					PermutationVector.Set< FTranslucentLightingInjectPS::FLightFunction >(InjectionData.bApplyLightFunction);
					PermutationVector.Set< FTranslucentLightingInjectPS::FVirtualShadowMap >(bUseVSM);
					PermutationVector.Set< FTranslucentLightingInjectPS::FAdaptiveVolumetricShadowMap >(InjectionData.bUseAdaptiveVolumetricShadowMap);

					auto PixelShader = MaterialShaderMap->GetShader< FTranslucentLightingInjectPS >( PermutationVector );

					SetPSOStateForVolumeInjection(VertexShader, GeometryShader, PixelShader.GetPixelShader(),
						VolumeBounds, bDirectionalLight, RHICmdList, GraphicsPSOInit);

#if PSO_PRECACHING_VALIDATE
					if (PSOCollectorStats::IsFullPrecachingValidationEnabled())
					{
						static const int32 MaterialPSOCollectorIndex = FPSOCollectorCreateManager::GetIndex(GetFeatureLevelShadingPath(GMaxRHIFeatureLevel), TranslucentLightingMaterialPSOCollectorName);
						PSOCollectorStats::CheckFullPipelineStateInCache(GraphicsPSOInit, EPSOPrecacheResult::Unknown, MaterialProxy, nullptr, nullptr, MaterialPSOCollectorIndex);
					}
#endif // PSO_PRECACHING_VALIDATE

					SetShaderParametersLegacyPS(RHICmdList, PixelShader, View, InjectionData.LightFunctionMaterialProxy);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

					RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
				});
			}
		}

		Textures.Ambient[TextureIndex] = VolumeAmbientTexture;
		Textures.Directional[TextureIndex] = VolumeDirectionalTexture;
	}

	GraphBuilder.FlushSetupQueue();
}

class FSimpleLightTranslucentLightingInjectPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSimpleLightTranslucentLightingInjectPS);
	SHADER_USE_PARAMETER_STRUCT(FSimpleLightTranslucentLightingInjectPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FVector4f, SimpleLightPositionAndRadius)
		SHADER_PARAMETER(FVector4f, SimpleLightColorAndExponent)
		SHADER_PARAMETER(uint32, VolumeCascadeIndex)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && (RHISupportsGeometryShaders(Parameters.Platform) || RHISupportsVertexShaderLayer(Parameters.Platform));
	}
};

IMPLEMENT_GLOBAL_SHADER(FSimpleLightTranslucentLightingInjectPS, "/Engine/Private/TranslucentLightInjectionShaders.usf", "SimpleLightInjectMainPS", SF_Pixel);

void InjectSimpleTranslucencyLightingVolumeArray(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const uint32 ViewIndex,
	const uint32 ViewCount,
	const FTranslucencyLightingVolumeTextures& Textures,
	const FSimpleLightArray& SimpleLights)
{
	int32 NumLightsToInject = 0;

	for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); LightIndex++)
	{
		if (SimpleLights.InstanceData[LightIndex].bAffectTranslucency)
		{
			NumLightsToInject++;
		}
	}

	if (NumLightsToInject > 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "InjectSimpleTranslucentLightArray");

		INC_DWORD_STAT_BY(STAT_NumLightsInjectedIntoTranslucency, NumLightsToInject);

		const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

		const float Exposure = View.GetLastEyeAdaptationExposure();

		// Inject into each volume cascade
		// Operate on one cascade at a time to reduce render target switches
		for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
		{
			const uint32 TextureIndex = Textures.GetIndex(View, VolumeCascadeIndex);

			RDG_EVENT_SCOPE(GraphBuilder, "Cascade%d", VolumeCascadeIndex);
			FRDGTextureRef VolumeAmbientTexture = Textures.Ambient[TextureIndex];
			FRDGTextureRef VolumeDirectionalTexture = Textures.Directional[TextureIndex];

			for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); LightIndex++)
			{
				const FSimpleLightEntry& SimpleLight = SimpleLights.InstanceData[LightIndex];
				const FSimpleLightPerViewEntry& SimpleLightPerViewData = SimpleLights.GetViewDependentData(LightIndex, ViewIndex, ViewCount);

				if (SimpleLight.bAffectTranslucency)
				{
					const FSphere LightBounds(SimpleLightPerViewData.Position, SimpleLight.Radius);
					const FVolumeBounds VolumeBounds = CalculateLightVolumeBounds(LightBounds, View, VolumeCascadeIndex, false);

					if (VolumeBounds.IsValid())
					{
						const FVector3f TranslatedLightPosition = FVector3f(SimpleLightPerViewData.Position + View.ViewMatrices.GetPreViewTranslation());

						auto* PassParameters = GraphBuilder.AllocParameters<FSimpleLightTranslucentLightingInjectPS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->VolumeCascadeIndex = VolumeCascadeIndex;
						PassParameters->SimpleLightPositionAndRadius = FVector4f(TranslatedLightPosition, SimpleLight.Radius);
						PassParameters->SimpleLightColorAndExponent = FVector4f((FVector3f)SimpleLight.Color * FLightRenderParameters::GetLightExposureScale(Exposure, SimpleLight.InverseExposureBlend), SimpleLight.Exponent);
						PassParameters->RenderTargets[0] = FRenderTargetBinding(VolumeAmbientTexture, ERenderTargetLoadAction::ELoad);
						PassParameters->RenderTargets[1] = FRenderTargetBinding(VolumeDirectionalTexture, ERenderTargetLoadAction::ELoad);

						TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
						TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);
						TShaderMapRef<FSimpleLightTranslucentLightingInjectPS> PixelShader(View.ShaderMap);

						GraphBuilder.AddPass(
							{},
							PassParameters,
							ERDGPassFlags::Raster,
							[VertexShader, GeometryShader, PixelShader, PassParameters, VolumeBounds, TranslucencyLightingVolumeDim](FRDGAsyncTask, FRHICommandList& RHICmdList)
						{
							FGraphicsPipelineStateInitializer GraphicsPSOInit;
							
							SetPSOStateForVolumeInjection(VertexShader, GeometryShader, PixelShader.GetPixelShader(),
								VolumeBounds, false, RHICmdList, GraphicsPSOInit);

							SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
							RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
						});
					}
				}
			}
		}
	}
}

class FDebugTranslucencyLightingVolumeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDebugTranslucencyLightingVolumeCS)
	SHADER_USE_PARAMETER_STRUCT(FDebugTranslucencyLightingVolumeCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, InnerVolumeMarkTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, OuterVolumeMarkTexture)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static int32 GetGroupSize()
	{
		return 4;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return EShaderPermutationPrecacheRequest::NotPrecached;
	}
};

IMPLEMENT_GLOBAL_SHADER(FDebugTranslucencyLightingVolumeCS, "/Engine/Private/TranslucentLightingShaders.usf", "DebugTranslucencyLightingVolumeCS", SF_Compute);

static void DrawDebugTranslucencyLightingVolume(
	FRDGBuilder& GraphBuilder,
	const TArrayView<const FViewInfo> Views)
{
	// Draw debug the translucency volume lighting buffer
	if (GTranslucencyLightingVolumeDebug <= 0)
	{
		return;
	}

	const FIntVector TranslucencyLightingVolumeDim(GetTranslucencyLightingVolumeDim());

	FRDGTextureRef DummyTexture = GSystemTextures.GetVolumetricBlackUintDummy(GraphBuilder);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
		RDG_EVENT_SCOPE(GraphBuilder, "View%d", ViewIndex);

		FDebugTranslucencyLightingVolumeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDebugTranslucencyLightingVolumeCS::FParameters>();
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->InnerVolumeMarkTexture = View.TranslucencyVolumeMarkData[0].MarkTexture ? View.TranslucencyVolumeMarkData[0].MarkTexture : DummyTexture;
		PassParameters->OuterVolumeMarkTexture = View.TranslucencyVolumeMarkData[1].MarkTexture ? View.TranslucencyVolumeMarkData[1].MarkTexture : DummyTexture;

		ShaderPrint::SetEnabled(true);
		ShaderPrint::RequestSpaceForLines(128u);
		ShaderPrint::RequestSpaceForCharacters(128u);
		ShaderPrint::RequestSpaceForTriangles(64u);
		ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);

		FDebugTranslucencyLightingVolumeCS::FPermutationDomain PermutationVector;
		auto ComputeShader = View.ShaderMap->GetShader<FDebugTranslucencyLightingVolumeCS>(PermutationVector);
		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(TranslucencyLightingVolumeDim, FDebugTranslucencyLightingVolumeCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("DebugTranslucencyLightingVolumeCS"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}
}

void FilterTranslucencyLightingVolume(
	FRDGBuilder& GraphBuilder,
	const TArrayView<const FViewInfo> Views,
	FTranslucencyLightingVolumeTextures& Textures)
{
	// Draw debug information if needed.
	DrawDebugTranslucencyLightingVolume(GraphBuilder, Views);

	const bool bMegaLightsTranslucencyVolume = Views.Num() > 0 && MegaLights::IsEnabled(*Views[0].Family) && MegaLights::UseTranslucencyVolume();

	const bool bTemporal = CVarTranslucencyLightingVolumeTemporal.GetValueOnRenderThread() || bMegaLightsTranslucencyVolume;

	if (!GUseTranslucentLightingVolumes || Views.Num() == 0 || !RHISupportsVolumeTextureRendering(Views[0].GetShaderPlatform()) || (!GUseTranslucencyVolumeBlur && !bTemporal))
	{
		// discard history since we are not updating it this frame

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
			{
				FTranslucencyLightingViewState& TranslucencyLightingViewState = View.ViewState->TranslucencyLighting;
				for (int32 Index = 0; Index < TVC_MAX; Index++)
				{
					TranslucencyLightingViewState.HistoryAmbient[Index] = nullptr;
					TranslucencyLightingViewState.HistoryDirectional[Index] = nullptr;
					
					TranslucencyLightingViewState.HistoryMark[Index] = nullptr;
				}
			}
		}

		return;
	}

	FRHISamplerState* SamplerStateRHI = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();
	RDG_EVENT_SCOPE_STAT(GraphBuilder, TranslucentLighting, "FilterTranslucentVolume %dx%dx%d Cascades:%d", TranslucencyLightingVolumeDim, TranslucencyLightingVolumeDim, TranslucencyLightingVolumeDim, TVC_MAX);
	RDG_GPU_STAT_SCOPE(GraphBuilder, TranslucentLighting);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View %d", ViewIndex);

		for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
		{
			bool bHasValidHistory = false;

			FRDGTextureRef HistoryAmbient;
			FRDGTextureRef HistoryDirectional;
			FRDGTextureRef HistoryMark;

			FVector4f PrevTranslucencyLightingVolumeMin;
			FVector4f PrevTranslucencyLightingVolumeInvSize;

			if (bTemporal && View.ViewState && !View.bCameraCut && !View.bPrevTransformsReset)
			{
				FTranslucencyLightingViewState& TranslucencyLightingViewState = View.ViewState->TranslucencyLighting;

				if (TranslucencyLightingViewState.HistoryAmbient[VolumeCascadeIndex] && TranslucencyLightingViewState.HistoryDirectional[VolumeCascadeIndex])
				{
					HistoryAmbient = GraphBuilder.RegisterExternalTexture(TranslucencyLightingViewState.HistoryAmbient[VolumeCascadeIndex]);
					HistoryDirectional = GraphBuilder.RegisterExternalTexture(TranslucencyLightingViewState.HistoryDirectional[VolumeCascadeIndex]);

					bHasValidHistory = true;
				}
				else
				{
					const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
					HistoryAmbient = SystemTextures.VolumetricBlack;
					HistoryDirectional = SystemTextures.VolumetricBlack;
				}
				
				if (TranslucencyLightingViewState.HistoryMark[VolumeCascadeIndex])
				{
					HistoryMark = GraphBuilder.RegisterExternalTexture(TranslucencyLightingViewState.HistoryMark[VolumeCascadeIndex]);
				}
				else
				{
					HistoryMark = nullptr;
				}

				{
					const FVector VolumeWorldMin = TranslucencyLightingViewState.HistoryVolumeMin[VolumeCascadeIndex];
					const float VolumeVoxelSize = TranslucencyLightingViewState.HistoryVoxelSize[VolumeCascadeIndex];
					const FVector3f VolumeSize = FVector3f(TranslucencyLightingViewState.HistoryVolumeSize[VolumeCascadeIndex]);

					const FVector3f VolumeTranslatedWorldMin = FVector3f(VolumeWorldMin + View.ViewMatrices.GetPreViewTranslation());

					PrevTranslucencyLightingVolumeMin = FVector4f(VolumeTranslatedWorldMin, 1.0f / TranslucencyLightingVolumeDim);
					PrevTranslucencyLightingVolumeInvSize = FVector4f(FVector3f(1.0f) / VolumeSize, VolumeVoxelSize);
				}
			}
			else
			{
				const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
				HistoryAmbient = SystemTextures.VolumetricBlack;
				HistoryDirectional = SystemTextures.VolumetricBlack;
				HistoryMark = nullptr;

				PrevTranslucencyLightingVolumeMin = FVector4f::Zero();
				PrevTranslucencyLightingVolumeInvSize = FVector4f::Zero();
			}

			const uint32 TextureIndex = Textures.GetIndex(View, VolumeCascadeIndex);

			if (!bTemporal || bHasValidHistory)
			{
				FRDGTextureRef InputVolumeAmbientTexture = Textures.Ambient[TextureIndex];
				FRDGTextureRef InputVolumeDirectionalTexture = Textures.Directional[TextureIndex];

				FRDGTextureRef OutputVolumeAmbientTexture = GraphBuilder.CreateTexture(InputVolumeAmbientTexture->Desc, InputVolumeAmbientTexture->Name);
				FRDGTextureRef OutputVolumeDirectionalTexture = GraphBuilder.CreateTexture(InputVolumeDirectionalTexture->Desc, InputVolumeDirectionalTexture->Name);

				Textures.Ambient[TextureIndex] = OutputVolumeAmbientTexture;
				Textures.Directional[TextureIndex] = OutputVolumeDirectionalTexture;

				const FIntVector VolumeSize = FIntVector(TranslucencyLightingVolumeDim);

				auto* PassParameters = GraphBuilder.AllocParameters<FFilterTranslucentVolumeCS::FParameters>();
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->VolumeSize = VolumeSize;
				PassParameters->TexelSize = 1.0f / TranslucencyLightingVolumeDim;
				PassParameters->VolumeCascadeIndex = VolumeCascadeIndex;
				PassParameters->TranslucencyLightingVolumeAmbient = InputVolumeAmbientTexture;
				PassParameters->TranslucencyLightingVolumeDirectional = InputVolumeDirectionalTexture;
				PassParameters->TranslucencyLightingVolumeAmbientSampler = SamplerStateRHI;
				PassParameters->TranslucencyLightingVolumeDirectionalSampler = SamplerStateRHI;
				PassParameters->RWTranslucencyLightingVolumeAmbient = GraphBuilder.CreateUAV(OutputVolumeAmbientTexture);
				PassParameters->RWTranslucencyLightingVolumeDirectional = GraphBuilder.CreateUAV(OutputVolumeDirectionalTexture);

				PassParameters->PrevTranslucencyLightingVolumeMin = PrevTranslucencyLightingVolumeMin;
				PassParameters->PrevTranslucencyLightingVolumeInvSize = PrevTranslucencyLightingVolumeInvSize;

				PassParameters->HistoryTextureBilinearUVMin = FVector3f(0.5f / TranslucencyLightingVolumeDim);
				PassParameters->HistoryTextureBilinearUVMax = FVector3f((TranslucencyLightingVolumeDim - 0.5f) / TranslucencyLightingVolumeDim);

				PassParameters->HistoryAmbient = HistoryAmbient;
				PassParameters->HistoryDirectional = HistoryDirectional;
				PassParameters->HistoryAmbientSampler = SamplerStateRHI;
				PassParameters->HistoryDirectionalSampler = SamplerStateRHI;

				PassParameters->HistoryMark = HistoryMark;

				PassParameters->HistoryWeight = CVarTranslucencyLightingVolumeHistoryWeight.GetValueOnRenderThread();

				FFilterTranslucentVolumeCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FFilterTranslucentVolumeCS::FUseTemporalReprojection>(bTemporal);
				PermutationVector.Set<FFilterTranslucentVolumeCS::FCheckHistoryMark>(HistoryMark != nullptr);

				auto ComputeShader = View.ShaderMap->GetShader<FFilterTranslucentVolumeCS>(PermutationVector);

				FIntVector NumGroups = FComputeShaderUtils::GetGroupCount(VolumeSize, FFilterTranslucentVolumeCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Cascade %d", VolumeCascadeIndex),
					ComputeShader,
					PassParameters,
					NumGroups);
			}

			if (bTemporal && View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
			{
				FTranslucencyLightingViewState& TranslucencyLightingViewState = View.ViewState->TranslucencyLighting;

				GraphBuilder.QueueTextureExtraction(Textures.Ambient[TextureIndex], &TranslucencyLightingViewState.HistoryAmbient[VolumeCascadeIndex]);
				GraphBuilder.QueueTextureExtraction(Textures.Directional[TextureIndex], &TranslucencyLightingViewState.HistoryDirectional[VolumeCascadeIndex]);

				if (View.TranslucencyVolumeMarkData[TextureIndex].MarkTexture)
				{
					GraphBuilder.QueueTextureExtraction(View.TranslucencyVolumeMarkData[TextureIndex].MarkTexture, &TranslucencyLightingViewState.HistoryMark[VolumeCascadeIndex]);
				}
				else
				{
					TranslucencyLightingViewState.HistoryMark[VolumeCascadeIndex] = nullptr;
				}

				TranslucencyLightingViewState.HistoryVolumeMin[VolumeCascadeIndex] = View.TranslucencyLightingVolumeMin[VolumeCascadeIndex];
				TranslucencyLightingViewState.HistoryVoxelSize[VolumeCascadeIndex] = View.TranslucencyVolumeVoxelSize[VolumeCascadeIndex];
				TranslucencyLightingViewState.HistoryVolumeSize[VolumeCascadeIndex] = View.TranslucencyLightingVolumeSize[VolumeCascadeIndex];
			}
		}
	}
}

static void SplitSimpleLightsByView(TConstArrayView<FViewInfo> Views, const FSimpleLightArray& SimpleLights, TArrayView<FSimpleLightArray> OutSimpleLightsByView)
{
	check(OutSimpleLightsByView.Num() == Views.Num());

	for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); ++LightIndex)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			FSimpleLightPerViewEntry PerViewEntry = SimpleLights.GetViewDependentData(LightIndex, ViewIndex, Views.Num());
			OutSimpleLightsByView[ViewIndex].InstanceData.Add(SimpleLights.InstanceData[LightIndex]);
			OutSimpleLightsByView[ViewIndex].PerViewData.Add(PerViewEntry);
		}
	}
}

void FDeferredShadingSceneRenderer::GatherTranslucencyVolumeMarkedVoxels(FRDGBuilder& GraphBuilder)
{
	if (!IsTranslucencyLightingVolumeUsingVoxelMarking() || !ViewFamily.EngineShowFlags.DirectLighting || !RHISupportsVolumeTextureRendering(ViewFamily.GetShaderPlatform()))
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "TranslucencyLightingVolumeGatherMarkedVoxels");

	const FIntVector VolumeSize = FIntVector(GetTranslucencyLightingVolumeDim());

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		for (uint32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
		{
			FViewInfo::FTranslucencyVolumeMarkData& VolumeMarkData = View.TranslucencyVolumeMarkData[VolumeCascadeIndex];

			const bool bUseVolumeMarkTexture = VolumeMarkData.MarkTexture != nullptr;

			if (VolumeMarkData.MarkTexture == nullptr)
			{
				continue;
			}

			VolumeMarkData.VoxelAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("TranslucencyLightingVolume.VoxelAllocator"));
			VolumeMarkData.VoxelData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), VolumeSize.X * VolumeSize.Y * VolumeSize.Z), TEXT("TranslucencyLightingVolume.VoxelData"));
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(VolumeMarkData.VoxelAllocator), 0);

			// TODO: when using spatial blur need to dilate marked voxels

			// gather marked voxels
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FGatherMarkedVoxelsCS::FParameters>();
				PassParameters->RWVoxelAllocator = GraphBuilder.CreateUAV(VolumeMarkData.VoxelAllocator);
				PassParameters->RWVoxelData = GraphBuilder.CreateUAV(VolumeMarkData.VoxelData);
				PassParameters->VolumeMarkTexture = View.TranslucencyVolumeMarkData[VolumeCascadeIndex].MarkTexture;
				PassParameters->VolumeSize = VolumeSize;

				FGatherMarkedVoxelsCS::FPermutationDomain PermutationVector;

				auto ComputeShader = View.ShaderMap->GetShader<FGatherMarkedVoxelsCS>(PermutationVector);

				FIntVector NumGroups = FComputeShaderUtils::GetGroupCount(VolumeSize, FGatherMarkedVoxelsCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("GatherMarkedVoxels(VolumeCascade=%d)"),
					ComputeShader,
					PassParameters,
					NumGroups);
			}

			VolumeMarkData.VoxelIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("TranslucencyLightingVolume.VoxelIndirectArgs"));

			// setup indirect args
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FInitIndirectArgsCS::FParameters>();
				PassParameters->RWIndirectArgs = GraphBuilder.CreateUAV(VolumeMarkData.VoxelIndirectArgs);
				PassParameters->VoxelAllocator = GraphBuilder.CreateSRV(VolumeMarkData.VoxelAllocator);

				FInitIndirectArgsCS::FPermutationDomain PermutationVector;

				auto ComputeShader = View.ShaderMap->GetShader<FInitIndirectArgsCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("InitIndirectArgs(VolumeCascade=%d)"),
					ComputeShader,
					PassParameters,
					FIntVector(1, 1, 1));
			}
		}
	}
}

void FDeferredShadingSceneRenderer::RenderTranslucencyLightingVolume(FRDGBuilder& GraphBuilder, FTranslucencyLightingVolumeTextures& Textures, const FSortedLightSetSceneInfo& SortedLightSet)
{
	if (!GUseTranslucentLightingVolumes || !ViewFamily.EngineShowFlags.DirectLighting || !RHISupportsVolumeTextureRendering(ViewFamily.GetShaderPlatform()))
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "TranslucencyLightingVolume");
	SCOPE_CYCLE_COUNTER(STAT_TranslucentInjectTime);

	const TArray<FSortedLightSceneInfo, SceneRenderingAllocator>& SortedLights = SortedLightSet.SortedLights;
	const FSimpleLightArray& SimpleLights = SortedLightSet.SimpleLights;
	const int32 SimpleLightsEnd = SortedLightSet.SimpleLightsEnd;

	const bool bMegaLightsTranslucencyVolume = MegaLights::IsEnabled(ViewFamily) && MegaLights::UseTranslucencyVolume();

	if (SimpleLights.InstanceData.Num() > 0)
	{
		auto& SimpleLightsByView = *GraphBuilder.AllocObject<TArray<FSimpleLightArray, SceneRenderingAllocator>>();
		SimpleLightsByView.SetNum(Views.Num());

		SplitSimpleLightsByView(Views, SimpleLights, SimpleLightsByView);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			FSimpleLightArray& SimpleLightArray = SimpleLightsByView[ViewIndex];

			if (SimpleLightArray.InstanceData.Num() > 0)
			{
				const FViewInfo& View = Views[ViewIndex];
				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
				RDG_EVENT_SCOPE(GraphBuilder, "InjectSimpleLightsTranslucentLighting");
				InjectSimpleTranslucencyLightingVolumeArray(GraphBuilder, View, ViewIndex, Views.Num(), Textures, SimpleLightArray);
			}
		}
	}

	// Shadowed and light function lights
	{
		FTranslucentLightInjectionCollector Collector(GraphBuilder, Views, bAreLightsInLightGrid);

		// Collect all the light injection data
		for (int32 LightIndex = SimpleLightsEnd; LightIndex < SortedLights.Num(); LightIndex++)
		{
			const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
			const FLightSceneInfo& LightSceneInfo = *SortedLightInfo.LightSceneInfo;
			const FLightSceneProxy& LightSceneProxy = *LightSceneInfo.Proxy;

			if (bMegaLightsTranslucencyVolume && SortedLightInfo.SortKey.Fields.bHandledByMegaLights)
			{
				continue;
			}

			const bool bDrawShadows = SortedLightInfo.SortKey.Fields.bShadowed;
			const FLightOcclusionType OcclusionType = GetLightOcclusionType(LightSceneProxy, ViewFamily);
			const bool bSupportShadowMaps = bDrawShadows && OcclusionType == FLightOcclusionType::Shadowmap;

			// Collect all the light injection data
			CollectLightForTranslucencyLightingVolumeInjection(&LightSceneInfo, bSupportShadowMaps, Collector);
		}

		// Run light injection for each view
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];
			RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
			RDG_EVENT_SCOPE(GraphBuilder, "InjectTranslucencyLightingVolume(View=%d)", ViewIndex);
			InjectTranslucencyLightingVolume(GraphBuilder, View, ViewIndex, Scene, *this, Collector, Textures);
		}
	}

	InjectTranslucencyLightingVolumeAmbientCubemap(GraphBuilder, Views, Textures);
	if (SortedLightSet.MegaLightsLightStart < SortedLightSet.SortedLights.Num())
	{
		InjectTranslucencyLightingVolumeMegaLights(GraphBuilder, Views, Textures);
	}
	FilterTranslucencyLightingVolume(GraphBuilder, Views, Textures);
}

class FTranslucentLightingMaterialPSOCollector : public IPSOCollector
{
public:
	FTranslucentLightingMaterialPSOCollector(ERHIFeatureLevel::Type InFeatureLevel) : 
		IPSOCollector(FPSOCollectorCreateManager::GetIndex(GetFeatureLevelShadingPath(InFeatureLevel), TranslucentLightingMaterialPSOCollectorName)),
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

void FTranslucentLightingMaterialPSOCollector::CollectPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& Material,
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FPSOPrecacheParams& PreCacheParams,
	TArray<FPSOPrecacheData>& PSOInitializers
)
{
	if (!Material.IsLightFunction() || GTranslucencyLightingVolumeMaterialPSOPrecache == 0)
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FWriteToSliceVS> VertexShader(GlobalShaderMap);
	TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(GlobalShaderMap);
	const FMaterialShaderMap* MaterialShaderMap = Material.GetGameThreadShaderMap();
	
	EPixelFormat TranslucencyPixelFormat;
	ETextureCreateFlags TranslucencyTargetFlags;
	FTranslucencyLightingVolumeTextures::GetTextureFormatAndCreationFlags(TranslucencyPixelFormat, TranslucencyTargetFlags);

	auto AddPSOInitializer = [&](bool bDirectionalLight, bool bDynamicShadow, bool bApplyLightFunction, bool bUseVSM, bool bUseAdaptiveVolumetricShadowMap)
		{
			FTranslucentLightingInjectPS::FPermutationDomain PermutationVector;
			PermutationVector.Set< FTranslucentLightingInjectPS::FRadialAttenuation >(!bDirectionalLight);
			PermutationVector.Set< FTranslucentLightingInjectPS::FDynamicallyShadowed >(bDynamicShadow);
			PermutationVector.Set< FTranslucentLightingInjectPS::FLightFunction >(bApplyLightFunction);
			PermutationVector.Set< FTranslucentLightingInjectPS::FVirtualShadowMap >(bUseVSM);
			PermutationVector.Set< FTranslucentLightingInjectPS::FAdaptiveVolumetricShadowMap >(bUseAdaptiveVolumetricShadowMap);

			auto PixelShader = MaterialShaderMap->GetShader< FTranslucentLightingInjectPS >(PermutationVector);
			
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			SetupPSOStateForVolumeInjection(VertexShader, GeometryShader, PixelShader.GetPixelShader(), bDirectionalLight, GraphicsPSOInit);
			
			// What render target formats to support?
			FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
			RenderTargetsInfo.NumSamples = 1;
			AddRenderTargetInfo(TranslucencyPixelFormat, TranslucencyTargetFlags, RenderTargetsInfo);
			AddRenderTargetInfo(TranslucencyPixelFormat, TranslucencyTargetFlags, RenderTargetsInfo);

			GraphicsPSOInit.StatePrecachePSOHash = RHIComputeStatePrecachePSOHash(GraphicsPSOInit);
			ApplyTargetsInfo(GraphicsPSOInit, RenderTargetsInfo);

			FPSOPrecacheData PSOPrecacheData;
			PSOPrecacheData.bRequired = true;
			PSOPrecacheData.Type = FPSOPrecacheData::EType::Graphics;
			PSOPrecacheData.GraphicsPSOInitializer = GraphicsPSOInit;
#if PSO_PRECACHING_VALIDATE
			PSOPrecacheData.PSOCollectorIndex = PSOCollectorIndex;
			PSOPrecacheData.VertexFactoryType = nullptr;
#endif // PSO_PRECACHING_VALIDATE

			PSOInitializers.Add(MoveTemp(PSOPrecacheData));
		};
	
	// Generate PSOs for all possible permutations - we don't have that many light function materials
	for (int32 DirectionalLightIndex = 0; DirectionalLightIndex < 2; ++DirectionalLightIndex)
	{
		bool bDirectionalLight = DirectionalLightIndex > 0;
		for (int32 DynamicShadowIndex = 0; DynamicShadowIndex < 2; ++DynamicShadowIndex)
		{
			bool bDynamicShadow = DynamicShadowIndex > 0;
			for (int32 ApplyLightFunctionIndex = 0; ApplyLightFunctionIndex < 2; ++ApplyLightFunctionIndex)
			{
				bool bApplyLightFunction = ApplyLightFunctionIndex > 0;
				for (int32 UseVSMIndex = 0; UseVSMIndex < 2; ++UseVSMIndex)
				{
					bool bUseVSM = UseVSMIndex > 0;
					for (int32 UseAdaptiveVolumetricShadowMapIndex = 0; UseAdaptiveVolumetricShadowMapIndex < 2; ++UseAdaptiveVolumetricShadowMapIndex)
					{
						bool bUseAdaptiveVolumetricShadowMap = UseAdaptiveVolumetricShadowMapIndex > 0;
						AddPSOInitializer(bDirectionalLight, bDynamicShadow, bApplyLightFunction, bUseVSM, bUseAdaptiveVolumetricShadowMap);
					}
				}
			}
		}
	}
}

IPSOCollector* CreateTranslucentLightingMaterialPSOCollector(ERHIFeatureLevel::Type FeatureLevel)
{
	return new FTranslucentLightingMaterialPSOCollector(FeatureLevel);
}
FRegisterPSOCollectorCreateFunction RegisterTranslucentLightingMaterialPSOCollector(&CreateTranslucentLightingMaterialPSOCollector, EShadingPath::Deferred, TranslucentLightingMaterialPSOCollectorName);

