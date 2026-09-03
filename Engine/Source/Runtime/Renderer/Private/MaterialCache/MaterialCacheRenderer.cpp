// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCache/MaterialCacheRenderer.h"
#include "DeferredShadingRenderer.h"
#include "MeshPassProcessor.h"
#include "PrimitiveSceneInfo.h"
#include "ScenePrivate.h"
#include "BasePassRendering.h"
#include "ComponentRecreateRenderStateContext.h"
#include "MaterialCachedData.h"
#include "InstanceCulling/InstanceCullingManager.h"
#include "Nanite/NaniteRayTracing.h"
#include "Nanite/NaniteShading.h"
#include "Nanite/NaniteShared.h"
#include "MaterialCache/MaterialCacheShaders.h"
#include "Rendering/NaniteStreamingManager.h"
#include "MaterialCacheDefinitions.h"
#include "RendererModule.h"
#include "MaterialCache/MaterialCache.h"
#include "MaterialCache/MaterialCacheMeshProcessor.h"
#include "MaterialCache/MaterialCachePrimitiveData.h"
#include "MaterialCache/MaterialCacheSceneExtension.h"
#include "MaterialCache/MaterialCacheStackProvider.h"
#include "Materials/MaterialRenderProxy.h"

static void MaterialCacheInvalidateRenderStates(IConsoleVariable*)
{
	FGlobalComponentRecreateRenderStateContext{}; //-V607
}

bool GMaterialCacheStaticMeshEnableViewportFromVS = true;
static FAutoConsoleVariableRef CVarMaterialCacheStaticMeshEnableViewportFromVS(
	TEXT("r.MaterialCache.StaticMesh.EnableViewportFromVS"),
	GMaterialCacheStaticMeshEnableViewportFromVS,
	TEXT("Enable sliced rendering of static unwrapping on platforms that support render target array index from vertex shaders"),
	FConsoleVariableDelegate::CreateStatic(MaterialCacheInvalidateRenderStates),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

bool GMaterialCacheVertexInvariantEnable = true;
static FAutoConsoleVariableRef CVarMaterialCacheEnableVertexInvariant(
	TEXT("r.MaterialCache.VertexInvariant.Enable"),
	GMaterialCacheVertexInvariantEnable,
	TEXT("Enable compute-only shading of materials that only use UV-derived (or vertex-invariant) data"),
	FConsoleVariableDelegate::CreateStatic(MaterialCacheInvalidateRenderStates),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

bool GMaterialCacheComamndCaching = false;
static FAutoConsoleVariableRef CVarMaterialCacheCommandCaching(
	TEXT("r.MaterialCache.CommandCaching"),
	GMaterialCacheComamndCaching,
	TEXT("Enable caching of mesh commands and layer shading commands"),
	FConsoleVariableDelegate::CreateStatic(MaterialCacheInvalidateRenderStates),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

BEGIN_SHADER_PARAMETER_STRUCT(FMaterialCacheABufferParameters, )
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, RWABuffer0)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, RWABuffer1)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, RWABuffer2)
END_SHADER_PARAMETER_STRUCT()

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FMaterialCacheUniformParameters, RENDERER_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FMaterialCacheABufferParameters, ABuffer)
	SHADER_PARAMETER_STRUCT(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint4>, ShadingBinData)
	SHADER_PARAMETER(uint32, SvPagePositionModMask)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FMaterialCacheRastShadeParameters, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMaterialCacheUniformParameters, Pass)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FMaterialCacheNaniteShadeParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRasterUniformParameters, NaniteRaster)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteShadingUniformParameters, NaniteShading)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FOpaqueBasePassUniformParameters, BasePass)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FMaterialCacheNaniteStackShadeParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, PageIndirections)
	SHADER_PARAMETER_STRUCT_INCLUDE(FMaterialCacheNaniteShadeParameters, Shade)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMaterialCacheUniformParameters, Pass)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FMaterialCacheCSStackShadeParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, PageIndirections)
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FOpaqueBasePassUniformParameters, BasePass)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMaterialCacheUniformParameters, Pass)
END_SHADER_PARAMETER_STRUCT()

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FMaterialCacheUniformParameters, "MaterialCachePass", SceneTextures);

DECLARE_GPU_STAT(MaterialCacheCompositePages);
DECLARE_GPU_STAT(MaterialCacheFinalize);

enum class EMaterialCacheRenderPath
{
	/**
	 * Standard hardware rasterization unwrap path
	 * Batches to a single mesh command set per layer
	 */
	HardwareRaster,

	/**
	 * Nanite rasterization unwrap path
	 * All pages shader the same rasterization context / vis-buffer, a single stack shares the same page vis-region
	 * Shading is parallel per layer, batched by material then primitive
	 */
	NaniteRaster,

	/**
	 * Shade-only path, enabled when the material doesn't make use of non-uv derived vertex data
	 */
	VertexInvariant,
	
	Count
};

struct FMaterialCacheGenericCSPrimitiveBatch
{
	const FPrimitiveSceneProxy* Proxy = nullptr;

	uint32_t PageIndirectionOffset = 0;

	TArray<uint32, SceneRenderingAllocator> Pages;

	FMaterialCacheLayerShadingCSCommand* ShadingCommand = nullptr;
};

struct FMaterialCacheGenericCSMaterialBatch
{
	const FMaterialRenderProxy* Material = nullptr;

	TArray<FMaterialCacheGenericCSPrimitiveBatch, SceneRenderingAllocator> PrimitiveBatches;
};

struct FMaterialCacheGenericCSBatch
{
	FRDGBufferRef PageIndirectionBuffer;

	uint32 PageCount = 0;

	TArray<FMaterialCacheGenericCSMaterialBatch, SceneRenderingAllocator> MaterialBatches;
};

struct FMaterialCacheStaticMeshCommand
{
	uint32 PageIndex;
	
	FVector4f UnwrapMinAndInvSize;
};

struct FMaterialCacheHardwareLayerRenderData
{
	TArray<FMaterialCacheStaticMeshCommand, SceneRenderingAllocator> MeshCommands;
	
	FMeshCommandOneFrameArray VisibleMeshCommands;
	
	TArray<int32, SceneRenderingAllocator> PrimitiveIds;
};

struct FMaterialCacheNaniteLayerRenderData
{
	FMaterialCacheGenericCSBatch GenericCSBatch;
};

struct FMaterialCacheNaniteRenderData
{
	TArray<Nanite::FInstanceDraw, SceneRenderingAllocator> InstanceDraws;

	TArray<FNaniteShadingBin, SceneRenderingAllocator> ShadingBins;

	FNaniteShadingCommands ShadingCommands;
};

struct FMaterialCacheVertexInvariantLayerRenderData
{
	FMaterialCacheGenericCSBatch GenericCSBatch;
};

struct FMaterialCachePageInfo
{
	FMaterialCachePageEntry Page;

	uint32_t ABufferPageIndex = 0;

	uint32_t SetupEntryIndex = 0;
};

struct FMaterialCachePageCollection
{
	TArray<FMaterialCachePageInfo, SceneRenderingAllocator> Pages;
};

struct FMaterialCacheLayerRenderData
{
	FMaterialCacheHardwareLayerRenderData Hardware;

	FMaterialCacheNaniteLayerRenderData Nanite;

	FMaterialCacheVertexInvariantLayerRenderData VertexInvariant;
};

enum class EMaterialCacheABufferTileLayout
{
	Horizontal,
	Sliced
};

struct FMaterialCacheABuffer
{
	EMaterialCacheABufferTileLayout Layout;

	TArray<FMaterialCachePageEntry> Pages;

	TArray<FRDGTextureRef, TInlineAllocator<3u>> ABufferTextures;
};

struct FMaterialCacheRenderData
{
	FMaterialCachePageCollection PageCollections[static_cast<uint32>(EMaterialCacheRenderPath::Count)];

	FMaterialCacheABuffer ABuffer;

	FMaterialCacheNaniteRenderData Nanite;

	TArray<FMaterialCacheLayerRenderData, SceneRenderingAllocator> Layers;
};

static constexpr uint32 ABufferPageIndexNotProduced = UINT32_MAX;

struct FMaterialCachePendingPageEntry
{
	FMaterialCachePageEntry Page;

	uint32 ABufferPageIndex = ABufferPageIndexNotProduced;
};

struct FMaterialCacheBlackboardPendingEntry
{
	FMaterialCacheSetup Setup;

	TArray<FMaterialCachePendingPageEntry, SceneRenderingAllocator> Pages;
};

struct FMaterialCacheBlackboardData
{
	/** Aggregated data */
	TArray<FMaterialCacheBlackboardPendingEntry, SceneRenderingAllocator> PendingEntries;

	/** Batched render data */
	FMaterialCacheRenderData RenderData;
};

struct FMaterialCacheHardwareContext
{
	FMaterialCacheUniformParameters* PassUniformParameters = nullptr;
};

struct FMaterialCacheNaniteContext
{
	FMaterialCacheNaniteShadeParameters* PassShadeParameters = nullptr;
	FMaterialCacheUniformParameters* PassUniformParameters = nullptr;
};

