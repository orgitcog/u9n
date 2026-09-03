// Copyright Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "VolumeLighting.h"
#include "MeshPassProcessor.inl"
#include "LumenTranslucencyVolumeLighting.h"
#include "LumenRadianceCache.h"
#include "TranslucentLighting.h"

int32 GLumenTranslucencyRadianceCacheReflections = 1;
FAutoConsoleVariableRef CVarLumenTranslucencyRadianceCache(
	TEXT("r.Lumen.TranslucencyReflections.RadianceCache"),
	GLumenTranslucencyRadianceCacheReflections,
	TEXT("Whether to use the Radiance Cache to provide Lumen Reflections on Translucent Surfaces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenTranslucencyReflectionsMarkDownsampleFactor = 4;
FAutoConsoleVariableRef CVarLumenTranslucencyRadianceCacheDownsampleFactor(
	TEXT("r.Lumen.TranslucencyReflections.MarkDownsampleFactor"),
	GLumenTranslucencyReflectionsMarkDownsampleFactor,
	TEXT("Downsample factor for marking translucent surfaces in the Lumen Radiance Cache.  Too low of factors will cause incorrect Radiance Cache coverage.  Should be a power of 2."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool GLumenTranslucencyRadianceCacheHZBOcclusionTest = true;
FAutoConsoleVariableRef CVarLumenTranslucencyRadianceCacheHZBOcclusionTest(
	TEXT("r.Lumen.TranslucencyReflections.HZBOcclusionTest"),
	GLumenTranslucencyRadianceCacheHZBOcclusionTest,
	TEXT("Whether to use HZB occlusion test when marking translucent surfaces in the Lumen Radiance Cache."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenTranslucencyReflectionsRadianceCacheReprojectionRadiusScale = 10;
FAutoConsoleVariableRef CVarLumenTranslucencyRadianceCacheReprojectionRadiusScale(
	TEXT("r.Lumen.TranslucencyReflections.ReprojectionRadiusScale"),
	GLumenTranslucencyReflectionsRadianceCacheReprojectionRadiusScale,
	TEXT("Larger values treat the Radiance Cache lighting as more distant."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenTranslucencyVolumeRadianceCacheClipmapFadeSize = 4.0f;
FAutoConsoleVariableRef CVarLumenTranslucencyVolumeRadianceCacheClipmapFadeSize(
	TEXT("r.Lumen.TranslucencyReflections.ClipmapFadeSize"),
	GLumenTranslucencyVolumeRadianceCacheClipmapFadeSize,
	TEXT("Size in Radiance Cache probes of the dithered transition region between clipmaps"),
	ECVF_RenderThreadSafe
);

namespace Lumen
{
	bool UseLumenTranslucencyRadianceCacheReflections(const FSceneViewFamily& ViewFamily)
	{
		return GLumenTranslucencyRadianceCacheReflections != 0 && ViewFamily.EngineShowFlags.LumenReflections;
	}

	bool ShouldRenderInTranslucencyRadianceCacheMarkPass(bool bShouldRenderInMainPass, const FMaterial& Material)
	{
		const bool bIsTranslucent = IsTranslucentBlendMode(Material);
		const ETranslucencyLightingMode TranslucencyLightingMode = Material.GetTranslucencyLightingMode();

		return bIsTranslucent
			&& (TranslucencyLightingMode == TLM_Surface || TranslucencyLightingMode == TLM_SurfacePerPixelLighting || IsTranslucencyLightingVolumeUsingVoxelMarking())
			&& bShouldRenderInMainPass
			&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain());
	}
}

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLumenTranslucencyRadianceCacheMarkPassUniformParameters, )
	SHADER_PARAMETER_STRUCT(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_STRUCT_INCLUDE(FHZBParameters, HZBParameters)
	SHADER_PARAMETER(float, HZBMipLevel)
	SHADER_PARAMETER(uint32, UseHZBTest)

	SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheMarkParameters, RadianceCacheMarkParameters)
	SHADER_PARAMETER(uint32, MarkRadianceCache)

	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, InnerVolumeMarkTexture)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, OuterVolumeMarkTexture)
	SHADER_PARAMETER(FIntVector, TranslucencyLightingVolumeSize)
	SHADER_PARAMETER(uint32, MarkTranslucencyLightingVolume)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FLumenTranslucencyRadianceCacheMarkPassUniformParameters, "LumenTranslucencyRadianceCacheMarkPass", SceneTextures);

class FLumenTranslucencyRadianceCacheMarkVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenTranslucencyRadianceCacheMarkVS, MeshMaterial);

protected:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
			&& IsTranslucentBlendMode(Parameters.MaterialParameters)
			&& ((DoesPlatformSupportLumenGI(Parameters.Platform) && Parameters.MaterialParameters.bIsTranslucencySurface)
				|| IsTranslucencyLightingVolumeUsingVoxelMarkingSupported());
	}

	FLumenTranslucencyRadianceCacheMarkVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}

	FLumenTranslucencyRadianceCacheMarkVS() = default;
};


