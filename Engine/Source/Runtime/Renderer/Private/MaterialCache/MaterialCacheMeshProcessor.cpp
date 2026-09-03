// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCache/MaterialCacheMeshProcessor.h"
#include "RendererModule.h"
#include "MeshPassUtils.h"
#include "NaniteSceneProxy.h"
#include "NaniteVertexFactory.h"
#include "ScenePrivate.h"
#include "MaterialCache/MaterialCacheShaders.h"
#include "Materials/MaterialRenderProxy.h"
#include "Nanite/NaniteShading.h"
#include "Nanite/NaniteShared.h"
#include "MeshPassProcessor.inl"
#include "Materials/Material.h"

extern bool GMaterialCacheStaticMeshEnableViewportFromVS;

template<bool bSupportsViewportFromVS>
static bool GetMaterialCacheShaders(
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	TShaderRef<FMaterialCacheUnwrapVSBase>& VertexShader,
	TShaderRef<FMaterialCacheUnwrapPS>& PixelShader)
{
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FMaterialCacheUnwrapVS<bSupportsViewportFromVS>>();
	ShaderTypes.AddShaderType<FMaterialCacheUnwrapPS>();

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);
	return true;
}

template<typename T>
static bool LoadShadingMaterial(
	ERHIFeatureLevel::Type FeatureLevel,
	const FMaterialRenderProxy* MaterialProxy,
	const FVertexFactoryType* NaniteVertexFactoryType,
	TShaderRef<T>& ComputeShader)
{
	const FMaterial& ShadingMaterial = MaterialProxy->GetIncompleteMaterialWithFallback(FeatureLevel);
	check(Nanite::IsSupportedMaterialDomain(ShadingMaterial.GetMaterialDomain()));
	check(Nanite::IsSupportedBlendMode(ShadingMaterial));

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<T>();

	FMaterialShaders Shaders;
	if (!ShadingMaterial.TryGetShaders(ShaderTypes, NaniteVertexFactoryType, Shaders))
	{
		return false;
	}

	return Shaders.TryGetComputeShader(ComputeShader);
}

template<typename T>
bool CreateMaterialCacheComputeLayerShadingCommand(
	const FScene& Scene,
	const FPrimitiveSceneProxy* SceneProxy,
	const FMaterialRenderProxy* Material,
	bool bAllowDefaultFallback,
	FRHICommandListBase& RHICmdList,
	FMaterialCacheLayerShadingCSCommand& OutShadingCommand)
{
	const ERHIFeatureLevel::Type FeatureLevel = Scene.GetFeatureLevel();

	const FNaniteVertexFactory* NaniteVertexFactory     = Nanite::GVertexFactoryResource.GetVertexFactory();
	const FVertexFactoryType*   NaniteVertexFactoryType = NaniteVertexFactory->GetType();

	// Get first available material
	const FMaterialRenderProxy* MaterialProxy = Material;
	while (MaterialProxy)
	{
		if (MaterialProxy->GetMaterialNoFallback(FeatureLevel))
		{
			break;
		}
		
		MaterialProxy = MaterialProxy->GetFallback(FeatureLevel);
	}

	if (!MaterialProxy)
	{
		UE_LOG(LogRenderer, Error, TEXT("Failed to get material cache fallback proxy"));
		return false;
	}

	TShaderRef<T> ShadeCS;
	
	if (!LoadShadingMaterial(FeatureLevel, MaterialProxy, NaniteVertexFactoryType, ShadeCS))
	{
		MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();

		if (!bAllowDefaultFallback)
		{
			return false;
		}
		
		if (!LoadShadingMaterial(FeatureLevel, MaterialProxy, NaniteVertexFactoryType, ShadeCS))
		{
			return false;
		}
	}

	OutShadingCommand.ComputeShader = ShadeCS;
	
	MaterialProxy->UpdateUniformExpressionCacheIfNeeded(RHICmdList, FeatureLevel);

	UE::MeshPassUtils::SetupComputeBindings(
		ShadeCS, &Scene, FeatureLevel, SceneProxy, 
		*MaterialProxy, *MaterialProxy->GetMaterialNoFallback(FeatureLevel),
		OutShadingCommand.ShaderBindings
	);

	return true;
}