struct FMaterialCacheVertexInvariantContext
{
	FMaterialCacheUniformParameters* PassUniformParameters = nullptr;
};

RDG_REGISTER_BLACKBOARD_STRUCT(FMaterialCacheBlackboardData);

static EMaterialCacheRenderPath GetMaterialCacheRenderPath(FSceneRenderer* Renderer, const FPrimitiveSceneProxy* Proxy, const FMaterialCacheStackEntry& StackEntry)
{
	// If the material doesn't make use of non-uv derived expressions, push it through the vertex invariant path
	if (FMaterialResource* Resource = StackEntry.Material->GetMaterialInterface()->GetMaterialResource(Renderer->FeatureLevel))
	{
		if (GMaterialCacheVertexInvariantEnable && !Resource->GetCachedExpressionData().bMaterialCacheHasNonUVDerivedExpression)
		{
			return EMaterialCacheRenderPath::VertexInvariant;
		}
	}

	// Otherwise, we need to rasterize, select the appropriate path
	if (Proxy->IsNaniteMesh())
	{
		return EMaterialCacheRenderPath::NaniteRaster;
	}
	else
	{
		return EMaterialCacheRenderPath::HardwareRaster;
	}
}

static FMaterialCacheGenericCSPrimitiveBatch& GetOrCreateCSPrimitiveBatch(FMaterialCacheGenericCSMaterialBatch& MaterialBatch, const FPrimitiveSceneProxy* Proxy)
{
	for (FMaterialCacheGenericCSPrimitiveBatch& PrimitiveBatch : MaterialBatch.PrimitiveBatches)
	{
		if (PrimitiveBatch.Proxy == Proxy)
		{
			return PrimitiveBatch;
		}
	}

	FMaterialCacheGenericCSPrimitiveBatch& PrimitiveBatch = MaterialBatch.PrimitiveBatches.Emplace_GetRef();
	PrimitiveBatch.Proxy = Proxy;
	return PrimitiveBatch;
}

static FMaterialCacheGenericCSMaterialBatch& GetOrCreateCSMaterialBatch(FMaterialCacheGenericCSBatch& LayerBatch, const FMaterialRenderProxy* Material)
{
	for (FMaterialCacheGenericCSMaterialBatch& MaterialBatch : LayerBatch.MaterialBatches)
	{
		if (MaterialBatch.Material == Material)
		{
			return MaterialBatch;
		}
	}

	FMaterialCacheGenericCSMaterialBatch& MaterialBatch = LayerBatch.MaterialBatches.Emplace_GetRef();
	MaterialBatch.Material = Material;
	return MaterialBatch;
}

struct FMaterialCachePageAllocation
{
	uint32 PageIndex;

	bool bAllocated = false;
};

FMaterialCacheGenericCSPrimitiveBatch& MaterialCacheAllocateGenericCSShadePage(FSceneRenderer* Renderer, const FMaterialCacheBlackboardPendingEntry& Entry, const FMaterialCachePendingPageEntry& Page, FMaterialCacheStackEntry StackEntry, const FPrimitiveSceneProxy* PrimitiveSceneProxy, FMaterialCacheGenericCSBatch& RenderData, FMaterialCachePageAllocation PageAllocation)
{
	FMaterialCacheGenericCSMaterialBatch& MaterialBatch = GetOrCreateCSMaterialBatch(RenderData, StackEntry.Material);
	FMaterialCacheGenericCSPrimitiveBatch& PrimitiveBatch = GetOrCreateCSPrimitiveBatch(MaterialBatch, PrimitiveSceneProxy);

	PrimitiveBatch.Pages.Add(PageAllocation.PageIndex);

	RenderData.PageCount++;

	return PrimitiveBatch;
}

static FMaterialCachePrimitiveCachedLayerCommands& GetCachedLayerCommands(FMaterialCachePrimitiveData* PrimitiveData, const FMaterialRenderProxy* RenderProxy)
{
	TUniquePtr<FMaterialCachePrimitiveCachedLayerCommands>& LayerCache = PrimitiveData->CachedCommands.Layers.FindOrAdd(RenderProxy->GetMaterialInterface());

	if (!LayerCache)
	{
		LayerCache = MakeUnique<FMaterialCachePrimitiveCachedLayerCommands>();
	}

	return *LayerCache.Get();
}

void MaterialCacheAllocateNaniteRasterPage(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, const FMaterialCacheBlackboardPendingEntry& Entry, const FMaterialCachePendingPageEntry& Page, FMaterialCacheStackEntry StackEntry, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FPrimitiveSceneInfo* PrimitiveSceneInfo, FMaterialCachePrimitiveData* PrimitiveData, FMaterialCacheNaniteRenderData& RenderData, FMaterialCacheNaniteLayerRenderData& LayerRenderData, FMaterialCachePageAllocation PageAllocation)
{
	FMaterialCacheGenericCSPrimitiveBatch& Batch = MaterialCacheAllocateGenericCSShadePage(Renderer, Entry, Page, StackEntry, PrimitiveSceneProxy, LayerRenderData.GenericCSBatch, PageAllocation);

	if (PageAllocation.bAllocated)
	{
		const int32 NumInstances = PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();

		// Create vis-buffer view for all instances
		for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; InstanceIndex++)
		{
			RenderData.InstanceDraws.Add(Nanite::FInstanceDraw{
				static_cast<uint32>(PrimitiveSceneInfo->GetInstanceSceneDataOffset()) + InstanceIndex,
				PageAllocation.PageIndex
				});
		}
	}

	if (!Batch.ShadingCommand)
	{
		FMaterialCachePrimitiveCachedLayerCommands& LayerCache = GetCachedLayerCommands(PrimitiveData, StackEntry.Material);

		if (!LayerCache.NaniteLayerShadingCommand.IsSet())
		{
			CreateMaterialCacheComputeLayerShadingCommand<FMaterialCacheNaniteShadeCS>(
				*Renderer->Scene,
				PrimitiveSceneProxy,
				StackEntry.Material,
				false,
				GraphBuilder.RHICmdList,
				LayerCache.NaniteLayerShadingCommand.Emplace()
			);
		}

		Batch.ShadingCommand = LayerCache.NaniteLayerShadingCommand.GetPtrOrNull();
	}
}

void MaterialCacheAllocateVertexInvariantPage(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, const FMaterialCacheBlackboardPendingEntry& Entry, const FMaterialCachePendingPageEntry& Page, FMaterialCacheStackEntry StackEntry, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FPrimitiveSceneInfo* PrimitiveSceneInfo, FMaterialCachePrimitiveData* PrimitiveData, FMaterialCacheVertexInvariantLayerRenderData& RenderData, FMaterialCachePageAllocation PageAllocation)
{
	FMaterialCacheGenericCSPrimitiveBatch& Batch = MaterialCacheAllocateGenericCSShadePage(Renderer, Entry, Page, StackEntry, PrimitiveSceneProxy, RenderData.GenericCSBatch, PageAllocation);

	if (!Batch.ShadingCommand)
	{
		FMaterialCachePrimitiveCachedLayerCommands& LayerCache = GetCachedLayerCommands(PrimitiveData, StackEntry.Material);

		if (!LayerCache.VertexInvariantShadingCommand.IsSet())
		{
			CreateMaterialCacheComputeLayerShadingCommand<FMaterialCacheShadeCS>(
				*Renderer->Scene,
				PrimitiveSceneProxy,
				StackEntry.Material,
				false,
				GraphBuilder.RHICmdList,
				LayerCache.VertexInvariantShadingCommand.Emplace()
			);
		}

		Batch.ShadingCommand = LayerCache.VertexInvariantShadingCommand.GetPtrOrNull();
	}
}

static FVector4f GetPageUnwrapMinAndInvSize(const FMaterialCachePageEntry& Page)
{
	return FVector4f{
		Page.UVRect.Min.X,
		Page.UVRect.Min.Y,
		1.0f / (Page.UVRect.Max.X - Page.UVRect.Min.X),
		1.0f / (Page.UVRect.Max.Y - Page.UVRect.Min.Y)
	};
}