IMPLEMENT_MATERIAL_SHADER_TYPE(, FLumenTranslucencyRadianceCacheMarkVS, TEXT("/Engine/Private/Lumen/LumenTranslucencyRadianceCacheMarkShaders.usf"), TEXT("MainVS"), SF_Vertex);

class FLumenTranslucencyRadianceCacheMarkPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenTranslucencyRadianceCacheMarkPS, MeshMaterial);

public:
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
			&& IsTranslucentBlendMode(Parameters.MaterialParameters)
			&& ((DoesPlatformSupportLumenGI(Parameters.Platform) && Parameters.MaterialParameters.bIsTranslucencySurface)
				|| IsTranslucencyLightingVolumeUsingVoxelMarkingSupported());
	}

	FLumenTranslucencyRadianceCacheMarkPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}

	FLumenTranslucencyRadianceCacheMarkPS() = default;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLumenTranslucencyRadianceCacheMarkPS, TEXT("/Engine/Private/Lumen/LumenTranslucencyRadianceCacheMarkShaders.usf"), TEXT("MainPS"), SF_Pixel);

class FLumenTranslucencyRadianceCacheMarkMeshProcessor : public FSceneRenderingAllocatorObject<FLumenTranslucencyRadianceCacheMarkMeshProcessor>, public FMeshPassProcessor
{
public:

	FLumenTranslucencyRadianceCacheMarkMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) override final;

	FMeshPassProcessorRenderState PassDrawRenderState;
};

bool GetLumenTranslucencyRadianceCacheMarkShaders(
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	TShaderRef<FLumenTranslucencyRadianceCacheMarkVS>& VertexShader,
	TShaderRef<FLumenTranslucencyRadianceCacheMarkPS>& PixelShader)
{
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLumenTranslucencyRadianceCacheMarkVS>();
	ShaderTypes.AddShaderType<FLumenTranslucencyRadianceCacheMarkPS>();

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);
	return true;
}

bool CanMaterialRenderInLumenTranslucencyRadianceCacheMarkPass(
	const FScene& Scene,
	const FSceneViewFamily& ViewFamily,
	const FPrimitiveSceneProxy& PrimitiveSceneProxy,
	const FMaterial& Material)
{
	const FSceneView* View = ViewFamily.Views[0];
	check(View);

	const bool bIsTranslucencyMarkPassNeeded = ShouldRenderLumenDiffuseGI(&Scene, *View) || IsTranslucencyLightingVolumeUsingVoxelMarking();

	return bIsTranslucencyMarkPassNeeded && Lumen::ShouldRenderInTranslucencyRadianceCacheMarkPass(PrimitiveSceneProxy.ShouldRenderInMainPass(), Material);
}