bool LoadMaterialCacheNaniteShadingPipeline(
	const FScene& Scene,
	const Nanite::FSceneProxyBase* SceneProxy,
	const Nanite::FSceneProxyBase::FMaterialSection& Section,
	FNaniteShadingPipeline& ShadingPipeline)
{
	const ERHIFeatureLevel::Type FeatureLevel = Scene.GetFeatureLevel();

	const FNaniteVertexFactory* NaniteVertexFactory     = Nanite::GVertexFactoryResource.GetVertexFactory();
	const FVertexFactoryType*   NaniteVertexFactoryType = NaniteVertexFactory->GetType();

	// Get first available material
	const FMaterialRenderProxy* MaterialProxy = Section.ShadingMaterialProxy;
	while (MaterialProxy)
	{
		if (MaterialProxy->GetMaterialNoFallback(FeatureLevel))
		{
			break;
		}
		
		MaterialProxy = MaterialProxy->GetFallback(FeatureLevel);
	}

	if (!MaterialProxy)
	{
		UE_LOG(LogRenderer, Error, TEXT("Failed to get material cache fallback proxy"));
		return false;
	}

	TShaderRef<FMaterialCacheNaniteShadeCS> ShadeCS;
	
	if (!LoadShadingMaterial(FeatureLevel, MaterialProxy, NaniteVertexFactoryType, ShadeCS))
	{
		MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();

		if (!LoadShadingMaterial(FeatureLevel, MaterialProxy, NaniteVertexFactoryType, ShadeCS))
		{
			return false;
		}
	}
	
	ShadingPipeline.MaterialProxy     = MaterialProxy;
	ShadingPipeline.Material          = MaterialProxy->GetMaterialNoFallback(FeatureLevel);
	ShadingPipeline.BoundTargetMask   = 0x0;
	ShadingPipeline.ComputeShader     = ShadeCS.GetComputeShader();
	ShadingPipeline.bIsTwoSided       = Section.MaterialRelevance.bTwoSided;
	ShadingPipeline.bIsMasked         = Section.MaterialRelevance.bMasked;
	ShadingPipeline.bNoDerivativeOps  = Nanite::HasNoDerivativeOps(ShadingPipeline.ComputeShader);
	ShadingPipeline.MaterialBitFlags  = Nanite::PackMaterialBitFlags(*ShadingPipeline.Material, ShadingPipeline.BoundTargetMask, ShadingPipeline.bNoDerivativeOps);
	ShadingPipeline.MaterialCacheData = MakePimpl<FNaniteMaterialCacheData, EPimplPtrMode::DeepCopy>();
	ShadingPipeline.ShaderBindings    = MakePimpl<FMeshDrawShaderBindings, EPimplPtrMode::DeepCopy>();
	
	ShadingPipeline.MaterialCacheData->TypedShader = ShadeCS;

	UE::MeshPassUtils::SetupComputeBindings(
		ShadeCS, &Scene, FeatureLevel, SceneProxy, 
		*MaterialProxy, *ShadingPipeline.Material,
		*ShadingPipeline.ShaderBindings
	);

	ShadingPipeline.ShaderBindingsHash = ShadingPipeline.ShaderBindings->GetDynamicInstancingHash();
	return true;
}

bool FMaterialCacheMeshProcessor::TryAddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial* Material)
{
	TMeshProcessorShaders<FMaterialCacheUnwrapVSBase, FMaterialCacheUnwrapPS> PassShaders;

	if (GRHISupportsArrayIndexFromAnyShader && GMaterialCacheStaticMeshEnableViewportFromVS)
	{
		if (!GetMaterialCacheShaders<true>(*Material, MeshBatch.VertexFactory->GetType(), PassShaders.VertexShader, PassShaders.PixelShader))
		{
			return false;
		}
	}
	else
	{
		if (!GetMaterialCacheShaders<false>(*Material, MeshBatch.VertexFactory->GetType(), PassShaders.VertexShader, PassShaders.PixelShader))
		{
			return false;
		}
	}

	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey            SortKey          = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode                MeshFillMode     = ComputeMeshFillMode(*Material, OverrideSettings);
	
	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		*MaterialRenderProxy,
		*Material,
		PassDrawRenderState,
		PassShaders,
		MeshFillMode,
		CM_None,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData
	);

	return true;
}