void MaterialCacheAllocateHardwareRasterPage(FSceneRenderer* Renderer, const FMaterialCacheBlackboardPendingEntry& Entry, const FMaterialCachePendingPageEntry& Page, FMaterialCacheStackEntry StackEntry, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FPrimitiveSceneInfo* PrimitiveSceneInfo, FMaterialCachePrimitiveData* PrimitiveData, FMaterialCacheHardwareLayerRenderData& RenderData, FMaterialCachePageAllocation PageAllocation)
{
	FMaterialCachePrimitiveCachedLayerCommands& LayerCache = GetCachedLayerCommands(PrimitiveData, StackEntry.Material);

	if (LayerCache.StaticMeshBatchCommands.IsEmpty())
	{
		for (int32 i = 0; i < PrimitiveSceneInfo->StaticMeshes.Num(); i++)
		{
			CreateMaterialCacheStaticLayerDrawCommand(
				*Renderer->Scene,
				PrimitiveSceneProxy,
				StackEntry.Material,
				PrimitiveSceneInfo->StaticMeshes[i],
				LayerCache.StaticMeshBatchCommands.Emplace_GetRef()
			);
		}
	}

	for (const FMaterialCacheMeshDrawCommand& MeshDrawCommand : LayerCache.StaticMeshBatchCommands)
	{
		FVisibleMeshDrawCommand Command;
		Command.Setup(
			&MeshDrawCommand.Command,
			PrimitiveSceneInfo->GetMDCIdInfo(),
			-1,
			MeshDrawCommand.CommandInfo.MeshFillMode,
			MeshDrawCommand.CommandInfo.MeshCullMode,
			MeshDrawCommand.CommandInfo.Flags,
			MeshDrawCommand.CommandInfo.SortKey,
			MeshDrawCommand.CommandInfo.CullingPayload,
			EMeshDrawCommandCullingPayloadFlags::NoScreenSizeCull,
			nullptr,
			0
		);

		FMaterialCacheStaticMeshCommand Cmd;
		Cmd.UnwrapMinAndInvSize = GetPageUnwrapMinAndInvSize(Page.Page);
		Cmd.PageIndex = PageAllocation.PageIndex;

		RenderData.MeshCommands.Add(Cmd);
		RenderData.VisibleMeshCommands.Add(Command);
		RenderData.PrimitiveIds.Add(PrimitiveSceneInfo->GetIndex());
	}
}

static uint32_t AllocateMaterialCacheABufferPage(FMaterialCacheRenderData& RenderData, const FMaterialCachePageEntry& Page)
{
	RenderData.ABuffer.Pages.Add(Page);
	return RenderData.ABuffer.Pages.Num() - 1;
}

static FMaterialCachePageAllocation AllocateMaterialCacheRenderPathPage(FMaterialCacheRenderData& RenderData, FMaterialCachePendingPageEntry& Page, uint32_t EntryIndex, EMaterialCacheRenderPath RenderPath, uint32& PageAllocationSet)
{
	FMaterialCachePageCollection& Collection = RenderData.PageCollections[static_cast<uint32>(RenderPath)];

	uint32 RenderPathMask = 1u << static_cast<uint32>(RenderPath);

	FMaterialCachePageAllocation Allocation;

	if (!(PageAllocationSet & RenderPathMask))
	{
		FMaterialCachePageInfo Info;
		Info.Page = Page.Page;
		Info.ABufferPageIndex = Page.ABufferPageIndex;
		Info.SetupEntryIndex = EntryIndex;
		Collection.Pages.Add(Info);

		Allocation.bAllocated = true;

		PageAllocationSet |= RenderPathMask;
	}

	check(!Collection.Pages.IsEmpty());
	Allocation.PageIndex = Collection.Pages.Num() - 1;

	return Allocation;
}

void CreatePageIndirectionBuffer(FRDGBuilder& GraphBuilder, FMaterialCacheGenericCSBatch& Batch)
{
	FRDGUploadData<uint32> PageIndirectionsData(GraphBuilder, Batch.PageCount);

	uint32 IndirectionOffset = 0;

	for (FMaterialCacheGenericCSMaterialBatch& MaterialBatch : Batch.MaterialBatches)
	{
		for (FMaterialCacheGenericCSPrimitiveBatch& PrimitiveBatch : MaterialBatch.PrimitiveBatches)
		{
			PrimitiveBatch.PageIndirectionOffset = IndirectionOffset;
			FMemory::Memcpy(&PageIndirectionsData[IndirectionOffset], PrimitiveBatch.Pages.GetData(), PrimitiveBatch.Pages.NumBytes());
			IndirectionOffset += PrimitiveBatch.Pages.Num();
		}
	}

	check(IndirectionOffset == Batch.PageCount);

	Batch.PageIndirectionBuffer = CreateUploadBuffer(
		GraphBuilder,
		TEXT("MaterialCache.PageIndirection"),
		sizeof(uint32_t), PageIndirectionsData.Num(),
		PageIndirectionsData
	);
}

static const FMaterialRenderProxy* GetMaterialCacheDefaultMaterial(const FPrimitiveSceneProxy* Proxy, const FPrimitiveSceneInfo* SceneInfo)
{
	// TODO: Support multiple sections for default path
	
	if (Proxy->IsNaniteMesh())
	{
		const auto* NaniteProxy = static_cast<const Nanite::FSceneProxy*>(Proxy);
		
		if (NaniteProxy->GetMaterialSections().IsEmpty())
		{
			return nullptr;
		}
		
		return NaniteProxy->GetMaterialSections()[0].ShadingMaterialProxy;
	}
	else
	{
		if (SceneInfo->StaticMeshes.IsEmpty())
		{
			return nullptr;
		}
		
		return SceneInfo->StaticMeshes[0].MaterialRenderProxy;
	}
}

static void MaterialCacheAllocateAndBatchPages(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, FMaterialCacheSceneExtension& SceneExtension, FMaterialCacheBlackboardData& Data)
{
	FMaterialCacheRenderData& RenderData = Data.RenderData;

	for (int32 EntryIndex = 0; EntryIndex < Data.PendingEntries.Num(); EntryIndex++)
	{
		FMaterialCacheBlackboardPendingEntry& Entry = Data.PendingEntries[EntryIndex];

		const FPrimitiveSceneProxy* PrimitiveSceneProxy = SceneExtension.GetSceneProxy(Entry.Setup.PrimitiveComponentId);
		if (!PrimitiveSceneProxy)
		{
			UE_LOG(LogRenderer, Error, TEXT("Failed to get primitive scene proxy"));
			continue;
		}

		const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
		if (!PrimitiveSceneInfo)
		{
			UE_LOG(LogRenderer, Error, TEXT("Failed to get primitive scene info"));
			continue;
		}
		
		FMaterialCachePrimitiveData* PrimitiveData = SceneExtension.GetPrimitiveData(Entry.Setup.PrimitiveComponentId);
		if (!PrimitiveData)
		{
			UE_LOG(LogRenderer, Error, TEXT("Failed to get primitive data"));
			continue;
		}

		// If caching is disabled, always rebuild
		if (!GMaterialCacheComamndCaching)
		{
			PrimitiveData->CachedCommands = {};
		}

		UMaterialCacheStackProvider* Provider = PrimitiveData->Provider.StackProvider.Get();

		for (FMaterialCachePendingPageEntry& Page : Entry.Pages)
		{
			Page.ABufferPageIndex = AllocateMaterialCacheABufferPage(RenderData, Page.Page);

			// Providers are optional, if none is supplied, just assume the primary material as a stack entry
			FMaterialCacheStack Stack;
			if (Provider)
			{
				Provider->Evaluate(Page.Page.UVRect, &Stack);

				// Do not produce pages for empty stacks
				if (Stack.Stack.IsEmpty())
				{
					continue;
				}
			}
			else
			{
				FMaterialCacheStackEntry StackEntry;
				StackEntry.Material = GetMaterialCacheDefaultMaterial(PrimitiveSceneProxy, PrimitiveSceneInfo);
				Stack.Stack.Add(StackEntry);
			}
			
			if (Stack.Stack.Num() > RenderData.Layers.Num())
			{
				RenderData.Layers.SetNum(Stack.Stack.Num());
			}

			uint32 PageAllocationSet = 0x0;

			for (int32 StackIndex = 0; StackIndex < Stack.Stack.Num(); StackIndex++)
			{
				const FMaterialCacheStackEntry& StackEntry = Stack.Stack[StackIndex];

				if (!StackEntry.Material)
				{
					UE_LOG(LogRenderer, Error, TEXT("Invalid stack entry"));
					continue;
				}

				FMaterialCacheLayerRenderData& Layer = RenderData.Layers[StackIndex];

				EMaterialCacheRenderPath RenderPath = GetMaterialCacheRenderPath(Renderer, PrimitiveSceneProxy, StackEntry);

				const FMaterialCachePageAllocation RenderPathPageIndex = AllocateMaterialCacheRenderPathPage(RenderData, Page, EntryIndex, RenderPath, PageAllocationSet);
				
				switch (RenderPath)
				{
				default:
					checkNoEntry();
					break;
				case EMaterialCacheRenderPath::HardwareRaster:
					MaterialCacheAllocateHardwareRasterPage(Renderer, Entry, Page, StackEntry, PrimitiveSceneProxy, PrimitiveSceneInfo, PrimitiveData, Layer.Hardware, RenderPathPageIndex);
					break;
				case EMaterialCacheRenderPath::NaniteRaster:
					MaterialCacheAllocateNaniteRasterPage(Renderer, GraphBuilder, Entry, Page, StackEntry, PrimitiveSceneProxy, PrimitiveSceneInfo, PrimitiveData, RenderData.Nanite, Layer.Nanite, RenderPathPageIndex);
					break;
				case EMaterialCacheRenderPath::VertexInvariant:
					MaterialCacheAllocateVertexInvariantPage(Renderer, GraphBuilder, Entry, Page, StackEntry, PrimitiveSceneProxy, PrimitiveSceneInfo, PrimitiveData, Layer.VertexInvariant, RenderPathPageIndex);
					break;
				}
			}
		}
	}

	for (FMaterialCacheLayerRenderData& LayerRenderData : RenderData.Layers)
	{
		CreatePageIndirectionBuffer(GraphBuilder, LayerRenderData.Nanite.GenericCSBatch);
		CreatePageIndirectionBuffer(GraphBuilder, LayerRenderData.VertexInvariant.GenericCSBatch);
	}
}