void FLumenTranslucencyRadianceCacheMarkMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (!ViewIfDynamicMeshCommand)
	{
		return;
	}

	const bool bIsTranslucencyMarkPassNeeded = ShouldRenderLumenDiffuseGI(Scene, *ViewIfDynamicMeshCommand) || IsTranslucencyLightingVolumeUsingVoxelMarking();

	if (MeshBatch.bUseForMaterial 
		&& PrimitiveSceneProxy
		//@todo - this filter should be done at a higher level
		&& bIsTranslucencyMarkPassNeeded)
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxy)
		{
			const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			if (Material)
			{
				auto TryAddMeshBatch = [this](const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId, const FMaterialRenderProxy& MaterialRenderProxy, const FMaterial& Material) -> bool
				{
					if (Lumen::ShouldRenderInTranslucencyRadianceCacheMarkPass(PrimitiveSceneProxy->ShouldRenderInMainPass(), Material))
					{
						const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
						FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

						TMeshProcessorShaders<
							FLumenTranslucencyRadianceCacheMarkVS,
							FLumenTranslucencyRadianceCacheMarkPS> PassShaders;

						if (!GetLumenTranslucencyRadianceCacheMarkShaders(
							Material,
							VertexFactory->GetType(),
							PassShaders.VertexShader,
							PassShaders.PixelShader))
						{
							return false;
						}

						FMeshMaterialShaderElementData ShaderElementData;
						ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

						const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
						const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
						const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);
						const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

						BuildMeshDrawCommands(
							MeshBatch,
							BatchElementMask,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							PassDrawRenderState,
							PassShaders,
							MeshFillMode,
							MeshCullMode,
							SortKey,
							EMeshPassFeatures::Default,
							ShaderElementData);
					}

					return true;
				};

				if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
				{
					break;
				}
			}

			MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
		}
	}
}

void FLumenTranslucencyRadianceCacheMarkMeshProcessor::CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
{
	LLM_SCOPE_BYTAG(Lumen);
	
	if (PreCacheParams.bRenderInMainPass && !Lumen::ShouldRenderInTranslucencyRadianceCacheMarkPass(PreCacheParams.bRenderInMainPass, Material))
	{
		return;
	}

	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

	TMeshProcessorShaders<
		FLumenTranslucencyRadianceCacheMarkVS,
		FLumenTranslucencyRadianceCacheMarkPS> PassShaders;

	if (!GetLumenTranslucencyRadianceCacheMarkShaders(
		Material,
		VertexFactoryData.VertexFactoryType,
		PassShaders.VertexShader,
		PassShaders.PixelShader))
	{
		return;
	}

	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	AddGraphicsPipelineStateInitializer(
		VertexFactoryData,
		Material,
		PassDrawRenderState,
		RenderTargetsInfo,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		(EPrimitiveType)PreCacheParams.PrimitiveType,
		EMeshPassFeatures::Default,
		true /*bRequired*/,
		PSOInitializers);
}

FLumenTranslucencyRadianceCacheMarkMeshProcessor::FLumenTranslucencyRadianceCacheMarkMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::LumenTranslucencyRadianceCacheMark, Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InPassDrawRenderState)
{}

FMeshPassProcessor* CreateLumenTranslucencyRadianceCacheMarkPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	LLM_SCOPE_BYTAG(Lumen);

	FMeshPassProcessorRenderState PassState;

	// We use HZB tests in the shader instead of hardware depth testing
	PassState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	PassState.SetBlendState(TStaticBlendState<>::GetRHI());

	return new FLumenTranslucencyRadianceCacheMarkMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassState, InDrawListContext);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(LumenTranslucencyRadianceCacheMarkPass, CreateLumenTranslucencyRadianceCacheMarkPassProcessor, EShadingPath::Deferred, EMeshPass::LumenTranslucencyRadianceCacheMark, EMeshPassFlags::MainView);

BEGIN_SHADER_PARAMETER_STRUCT(FLumenTranslucencyRadianceCacheMarkParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenTranslucencyRadianceCacheMarkPassUniformParameters, MarkPass)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

extern int32 GetTranslucencyLightingVolumeDim();