#if WITH_EDITOR
bool IsMaterialCacheMaterialReady(ERHIFeatureLevel::Type FeatureLevel, const FPrimitiveSceneProxy* Proxy)
{
	bool bAnyCaching = false;
	
	if (Proxy->IsNaniteMesh())
	{
		const auto* NaniteProxy = static_cast<const Nanite::FSceneProxy*>(Proxy);
		
		for (const Nanite::FSceneProxyBase::FMaterialSection& MaterialSection : NaniteProxy->GetMaterialSections())
		{
			if (const FMaterial* Material = MaterialSection.RasterMaterialProxy->GetMaterialNoFallback(FeatureLevel))
			{
				bAnyCaching |= Material->IsCachingShaders();
			}

			if (const FMaterial* Material = MaterialSection.ShadingMaterialProxy->GetMaterialNoFallback(FeatureLevel))
			{
				bAnyCaching |= Material->IsCachingShaders();
			}
		}
	}
	else
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy->GetPrimitiveSceneInfo();
		if (!PrimitiveSceneInfo)
		{
			return false;
		}

		for (const FStaticMeshBatch& StaticMesh : PrimitiveSceneInfo->StaticMeshes)
		{
			if (const FMaterial* Material = StaticMesh.MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel))
			{
				bAnyCaching |= Material->IsCachingShaders();
			}
		}
	}

	return !bAnyCaching;
}
#endif // WITH_EDITOR

void FMaterialCacheMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (!MeshBatch.bUseForMaterial)
	{
		return;
	}
	
	const FMaterialRenderProxy* MaterialRenderProxy = OverrideLayerMaterialProxy ? OverrideLayerMaterialProxy : MeshBatch.MaterialRenderProxy;
	while (MaterialRenderProxy)
	{
		if (const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel))
		{
			if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, MaterialRenderProxy, Material))
			{
				break;
			}
		}

		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
	}
}

void FMaterialCacheMeshProcessor::CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
{
	if (!PreCacheParams.bRenderInMainPass)
	{
		return;
	}

	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
	const ERasterizerFillMode                MeshFillMode     = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode                MeshCullMode     = ComputeMeshCullMode(Material, OverrideSettings);

	TMeshProcessorShaders<FMaterialCacheUnwrapVSBase, FMaterialCacheUnwrapPS> PassShaders;

	if (GRHISupportsArrayIndexFromAnyShader && GMaterialCacheStaticMeshEnableViewportFromVS)
	{
		if (!GetMaterialCacheShaders<true>(Material, VertexFactoryData.VertexFactoryType, PassShaders.VertexShader, PassShaders.PixelShader))
		{
			return;
		}
	}
	else
	{
		if (!GetMaterialCacheShaders<false>(Material, VertexFactoryData.VertexFactoryType, PassShaders.VertexShader, PassShaders.PixelShader))
		{
			return;
		}
	}

	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	RenderTargetsInfo.NumSamples = 1;
	RenderTargetsInfo.RenderTargetsEnabled = 1;

	// First exported attribute
	// TODO[MP]: Support multiple physical layers
	RenderTargetsInfo.RenderTargetFormats[0] = PF_R8G8B8A8;
	RenderTargetsInfo.RenderTargetFlags[0]   = TexCreate_ShaderResource | TexCreate_RenderTargetable;

	AddGraphicsPipelineStateInitializer(
		VertexFactoryData,
		Material,
		PassDrawRenderState,
		RenderTargetsInfo,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		static_cast<EPrimitiveType>(PreCacheParams.PrimitiveType),
		EMeshPassFeatures::Default, 
		true,
		PSOInitializers
	);
}