static FIntPoint GetMaterialCacheTileSize()
{
	static uint32 Width = GetMaterialCacheTileWidth();
	return FIntPoint(Width, Width);
}

static void MaterialCacheCreateABuffer(FRDGBuilder& GraphBuilder, FMaterialCacheRenderData& RenderData)
{
	FIntPoint TileSize = GetMaterialCacheTileSize();

	TArray<EPixelFormat, TInlineAllocator<MaterialCacheMaxABuffers>> Formats;
	GetMaterialCacheABufferFormats({}, Formats);

	ETextureCreateFlags Flags =
		ETextureCreateFlags::ShaderResource | 
		ETextureCreateFlags::UAV |
		ETextureCreateFlags::TargetArraySlicesIndependently |
		ETextureCreateFlags::RenderTargetable;

	FRDGTextureDesc Desc;
	if (GRHISupportsArrayIndexFromAnyShader && GMaterialCacheStaticMeshEnableViewportFromVS)
	{
		Desc = FRDGTextureDesc::Create2DArray(
			TileSize,
			PF_Unknown,
			FClearValueBinding::Black,
			Flags,
			RenderData.ABuffer.Pages.Num()
		);

		RenderData.ABuffer.Layout = EMaterialCacheABufferTileLayout::Sliced;
	}
	else
	{
		// TODO[MP]: This needs to be atlassed instead, we do have size limitations...
		Desc = FRDGTextureDesc::Create2DArray(
			TileSize * FIntPoint(RenderData.ABuffer.Pages.Num(), 1),
			PF_Unknown,
			FClearValueBinding::Black,
			Flags,
			1
		);

		RenderData.ABuffer.Layout = EMaterialCacheABufferTileLayout::Horizontal;
	}

	// Must have static lifetimes
	static const TCHAR* ABufferNames[] = {
		TEXT("MaterialCacheABuffer0"),
		TEXT("MaterialCacheABuffer1"),
		TEXT("MaterialCacheABuffer2"),
	};

	for (int32 ABufferIndex = 0; ABufferIndex < Formats.Num(); ABufferIndex++)
	{
		Desc.Format = Formats[ABufferIndex];
		RenderData.ABuffer.ABufferTextures.Add(GraphBuilder.CreateTexture(Desc, ABufferNames[ABufferIndex]));
	}

	FRDGTextureClearInfo TextureClearInfo;
	TextureClearInfo.ClearColor = FLinearColor(0, 0, 0, 0);
	TextureClearInfo.NumSlices  = Desc.ArraySize;

	// TODO[MP]: This is a clear per-slice, which is inefficient
	// There should be something better somewhere
	AddClearRenderTargetPass(GraphBuilder, RenderData.ABuffer.ABufferTextures[0], TextureClearInfo);
	AddClearRenderTargetPass(GraphBuilder, RenderData.ABuffer.ABufferTextures[1], TextureClearInfo);
	AddClearRenderTargetPass(GraphBuilder, RenderData.ABuffer.ABufferTextures[2], TextureClearInfo);
}

static FUintVector3 GetMaterialCacheABufferTilePhysicalLocation(const FMaterialCacheRenderData& RenderData, uint32_t ABufferPageIndex)
{
	const FIntPoint TileSize = GetMaterialCacheTileSize();

	switch (RenderData.ABuffer.Layout)
	{
	default:
		checkNoEntry();
		return {};
	case EMaterialCacheABufferTileLayout::Horizontal:
		return FUintVector3(TileSize.X * ABufferPageIndex, 0, 0);
	case EMaterialCacheABufferTileLayout::Sliced:
		return FUintVector3(0, 0, ABufferPageIndex);
	}
}

static void GetShadingBinData(const FMaterialCacheBlackboardData& Data, FMaterialCacheSceneExtension& SceneExtension, const FMaterialCachePageCollection& Collection, FRDGUploadData<UE::HLSL::FMaterialCacheBinData>& Out, const FIntPoint& TileSize)
{
	for (int32 PageIndex = 0; PageIndex < Collection.Pages.Num(); PageIndex++)
	{
		const FMaterialCachePageInfo& Info = Collection.Pages[PageIndex];

		UE::HLSL::FMaterialCacheBinData& BinData = Out[PageIndex];

		BinData.ABufferPhysicalPosition = GetMaterialCacheABufferTilePhysicalLocation(Data.RenderData, Info.ABufferPageIndex);

		BinData.UVMinAndInvSize = FVector4f{
			Info.Page.UVRect.Min.X,
			Info.Page.UVRect.Min.Y,
			1.0f / (Info.Page.UVRect.Max.X - Info.Page.UVRect.Min.X),
			1.0f / (Info.Page.UVRect.Max.Y - Info.Page.UVRect.Min.Y)
		};

		FVector2f UVRange = Info.Page.UVRect.Max - Info.Page.UVRect.Min;
		BinData.UVMinAndThreadAdvance = FVector4f(
			Info.Page.UVRect.Min,
			FVector2f(1.0f / TileSize.X, 1.0f / TileSize.Y) * UVRange
		);

		const FMaterialCacheBlackboardPendingEntry& Entry = Data.PendingEntries[Info.SetupEntryIndex];

		if (const FPrimitiveSceneProxy* PrimitiveSceneProxy = SceneExtension.GetSceneProxy(Entry.Setup.PrimitiveComponentId))
		{
			BinData.PrimitiveData = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetPersistentIndex().Index;
		}
	}
}

static void MaterialCacheSetupHardwareContext(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, FMaterialCacheSceneExtension& SceneExtension, FMaterialCacheBlackboardData& Data, FMaterialCacheRenderData& RenderData, FMaterialCacheHardwareContext& Context)
{
	const FMaterialCachePageCollection& Collection = RenderData.PageCollections[static_cast<uint32>(EMaterialCacheRenderPath::HardwareRaster)];

	if (Collection.Pages.IsEmpty())
	{
		return;
	}
	
	const FIntPoint TileSize = GetMaterialCacheTileSize();

	// All shading data, one per page
	FRDGUploadData<UE::HLSL::FMaterialCacheBinData> ShadingDataArray(GraphBuilder, Collection.Pages.Num());
	GetShadingBinData(Data, SceneExtension, Collection, ShadingDataArray, TileSize);

	FRDGBufferRef ShadingBinData = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(FUintVector4), ShadingDataArray.NumBytes() / sizeof(FUintVector4)),
		TEXT("MaterialCache.ShadingBinData")
	);
	
	GraphBuilder.QueueBufferUpload(ShadingBinData, ShadingDataArray.GetData(), ShadingDataArray.NumBytes(), ERDGInitialDataFlags::None);

	FMaterialCacheUniformParameters* PassUniformParameters = GraphBuilder.AllocParameters<FMaterialCacheUniformParameters>();
	PassUniformParameters->ShadingBinData = GraphBuilder.CreateSRV(ShadingBinData, PF_R32G32B32A32_UINT);
	PassUniformParameters->SvPagePositionModMask = GetMaterialCacheTileWidth() - 1u;
	SetupSceneTextureUniformParameters(GraphBuilder, &Renderer->GetActiveSceneTextures(), Renderer->Scene->GetFeatureLevel(), ESceneTextureSetupMode::None, PassUniformParameters->SceneTextures);

	Context.PassUniformParameters = PassUniformParameters;
}

static FUintVector4 GetMaterialCacheABufferTilePhysicalViewport(const FMaterialCacheRenderData& RenderData, uint32_t ABufferPageIndex)
{
	const FIntPoint TileSize = GetMaterialCacheTileSize();

	switch (RenderData.ABuffer.Layout)
	{
	default:
		checkNoEntry();
		return {};
	case EMaterialCacheABufferTileLayout::Horizontal:
		return FUintVector4(
			TileSize.X * ABufferPageIndex, 0,
			TileSize.X * (ABufferPageIndex + 1), TileSize.Y
		);
	case EMaterialCacheABufferTileLayout::Sliced:
		return FUintVector4(0, 0, TileSize.X, TileSize.Y);
	}
}