void LumenTranslucencyReflectionsMarkUsedProbes(
	FRDGBuilder& GraphBuilder,
	const FSceneRenderer& SceneRenderer,
	FViewInfo& View,
	const FSceneTextures& SceneTextures,
	const LumenRadianceCache::FRadianceCacheMarkParameters* RadianceCacheMarkParameters)
{
	check(GLumenTranslucencyRadianceCacheReflections != 0);

	const EMeshPass::Type MeshPass = EMeshPass::LumenTranslucencyRadianceCacheMark;
	const float ViewportScale = 1.0f / GLumenTranslucencyReflectionsMarkDownsampleFactor;
	FIntRect DownsampledViewRect = GetScaledRect(View.ViewRect, ViewportScale);

	auto* Pass = View.ParallelMeshDrawCommandPasses[MeshPass];

	if (!Pass)
	{
		return;
	}

	View.BeginRenderView();

	FLumenTranslucencyRadianceCacheMarkParameters* PassParameters = GraphBuilder.AllocParameters<FLumenTranslucencyRadianceCacheMarkParameters>();

	{
		FViewUniformShaderParameters DownsampledTranslucencyViewParameters = *View.CachedViewUniformShaderParameters;

		FViewMatrices ViewMatrices = View.ViewMatrices;
		FViewMatrices PrevViewMatrices = View.PrevViewInfo.ViewMatrices;

		// Update the parts of DownsampledTranslucencyParameters which are dependent on the buffer size and view rect
		View.SetupViewRectUniformBufferParameters(
			DownsampledTranslucencyViewParameters,
			SceneTextures.Config.Extent,
			DownsampledViewRect,
			ViewMatrices,
			PrevViewMatrices);

		PassParameters->View.View = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(DownsampledTranslucencyViewParameters, UniformBuffer_SingleFrame);
		
		if (View.bShouldBindInstancedViewUB)
		{
			FInstancedViewUniformShaderParameters LocalInstancedViewUniformShaderParameters;
			InstancedViewParametersUtils::CopyIntoInstancedViewParameters(LocalInstancedViewUniformShaderParameters, DownsampledTranslucencyViewParameters, 0);

			if (const FViewInfo* InstancedView = View.GetInstancedView())
			{
				InstancedView->SetupViewRectUniformBufferParameters(
					DownsampledTranslucencyViewParameters,
					SceneTextures.Config.Extent,
					GetScaledRect(InstancedView->ViewRect, ViewportScale),
					ViewMatrices,
					PrevViewMatrices);

				InstancedViewParametersUtils::CopyIntoInstancedViewParameters(LocalInstancedViewUniformShaderParameters, DownsampledTranslucencyViewParameters, 1);
			}

			PassParameters->View.InstancedView = TUniformBufferRef<FInstancedViewUniformShaderParameters>::CreateUniformBufferImmediate(
				reinterpret_cast<const FInstancedViewUniformShaderParameters&>(LocalInstancedViewUniformShaderParameters),
				UniformBuffer_SingleFrame);
		}
	}

	const bool bMarkRadianceCache = (RadianceCacheMarkParameters != nullptr);

	LumenRadianceCache::FRadianceCacheMarkParameters NullPlaceholder;
	if (RadianceCacheMarkParameters == nullptr)
	{
		FRDGTextureRef PlaceholderTexture = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create3D(FIntVector(4,4,4), PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("LumenRadianceCacheMarkPlaceholder"));
		NullPlaceholder.RWRadianceProbeIndirectionTexture = GraphBuilder.CreateUAV(PlaceholderTexture);
		RadianceCacheMarkParameters = &NullPlaceholder;
	}

	const FIntVector TranslucencyLightingVolumeDim(GetTranslucencyLightingVolumeDim());

	FRDGTextureUAVRef InnerVolumeMarkTextureUAV = nullptr;
	FRDGTextureUAVRef OuterVolumeMarkTextureUAV = nullptr;

	const bool bMarkTranslucencyLightingVolume = IsTranslucencyLightingVolumeUsingVoxelMarking();

	if (bMarkTranslucencyLightingVolume)
	{
		View.TranslucencyVolumeMarkData[0].MarkTexture = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create3D(TranslucencyLightingVolumeDim, PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("TranslucencyLightVolume.InnerMarkTexture"));

		View.TranslucencyVolumeMarkData[1].MarkTexture = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create3D(TranslucencyLightingVolumeDim, PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("TranslucencyLightVolume.OuterMarkTexture"));

		InnerVolumeMarkTextureUAV = GraphBuilder.CreateUAV(View.TranslucencyVolumeMarkData[0].MarkTexture);
		OuterVolumeMarkTextureUAV = GraphBuilder.CreateUAV(View.TranslucencyVolumeMarkData[1].MarkTexture);

		AddClearUAVPass(GraphBuilder, InnerVolumeMarkTextureUAV, 0u);
		AddClearUAVPass(GraphBuilder, OuterVolumeMarkTextureUAV, 0u);
	}
	else
	{
		FRDGTextureRef PlaceholderTexture0 = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create3D(FIntVector(4, 4, 4), PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("TranslucencyLightVolume.InnerMarkTexture.Placeholder"));

		FRDGTextureRef PlaceholderTexture1 = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create3D(FIntVector(4, 4, 4), PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("TranslucencyLightVolume.OuterMarkTexture.Placeholder"));

		InnerVolumeMarkTextureUAV = GraphBuilder.CreateUAV(PlaceholderTexture0);
		OuterVolumeMarkTextureUAV = GraphBuilder.CreateUAV(PlaceholderTexture1);
	}

	{
		FLumenTranslucencyRadianceCacheMarkPassUniformParameters& MarkPassParameters = *GraphBuilder.AllocParameters<FLumenTranslucencyRadianceCacheMarkPassUniformParameters>();
		SetupSceneTextureUniformParameters(GraphBuilder, &SceneTextures, View.FeatureLevel, ESceneTextureSetupMode::All, MarkPassParameters.SceneTextures);
		MarkPassParameters.RadianceCacheMarkParameters = *RadianceCacheMarkParameters;
		MarkPassParameters.RadianceCacheMarkParameters.InvClipmapFadeSizeForMark = 1.0f / FMath::Clamp(GLumenTranslucencyVolumeRadianceCacheClipmapFadeSize, .001f, 16.0f);
		MarkPassParameters.MarkRadianceCache = bMarkRadianceCache ? 1 : 0;

		MarkPassParameters.HZBParameters = GetHZBParameters(GraphBuilder, View, EHZBType::FurthestHZB);
		MarkPassParameters.HZBMipLevel = FMath::Max<float>((int32)FMath::FloorLog2((float)GLumenTranslucencyReflectionsMarkDownsampleFactor) - 1, 0.0f);
		MarkPassParameters.UseHZBTest = GLumenTranslucencyRadianceCacheHZBOcclusionTest ? 1 : 0;

		MarkPassParameters.InnerVolumeMarkTexture = InnerVolumeMarkTextureUAV;
		MarkPassParameters.OuterVolumeMarkTexture = OuterVolumeMarkTextureUAV;
		MarkPassParameters.TranslucencyLightingVolumeSize = TranslucencyLightingVolumeDim;
		MarkPassParameters.MarkTranslucencyLightingVolume = bMarkTranslucencyLightingVolume ? 1 : 0;

		PassParameters->MarkPass = GraphBuilder.CreateUniformBuffer(&MarkPassParameters);
	}

	Pass->BuildRenderingCommands(GraphBuilder, SceneRenderer.Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("TranslucentSurfacesMarkPass"),
		PassParameters,
		ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
		[&View, Pass, &SceneRenderer, MeshPass, PassParameters, ViewportScale, DownsampledViewRect](FRDGAsyncTask, FRHICommandList& RHICmdList)
	{
		FRHIRenderPassInfo RPInfo;
		RPInfo.ResolveRect = FResolveRect(DownsampledViewRect);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("LumenTranslucencyRadianceCacheMark"));

		FSceneRenderer::SetStereoViewport(RHICmdList, View, ViewportScale);
		Pass->Draw(RHICmdList, &PassParameters->InstanceCullingDrawParams);

		RHICmdList.EndRenderPass();
	});
}