FMeshDrawCommand& FMaterialCacheMeshPassContext::AddCommand(FMeshDrawCommand& Initializer, uint32 NumElements)
{
	
	return Initializer;
}

void FMaterialCacheMeshPassContext::FinalizeCommand(
	const FMeshBatch& MeshBatch, int32 BatchElementIndex, const FMeshDrawCommandPrimitiveIdInfo& IdInfo,
	ERasterizerFillMode MeshFillMode, ERasterizerCullMode MeshCullMode, FMeshDrawCommandSortKey SortKey, EFVisibleMeshDrawCommandFlags Flags,
	const FGraphicsMinimalPipelineStateInitializer& PipelineState, const FMeshProcessorShaders* ShadersForDebugging,
	FMeshDrawCommand& MeshDrawCommand)
{
	FGraphicsMinimalPipelineStateId PipelineId = FGraphicsMinimalPipelineStateId::GetPersistentId(PipelineState);

	MeshDrawCommand.SetDrawParametersAndFinalize(MeshBatch, BatchElementIndex, PipelineId, ShadersForDebugging);

	Command.Command = MeshDrawCommand;
	Command.CommandInfo = FCachedMeshDrawCommandInfo(EMeshPass::MaterialCacheProjection);
	Command.CommandInfo.SortKey = SortKey;
	Command.CommandInfo.CullingPayload = CreateCullingPayload(MeshBatch, MeshBatch.Elements[BatchElementIndex]);
	Command.CommandInfo.MeshFillMode = MeshFillMode;
	Command.CommandInfo.MeshCullMode = MeshCullMode;
	Command.CommandInfo.Flags = Flags;
}

FMaterialCacheMeshProcessor::FMaterialCacheMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext, const FMaterialRenderProxy* OverrideLayerMaterialProxy)
	: FMeshPassProcessor(EMeshPass::MaterialCacheProjection, Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, OverrideLayerMaterialProxy(OverrideLayerMaterialProxy)
	, PassDrawRenderState(InPassDrawRenderState)
{
	
}

void CreateMaterialCacheStaticLayerDrawCommand(
	FScene& Scene,
	const FPrimitiveSceneProxy* Proxy,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FStaticMeshBatch& MeshBatch,
	FMaterialCacheMeshDrawCommand& OutMeshCommand)
{
	FMaterialCacheMeshPassContext Context;

	// TODO[MP]: Fixed function blending is a developmental thing
	FMeshPassProcessorRenderState PassState;
	PassState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	PassState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One>::GetRHI());

	// Process the command
	// TODO[MP]: Consider instantiating once somewhere
	FMaterialCacheMeshProcessor Processor(&Scene, Scene.GetFeatureLevel(), nullptr, PassState, &Context, MaterialRenderProxy);
	Processor.AddMeshBatch(MeshBatch, ~0ull, Proxy);
	OutMeshCommand = Context.Command;
}

FMeshPassProcessor* CreateMaterialCacheMeshProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassState;
	PassState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	PassState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One>::GetRHI());
	return new FMaterialCacheMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassState, InDrawListContext, nullptr);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(MaterialCacheMesh, CreateMaterialCacheMeshProcessor, EShadingPath::Deferred, EMeshPass::MaterialCacheProjection, EMeshPassFlags::CachedMeshCommands);

/** Instantiate per shading command */
template bool CreateMaterialCacheComputeLayerShadingCommand<FMaterialCacheShadeCS>(const FScene& Scene, const FPrimitiveSceneProxy* SceneProxy, const FMaterialRenderProxy* Material, bool bAllowDefaultFallback, FRHICommandListBase& RHICmdList, FMaterialCacheLayerShadingCSCommand& Out);
template bool CreateMaterialCacheComputeLayerShadingCommand<FMaterialCacheNaniteShadeCS>(const FScene& Scene, const FPrimitiveSceneProxy* SceneProxy, const FMaterialRenderProxy* Material, bool bAllowDefaultFallback, FRHICommandListBase& RHICmdList, FMaterialCacheLayerShadingCSCommand& Out);