static void MaterialCacheRenderHardwarePages(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, FMaterialCacheRenderData& RenderData, FMaterialCacheLayerRenderData& LayerRenderData, FMaterialCacheHardwareContext& Context, uint32 LayerBatchIndex)
{
	const FMaterialCachePageCollection& Collection = RenderData.PageCollections[static_cast<uint32>(EMaterialCacheRenderPath::HardwareRaster)];

	if (Collection.Pages.IsEmpty())
	{
		return;
	}
	
	const bool bUseArrayTargetablePages = GRHISupportsArrayIndexFromAnyShader && GMaterialCacheStaticMeshEnableViewportFromVS;

	const FIntPoint TileSize = GetMaterialCacheTileSize();

	FInstanceCullingResult   InstanceCullingResult;
	FInstanceCullingContext* InstanceCullingContext = nullptr;
	FRHIBuffer* PrimitiveIdVertexBuffer = nullptr;

	if (Renderer->Scene->GPUScene.IsEnabled())
	{
		InstanceCullingContext = GraphBuilder.AllocObject<FInstanceCullingContext>(
			TEXT("FInstanceCullingContext"),
			Renderer->Views[0].GetShaderPlatform(),
			nullptr,
			TArrayView<const int32>(&Renderer->Views[0].SceneRendererPrimaryViewId, 1),
			nullptr
		);

		int32 MaxInstances = 0;
		int32 VisibleMeshDrawCommandsNum = 0;
		int32 NewPassVisibleMeshDrawCommandsNum = 0;

		InstanceCullingContext->SetupDrawCommands(
			LayerRenderData.Hardware.VisibleMeshCommands,
			false,
			Renderer->Scene,
			MaxInstances,
			VisibleMeshDrawCommandsNum,
			NewPassVisibleMeshDrawCommandsNum
		);

		InstanceCullingContext->BuildRenderingCommands(
			GraphBuilder,
			Renderer->Scene->GPUScene,
			Renderer->Views[0].DynamicPrimitiveCollector.GetInstanceSceneDataOffset(),
			Renderer->Views[0].DynamicPrimitiveCollector.NumInstances(),
			InstanceCullingResult
		);
	}
	else
	{
		const uint32 PrimitiveIdBufferDataSize = LayerRenderData.Hardware.PrimitiveIds.Num() * sizeof(int32);

		FPrimitiveIdVertexBufferPoolEntry Entry = GPrimitiveIdVertexBufferPool.Allocate(GraphBuilder.RHICmdList, PrimitiveIdBufferDataSize);
		PrimitiveIdVertexBuffer = Entry.BufferRHI;

		// Copy over primitive ids
		void* RESTRICT PrimitiveData = GraphBuilder.RHICmdList.LockBuffer(PrimitiveIdVertexBuffer, 0, PrimitiveIdBufferDataSize, RLM_WriteOnly);
		FMemory::Memcpy(PrimitiveData, LayerRenderData.Hardware.PrimitiveIds.GetData(), PrimitiveIdBufferDataSize);
		GraphBuilder.RHICmdList.UnlockBuffer(PrimitiveIdVertexBuffer);

		GPrimitiveIdVertexBufferPool.ReturnToFreeList(Entry);
	}

	FMaterialCacheRastShadeParameters* MeshPassParameters = GraphBuilder.AllocParameters<FMaterialCacheRastShadeParameters>();
	MeshPassParameters->View = GraphBuilder.CreateUniformBuffer(GraphBuilder.AllocParameters(Renderer->Views[0].CachedViewUniformShaderParameters.Get()));
	MeshPassParameters->Pass = GraphBuilder.CreateUniformBuffer(Context.PassUniformParameters);
	MeshPassParameters->Scene = Renderer->Views[0].GetSceneUniforms().GetBuffer(GraphBuilder);
	InstanceCullingResult.GetDrawParameters(MeshPassParameters->InstanceCullingDrawParams);

	// Blend mode for development
	uint32 Flags = UE::HLSL::MatCache_None;
	if (!LayerBatchIndex)
	{
		Flags |= UE::HLSL::MatCache_DefaultBottomLayer;
	}

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Hardware Batch (%u pages)", Collection.Pages.Num()),
		MeshPassParameters,
		ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
		[
			bUseArrayTargetablePages, Flags,
			Renderer, PrimitiveIdVertexBuffer, TileSize, MeshPassParameters, InstanceCullingContext, &LayerRenderData, &Collection, &RenderData
		](FRDGAsyncTask, FRHICommandList& RHICmdList) mutable
		{
			FMeshDrawCommandStateCache StateCache;

			FMeshDrawCommandOverrideArgs OverrideArgs = GetMeshDrawCommandOverrideArgs(MeshPassParameters->InstanceCullingDrawParams);

			FMeshDrawCommandSceneArgs SceneArgs;
			
			if (IsUniformBufferStaticSlotValid(InstanceCullingContext->InstanceCullingStaticSlot))
			{
				if (InstanceCullingContext->bUsesUniformBufferView)
				{
					SceneArgs.BatchedPrimitiveSlot = InstanceCullingContext->InstanceCullingStaticSlot;
				}

				RHICmdList.SetStaticUniformBuffer(InstanceCullingContext->InstanceCullingStaticSlot, OverrideArgs.InstanceCullingStaticUB);
			}
			
			// TODO: Borders
			if (bUseArrayTargetablePages)
			{
				RHICmdList.SetViewport(0, 0, 0, TileSize.X, TileSize.Y, 1.0f);
			}

			for (int32 CommandIndex = 0; CommandIndex < LayerRenderData.Hardware.MeshCommands.Num(); CommandIndex++)
			{
				const FMaterialCacheStaticMeshCommand& Command = LayerRenderData.Hardware.MeshCommands[CommandIndex];

				const FMaterialCachePageInfo& PageInfo = Collection.Pages[Command.PageIndex];

				if (!bUseArrayTargetablePages)
				{
					const FUintVector4 Viewport = GetMaterialCacheABufferTilePhysicalViewport(RenderData, PageInfo.ABufferPageIndex);
					RHICmdList.SetViewport(
						Viewport.X, Viewport.Y, 0,
						Viewport.Z, Viewport.W, 1.0f
					);
				}

				FGraphicsMinimalPipelineStateSet GraphicsMinimalPipelineStateSet;

				check(GRHISupportsShaderRootConstants);
				SceneArgs.RootConstants = FUintVector4(
					Command.PageIndex,
					PageInfo.ABufferPageIndex,
					static_cast<uint32>(Flags),
					0
				);

				SceneArgs.PrimitiveIdOffset = CommandIndex * FInstanceCullingContext::GetInstanceIdBufferStride(Renderer->Scene->GetShaderPlatform());

				if (Renderer->Scene->GPUScene.IsEnabled())
				{
					const FInstanceCullingContext::FMeshDrawCommandInfo& DrawCommandInfo = InstanceCullingContext->MeshDrawCommandInfos[CommandIndex];

					SceneArgs.IndirectArgsByteOffset = 0u;
					SceneArgs.IndirectArgsBuffer = nullptr;

					if (DrawCommandInfo.bUseIndirect)
					{
						SceneArgs.IndirectArgsByteOffset = OverrideArgs.IndirectArgsByteOffset + DrawCommandInfo.IndirectArgsOffsetOrNumInstances;
						SceneArgs.IndirectArgsBuffer = OverrideArgs.IndirectArgsBuffer;
					}

					SceneArgs.PrimitiveIdOffset = OverrideArgs.InstanceDataByteOffset + DrawCommandInfo.InstanceDataByteOffset;
					SceneArgs.PrimitiveIdsBuffer = OverrideArgs.InstanceBuffer;

					FMeshDrawCommand::SubmitDraw(
						*LayerRenderData.Hardware.VisibleMeshCommands[CommandIndex].MeshDrawCommand,
						GraphicsMinimalPipelineStateSet,
						SceneArgs,
						1,
						RHICmdList,
						StateCache
					);
				}
				else
				{
					SceneArgs.PrimitiveIdsBuffer = PrimitiveIdVertexBuffer;

					FMeshDrawCommand::SubmitDraw(
						*LayerRenderData.Hardware.VisibleMeshCommands[CommandIndex].MeshDrawCommand,
						GraphicsMinimalPipelineStateSet,
						SceneArgs,
						1,
						RHICmdList,
						StateCache
					);
				}
			}
		});
}

static void MaterialCacheRenderNanitePages(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, FMaterialCacheBlackboardData& Data, FMaterialCacheRenderData& RenderData, FMaterialCacheLayerRenderData& LayerRenderData, FMaterialCacheNaniteContext& Context, uint32 LayerBatchIndex)
{
	const FIntPoint TileSize = GetMaterialCacheTileSize();

	const FMaterialCachePageCollection& Collection = RenderData.PageCollections[static_cast<uint32>(EMaterialCacheRenderPath::NaniteRaster)];

	if (Collection.Pages.IsEmpty())
	{
		return;
	}
	
	FMaterialCacheNaniteStackShadeParameters* Params = GraphBuilder.AllocParameters<FMaterialCacheNaniteStackShadeParameters>();
	Params->Shade = *Context.PassShadeParameters;
	Params->PageIndirections = GraphBuilder.CreateSRV(LayerRenderData.Nanite.GenericCSBatch.PageIndirectionBuffer, PF_R32_UINT);
	Params->Pass = GraphBuilder.CreateUniformBuffer(Context.PassUniformParameters);
	
	// Blend mode for development
	uint32 Flags = UE::HLSL::MatCache_None;
	if (!LayerBatchIndex)
	{
		Flags |= UE::HLSL::MatCache_DefaultBottomLayer;
	}

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Nanite Batch (%u pages)", Collection.Pages.Num()),
		Params,
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[
			Flags, TileSize, Params, &LayerRenderData
		](FRHICommandList& RHICmdList) mutable
		{
			// Subsequent batches can run in parallel without issue
			for (FMaterialCacheGenericCSMaterialBatch& MaterialBatch : LayerRenderData.Nanite.GenericCSBatch.MaterialBatches)
			{
				for (FMaterialCacheGenericCSPrimitiveBatch& PrimitiveBatch : MaterialBatch.PrimitiveBatches)
				{
					auto Shader = TShaderRef<FMaterialCacheNaniteShadeCS>::Cast(PrimitiveBatch.ShadingCommand->ComputeShader);

					if (!Shader.IsValid())
					{
						UE_LOG(LogRenderer, Error, TEXT("Invalid shading command"));
						continue;
					}

					SetComputePipelineState(RHICmdList, Shader.GetComputeShader());

					FUintVector4 RootData;
					RootData.X = PrimitiveBatch.PageIndirectionOffset;
					RootData.Y = 0;
					RootData.Z = static_cast<uint32>(ENaniteMeshPass::MaterialCache);
					RootData.W = static_cast<uint32>(Flags);

					// Bind parameters
					FRHIBatchedShaderParameters& ShadingParameters = RHICmdList.GetScratchShaderParameters();
					PrimitiveBatch.ShadingCommand->ShaderBindings.SetParameters(ShadingParameters);
					Shader->SetPassParameters(ShadingParameters, RootData, Params->PageIndirections->GetRHI());
					RHICmdList.SetBatchedShaderParameters(Shader.GetComputeShader(), ShadingParameters);

					// TODO: Case with no root support
					check(GRHISupportsShaderRootConstants);
					RHICmdList.SetShaderRootConstants(RootData);

					// Dispatch the bin over all pages
					RHICmdList.DispatchComputeShader(
						FMath::DivideAndRoundUp(TileSize.X * TileSize.Y, 64),
						PrimitiveBatch.Pages.Num(),
						1
					);
				}
			}
		});
}

static void MaterialCacheSetupVertexInvariantContext(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, FMaterialCacheSceneExtension& SceneExtension, FMaterialCacheBlackboardData& Data, FMaterialCacheRenderData& RenderData, FMaterialCacheVertexInvariantContext& Context)
{
	const FIntPoint TileSize = GetMaterialCacheTileSize();

	const FMaterialCachePageCollection& Collection = RenderData.PageCollections[static_cast<uint32>(EMaterialCacheRenderPath::VertexInvariant)];

	if (Collection.Pages.IsEmpty())
	{
		return;
	}

	// All shading data, one per page
	FRDGUploadData<UE::HLSL::FMaterialCacheBinData> ShadingDataArray(GraphBuilder, Collection.Pages.Num());
	GetShadingBinData(Data, SceneExtension, Collection, ShadingDataArray, TileSize);

	FRDGBufferRef ShadingBinData = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("MaterialCache.ShadingBinData"),
		sizeof(UE::HLSL::FMaterialCacheBinData),
		ShadingDataArray.Num(), ShadingDataArray.GetData(),
		ShadingDataArray.NumBytes()
	);

	FMaterialCacheUniformParameters* PassUniformParameters = GraphBuilder.AllocParameters<FMaterialCacheUniformParameters>();
	PassUniformParameters->ShadingBinData = GraphBuilder.CreateSRV(ShadingBinData);
	SetupSceneTextureUniformParameters(GraphBuilder, &Renderer->GetActiveSceneTextures(), Renderer->Scene->GetFeatureLevel(), ESceneTextureSetupMode::None, PassUniformParameters->SceneTextures);

	Context.PassUniformParameters = PassUniformParameters;
}

static void MaterialCacheRenderVertexInvariantPages(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, FMaterialCacheBlackboardData& Data, FMaterialCacheRenderData& RenderData, FMaterialCacheLayerRenderData& LayerRenderData, FMaterialCacheVertexInvariantContext& Context, uint32 LayerBatchIndex)
{
	const FIntPoint TileSize = GetMaterialCacheTileSize();

	const FMaterialCachePageCollection& Collection = RenderData.PageCollections[static_cast<uint32>(EMaterialCacheRenderPath::VertexInvariant)];

	if (Collection.Pages.IsEmpty())
	{
		return;
	}
	
	FMaterialCacheCSStackShadeParameters* Params = GraphBuilder.AllocParameters<FMaterialCacheCSStackShadeParameters>();
	Params->View = Renderer->Views[0].GetShaderParameters();
	Params->Pass = GraphBuilder.CreateUniformBuffer(Context.PassUniformParameters);
	Params->Scene = Renderer->Views[0].GetSceneUniforms().GetBuffer(GraphBuilder);
	Params->PageIndirections = GraphBuilder.CreateSRV(LayerRenderData.VertexInvariant.GenericCSBatch.PageIndirectionBuffer, PF_R32_UINT);
	
	// Blend mode for development
	uint32 Flags = UE::HLSL::MatCache_None;
	if (!LayerBatchIndex)
	{
		Flags |= UE::HLSL::MatCache_DefaultBottomLayer;
	}
	
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Vertex-Invariant Batch (%u)", Collection.Pages.Num()),
		Params,
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[
			&LayerRenderData, Flags, TileSize, Params
		](FRHICommandList& RHICmdList) mutable
		{
			// Subsequent batches can run in parallel without issue
			for (const FMaterialCacheGenericCSMaterialBatch& MaterialBatch : LayerRenderData.VertexInvariant.GenericCSBatch.MaterialBatches)
			{
				for (const FMaterialCacheGenericCSPrimitiveBatch& PrimitiveBatch : MaterialBatch.PrimitiveBatches)
				{
					auto Shader = TShaderRef<FMaterialCacheShadeCS>::Cast(PrimitiveBatch.ShadingCommand->ComputeShader);
					if (!Shader.IsValid())
					{
						UE_LOG(LogRenderer, Error, TEXT("Invalid shading command"));
						continue;
					}

					SetComputePipelineState(RHICmdList, Shader.GetComputeShader());

					FUintVector4 RootData;
					RootData.X = PrimitiveBatch.PageIndirectionOffset;
					RootData.Y = static_cast<uint32>(Flags);
					RootData.Z = 0;
					RootData.W = 0;

					// Bind parameters
					FRHIBatchedShaderParameters& ShadingParameters = RHICmdList.GetScratchShaderParameters();
					PrimitiveBatch.ShadingCommand->ShaderBindings.SetParameters(ShadingParameters);
					Shader->SetPassParameters(ShadingParameters, RootData, Params->PageIndirections->GetRHI());
					RHICmdList.SetBatchedShaderParameters(Shader.GetComputeShader(), ShadingParameters);

					// TODO: Case with no root support
					check(GRHISupportsShaderRootConstants);
					RHICmdList.SetShaderRootConstants(RootData);

					// Dispatch the bin over all pages
					RHICmdList.DispatchComputeShader(
						FMath::DivideAndRoundUp(TileSize.X * TileSize.Y, 64),
						PrimitiveBatch.Pages.Num(),
						1
					);
				}
			}
		}
	);
}

static void GetNaniteRectArray(const FMaterialCachePageCollection& Collection, const FIntPoint& TileSize, FRDGUploadData<FUintVector4>& Out)
{
	for (int32 PageIndex = 0; PageIndex < Collection.Pages.Num(); PageIndex++)
	{
		Out[PageIndex] = FUintVector4(
			TileSize.X * PageIndex,
			0,
			TileSize.X * (PageIndex + 1),
			TileSize.Y
		);
	}
}

static void MaterialCacheSetupNaniteContext(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, FMaterialCacheSceneExtension& SceneExtension, FMaterialCacheBlackboardData& Data, FMaterialCacheRenderData& RenderData, FMaterialCacheNaniteContext& Context)
{
	const FIntPoint TileSize = GetMaterialCacheTileSize();
	
	const FMaterialCachePageCollection& Collection = RenderData.PageCollections[static_cast<uint32>(EMaterialCacheRenderPath::NaniteRaster)];

	if (Collection.Pages.IsEmpty())
	{
		return;
	}

	// TODO[MP]: Just need to split up the batches
	checkf(Collection.Pages.Num() <= NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS, TEXT("Pending support for > 128 pages per frame"));

	// Wait for all bins to finish
	Renderer->Scene->WaitForCacheNaniteMaterialBinsTask();

	// TODO[MP]: With the layering, we probably don't need this
	Nanite::BuildShadingCommands(
		GraphBuilder,
		*Renderer->Scene,
		ENaniteMeshPass::MaterialCache,
		RenderData.Nanite.ShadingCommands,
		Nanite::EBuildShadingCommandsMode::Custom
	);

	// Create a view per page, we render all views laid out horizontally across the vis-buffer
	Nanite::FPackedViewArray* NaniteViews = Nanite::FPackedViewArray::CreateWithSetupTask(
		GraphBuilder,
		Collection.Pages.Num(),
		[&Data, TileSize, Renderer, &Collection](Nanite::FPackedViewArray::ArrayType& OutViews)
		{
			const FMatrix ProjectionMatrix = FReversedZOrthoMatrix(
				0, TileSize.X,
				0, TileSize.Y,
				1.0f,
				0
			);

			FViewMatrices::FMinimalInitializer Initializer;
			Initializer.ViewRotationMatrix = FMatrix::Identity;
			Initializer.ViewOrigin = FVector::Zero();
			Initializer.ProjectionMatrix = ProjectionMatrix;
			Initializer.ConstrainedViewRect = Renderer->Views[0].SceneViewInitOptions.GetConstrainedViewRect();
			Initializer.StereoPass = Renderer->Views[0].SceneViewInitOptions.StereoPass;
			auto ViewMatrices = FViewMatrices(Initializer);

			Nanite::FPackedViewParams Params;
			Params.ViewMatrices = ViewMatrices;
			Params.PrevViewMatrices = ViewMatrices;
			Params.RasterContextSize = FIntPoint(TileSize.X * Collection.Pages.Num(), TileSize.Y);
			Params.Flags = 0x0;
			Params.StreamingPriorityCategory = 3;
			Params.MinBoundsRadius = 0;
			Params.ViewLODDistanceFactor = Renderer->Views[0].LODDistanceFactor;
			Params.HZBTestViewRect = Renderer->Views[0].PrevViewInfo.ViewRect;
			Params.MaxPixelsPerEdgeMultipler = 1.0f;
			Params.GlobalClippingPlane = Renderer->Views[0].GlobalClippingPlane;
			Params.SceneRendererPrimaryViewId = Renderer->Views[0].SceneRendererPrimaryViewId;

			uint32_t PageOffset = 0;

			for (const FMaterialCacheBlackboardPendingEntry& PendingEntry : Data.PendingEntries)
			{
				for (const FMaterialCachePendingPageEntry& Page : PendingEntry.Pages)
				{
					Params.ViewRect = FIntRect(
						TileSize.X * PageOffset,
						0,
						TileSize.X * (PageOffset + 1),
						TileSize.Y
					);

					Nanite::FPackedView View = Nanite::CreatePackedView(Params);

					View.MaterialCacheUnwrapMinAndInvSize = FVector4f(
						Page.Page.UVRect.Min.X,
						Page.Page.UVRect.Min.Y,
						1.0f / (Page.Page.UVRect.Max.X - Page.Page.UVRect.Min.X),
						1.0f / (Page.Page.UVRect.Max.Y - Page.Page.UVRect.Min.Y)
					);

					View.MaterialCachePageAdvanceAndInvCount = FVector4f(
						PageOffset / static_cast<float>(Collection.Pages.Num()),
						1.0f / Collection.Pages.Num()
					);

					OutViews.Add(MoveTemp(View));

					PageOffset++;
				}
			}
		});

	// Rasterization view rectangles, one per page
	FRDGUploadData<FUintVector4> RasterRectArray(GraphBuilder, Collection.Pages.Num());
	GetNaniteRectArray(Collection, TileSize, RasterRectArray);

	// All shading data, one per page
	FRDGUploadData<UE::HLSL::FMaterialCacheBinData> ShadingDataArray(GraphBuilder, Collection.Pages.Num());
	GetShadingBinData(Data, SceneExtension, Collection, ShadingDataArray, TileSize);

	FRDGBufferRef RasterRectBuffer = CreateUploadBuffer(
		GraphBuilder,
		TEXT("MaterialCache.Rects"),
		sizeof(FUintVector4), FMath::RoundUpToPowerOfTwo(RasterRectArray.Num()),
		RasterRectArray
	);

	FRDGBuffer* PackedViewBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("MaterialCache.PackedViews"),
		NaniteViews->GetViews().GetTypeSize(),
		NaniteViews->NumViews,
		NaniteViews->GetViews().GetData(),
		NaniteViews->GetViews().NumBytes()
	);

	FRDGBuffer* ShadingBinData = CreateByteAddressBuffer(
		GraphBuilder,
		TEXT("MaterialCache.ShadingBinData"),
		ShadingDataArray.NumBytes(), ShadingDataArray.GetData(),
		ShadingDataArray.NumBytes()
	);

	Nanite::FSharedContext SharedContext{};
	SharedContext.FeatureLevel = Renderer->Scene->GetFeatureLevel();
	SharedContext.ShaderMap = GetGlobalShaderMap(SharedContext.FeatureLevel);
	SharedContext.Pipeline = Nanite::EPipeline::MaterialCache;

	// Create context, tile all pages horizontally
	Nanite::FRasterContext RasterContext = Nanite::InitRasterContext(
		GraphBuilder,
		SharedContext,
		Renderer->ViewFamily,
		FIntPoint(TileSize.X * Collection.Pages.Num(), TileSize.Y),
		FIntRect(0, 0, TileSize.X * Collection.Pages.Num(), TileSize.Y),
		Nanite::EOutputBufferMode::VisBuffer,
		true,
		false,
		GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RasterRectBuffer, PF_R32G32B32A32_UINT)),
		Collection.Pages.Num()
	);

	// Setup object space config
	Nanite::FConfiguration CullingConfig = { 0 };
	CullingConfig.SetViewFlags(Renderer->Views[0]);
	CullingConfig.bIsMaterialCache = true;
	CullingConfig.bForceHWRaster = true;
	CullingConfig.bUpdateStreaming = true;

	TUniquePtr<Nanite::IRenderer> NaniteRenderer = Nanite::IRenderer::Create(
		GraphBuilder,
		*Renderer->Scene,
		Renderer->Views[0],
		Renderer->GetSceneUniforms(),
		SharedContext,
		RasterContext,
		CullingConfig,
		FIntRect(),
		nullptr
	);

	Nanite::FRasterResults RasterResults;

	NaniteRenderer->DrawGeometry(
		Renderer->Scene->NaniteRasterPipelines[ENaniteMeshPass::MaterialCache],
		RasterResults.VisibilityQuery,
		*NaniteViews,
		RenderData.Nanite.InstanceDraws
	);

	NaniteRenderer->ExtractResults(RasterResults);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	FNaniteRasterUniformParameters* RasterUniformParameters = GraphBuilder.AllocParameters<FNaniteRasterUniformParameters>();
	RasterUniformParameters->PageConstants = RasterResults.PageConstants;
	RasterUniformParameters->MaxNodes = Nanite::FGlobalResources::GetMaxNodes();
	RasterUniformParameters->MaxVisibleClusters = Nanite::FGlobalResources::GetMaxVisibleClusters();
	RasterUniformParameters->MaxCandidatePatches = Nanite::FGlobalResources::GetMaxCandidatePatches();
	RasterUniformParameters->MaxPatchesPerGroup = RasterResults.MaxPatchesPerGroup;
	RasterUniformParameters->MeshPass = RasterResults.MeshPass;
	RasterUniformParameters->InvDiceRate = RasterResults.InvDiceRate;
	RasterUniformParameters->RenderFlags = RasterResults.RenderFlags;
	RasterUniformParameters->DebugFlags = RasterResults.DebugFlags;

	FNaniteShadingUniformParameters* ShadingUniformParameters = GraphBuilder.AllocParameters<FNaniteShadingUniformParameters>();
	ShadingUniformParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	ShadingUniformParameters->HierarchyBuffer = Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
	ShadingUniformParameters->VisibleClustersSWHW = GraphBuilder.CreateSRV(RasterResults.VisibleClustersSWHW);
	ShadingUniformParameters->VisBuffer64 = RasterContext.VisBuffer64;
	ShadingUniformParameters->DbgBuffer64 = SystemTextures.Black;
	ShadingUniformParameters->DbgBuffer32 = SystemTextures.Black;
	ShadingUniformParameters->ShadingMask = SystemTextures.Black;
	ShadingUniformParameters->ShadingBinData = GraphBuilder.CreateSRV(ShadingBinData);
	ShadingUniformParameters->MultiViewEnabled = 1;
	ShadingUniformParameters->MultiViewIndices = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder));
	ShadingUniformParameters->MultiViewRectScaleOffsets = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<FVector4>(GraphBuilder));
	ShadingUniformParameters->InViews = GraphBuilder.CreateSRV(PackedViewBuffer);

	FMaterialCacheNaniteShadeParameters* PassParameters = GraphBuilder.AllocParameters<FMaterialCacheNaniteShadeParameters>();
	PassParameters->NaniteRaster = GraphBuilder.CreateUniformBuffer(RasterUniformParameters);
	PassParameters->NaniteShading = GraphBuilder.CreateUniformBuffer(ShadingUniformParameters);
	PassParameters->View = Renderer->Views[0].GetShaderParameters();
	PassParameters->Scene = Renderer->Views[0].GetSceneUniforms().GetBuffer(GraphBuilder);
	Context.PassShadeParameters = PassParameters;

	FMaterialCacheUniformParameters* PassUniformParameters = GraphBuilder.AllocParameters<FMaterialCacheUniformParameters>();
	PassUniformParameters->ShadingBinData = GraphBuilder.CreateSRV(ShadingBinData);
	SetupSceneTextureUniformParameters(GraphBuilder, &Renderer->GetActiveSceneTextures(), Renderer->Scene->GetFeatureLevel(), ESceneTextureSetupMode::None, PassUniformParameters->SceneTextures);
	Context.PassUniformParameters = PassUniformParameters;
}

static void MaterialCacheFinalizePages(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, FMaterialCacheBlackboardData& Data, FMaterialCacheRenderData& RenderData)
{
	RDG_EVENT_SCOPE(GraphBuilder, "Finalize Pages");

	if (!RenderData.ABuffer.Pages.Num())
	{
		return;
	}

	const FIntPoint TileSize = GetMaterialCacheTileSize();

	FRDGUploadData<UE::HLSL::FMaterialCachePageWriteData> PageWriteDataArray(GraphBuilder, RenderData.ABuffer.Pages.Num());

	for (int32 PageIndex = 0; PageIndex < RenderData.ABuffer.Pages.Num(); PageIndex++)
	{
		const FMaterialCachePageEntry& Page = RenderData.ABuffer.Pages[PageIndex];

		UE::HLSL::FMaterialCachePageWriteData& BinData = PageWriteDataArray[PageIndex];
		BinData.ABufferPhysicalPosition = GetMaterialCacheABufferTilePhysicalLocation(Data.RenderData, PageIndex);
		BinData.VTPhysicalPosition = FUintVector2(Page.TileRect.Min.X, Page.TileRect.Min.Y);
	}

	FRDGBuffer* PageWriteData = CreateByteAddressBuffer(
		GraphBuilder,
		TEXT("MaterialCache.PageWriteData"),
		PageWriteDataArray.NumBytes(), PageWriteDataArray.GetData(),
		PageWriteDataArray.NumBytes()
	);

	auto* PassParameters = GraphBuilder.AllocParameters<FMaterialCacheABufferWritePagesCS::FParameters>();
	PassParameters->PageWriteData = GraphBuilder.CreateSRV(PageWriteData);
	PassParameters->ABuffer0 = GraphBuilder.CreateSRV(RenderData.ABuffer.ABufferTextures[0]);
	PassParameters->ABuffer1 = GraphBuilder.CreateSRV(RenderData.ABuffer.ABufferTextures[1]);
	PassParameters->ABuffer2 = GraphBuilder.CreateSRV(RenderData.ABuffer.ABufferTextures[2]);
	PassParameters->RWVTLayer0 = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(Data.PendingEntries[0].Setup.PhysicalRenderTargets[0]));
	PassParameters->RWVTLayer1 = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(Data.PendingEntries[0].Setup.PhysicalRenderTargets[1]));
	PassParameters->RWVTLayer2 = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(Data.PendingEntries[0].Setup.PhysicalRenderTargets[2]));

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("WritePages"),
		Renderer->Views[0].ShaderMap->GetShader<FMaterialCacheABufferWritePagesCS>(),
		PassParameters,
		FIntVector(FMath::DivideAndRoundUp(TileSize.X * TileSize.Y, 64), RenderData.ABuffer.Pages.Num(), 1));
}

static void SetMaterialCacheABufferParameters(FRDGBuilder& GraphBuilder, FMaterialCacheRenderData& RenderData, FMaterialCacheHardwareContext& HardwareContext, FMaterialCacheNaniteContext& NaniteContext, FMaterialCacheVertexInvariantContext& VertexInvariantContext)
{
	FMaterialCacheABufferParameters PassParameters;
	PassParameters.RWABuffer0 = GraphBuilder.CreateUAV(RenderData.ABuffer.ABufferTextures[0], ERDGUnorderedAccessViewFlags::SkipBarrier);
	PassParameters.RWABuffer1 = GraphBuilder.CreateUAV(RenderData.ABuffer.ABufferTextures[1], ERDGUnorderedAccessViewFlags::SkipBarrier);
	PassParameters.RWABuffer2 = GraphBuilder.CreateUAV(RenderData.ABuffer.ABufferTextures[2], ERDGUnorderedAccessViewFlags::SkipBarrier);

	if (HardwareContext.PassUniformParameters)
	{
		HardwareContext.PassUniformParameters->ABuffer = PassParameters;
	}
	
	if (NaniteContext.PassUniformParameters)
	{
		NaniteContext.PassUniformParameters->ABuffer = PassParameters;
	}
	
	if (VertexInvariantContext.PassUniformParameters)
	{
		VertexInvariantContext.PassUniformParameters->ABuffer = PassParameters;
	}
}

static void MaterialCacheRenderLayers(FSceneRenderer* Renderer, FRDGBuilder& GraphBuilder, FMaterialCacheSceneExtension& SceneExtension, FMaterialCacheBlackboardData& Data)
{
	FMaterialCacheRenderData& RenderData = Data.RenderData;
	MaterialCacheCreateABuffer(GraphBuilder, RenderData);

	// Scope for timings, composite all pages
	{
		RDG_EVENT_SCOPE_STAT(GraphBuilder, MaterialCacheCompositePages, "MaterialCacheCompositePages");
		RDG_GPU_STAT_SCOPE(GraphBuilder, MaterialCacheCompositePages);

		FMaterialCacheHardwareContext HardwareContext;
		MaterialCacheSetupHardwareContext(Renderer, GraphBuilder, SceneExtension, Data, RenderData, HardwareContext);

		FMaterialCacheNaniteContext NaniteContext;
		MaterialCacheSetupNaniteContext(Renderer, GraphBuilder, SceneExtension, Data, RenderData, NaniteContext);

		FMaterialCacheVertexInvariantContext VertexInvariantContext;
		MaterialCacheSetupVertexInvariantContext(Renderer, GraphBuilder, SceneExtension, Data, RenderData, VertexInvariantContext);

		for (int32 LayerIndex = 0; LayerIndex < RenderData.Layers.Num(); LayerIndex++)
		{
			FMaterialCacheLayerRenderData& Layer = RenderData.Layers[LayerIndex];
			RDG_EVENT_SCOPE(GraphBuilder, "Layer %u", LayerIndex);

			// Set the ABuffer, skips barriers within a layer on RW passes
			SetMaterialCacheABufferParameters(GraphBuilder, RenderData, HardwareContext, NaniteContext, VertexInvariantContext);

			// Render all pages for this layer
			MaterialCacheRenderHardwarePages(Renderer, GraphBuilder, RenderData, Layer, HardwareContext, LayerIndex);
			MaterialCacheRenderNanitePages(Renderer, GraphBuilder, Data, RenderData, Layer, NaniteContext, LayerIndex);
			MaterialCacheRenderVertexInvariantPages(Renderer, GraphBuilder, Data, RenderData, Layer, VertexInvariantContext, LayerIndex);
		}
	}

	RDG_EVENT_SCOPE_STAT(GraphBuilder, MaterialCacheFinalize, "MaterialCacheFinalize");
	RDG_GPU_STAT_SCOPE(GraphBuilder, MaterialCacheFinalize);

	MaterialCacheFinalizePages(Renderer, GraphBuilder, Data, RenderData);
}

void MaterialCacheEnqueuePages(
	FRDGBuilder& GraphBuilder,
	const FMaterialCacheSetup& Setup,
	const TArrayView<FMaterialCachePageEntry>& Pages
)
{
	if (Pages.IsEmpty())
	{
		return;
	}

	auto& Data = GraphBuilder.Blackboard.GetOrCreate<FMaterialCacheBlackboardData>();

	FMaterialCacheBlackboardPendingEntry& Entry = Data.PendingEntries.Emplace_GetRef();
	Entry.Setup = Setup;
	Entry.Pages.SetNumUninitialized(Pages.Num());

	for (int32 PageIndex = 0; PageIndex < Pages.Num(); PageIndex++)
	{
		FMaterialCachePendingPageEntry& Page = Entry.Pages[PageIndex];
		Page.Page             = Pages[PageIndex];
		Page.ABufferPageIndex = ABufferPageIndexNotProduced;
	}
}

void MaterialCacheRenderPages(FRDGBuilder& GraphBuilder, FSceneRenderer* Renderer)
{
	auto& Data = GraphBuilder.Blackboard.GetOrCreate<FMaterialCacheBlackboardData>();
	if (Data.PendingEntries.IsEmpty())
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "MaterialCache");

	auto& SceneExtension = Renderer->Scene->GetExtension<FMaterialCacheSceneExtension>();

	MaterialCacheAllocateAndBatchPages(Renderer, GraphBuilder, SceneExtension, Data);

	MaterialCacheRenderLayers(Renderer, GraphBuilder, SceneExtension, Data);

	Data.PendingEntries.Empty();
}
