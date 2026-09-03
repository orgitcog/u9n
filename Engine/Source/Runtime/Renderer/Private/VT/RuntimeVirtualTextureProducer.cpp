// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTextureProducer.h"

#include "RendererInterface.h"
#include "ScenePrivate.h"
#include "VT/RuntimeVirtualTextureRender.h"
#include "VT/RuntimeVirtualTextureSceneProxy.h"


FRuntimeVirtualTextureFinalizer::FRuntimeVirtualTextureFinalizer(
	FVTProducerDescription const& InDesc, 
	int32 InRuntimeVirtualTextureId,
	ERuntimeVirtualTextureMaterialType InMaterialType, 
	bool InClearTextures, 
	FSceneInterface* InScene, 
	FTransform const& InUVToWorld,
	FBox const& InWorldBounds,
	FVector4f const& InCustomMaterialData)
	: Desc(InDesc)
	, RuntimeVirtualTextureId(InRuntimeVirtualTextureId)
	, MaterialType(InMaterialType)
	, bClearTextures(InClearTextures)
	, Scene(InScene)
	, UVToWorld(InUVToWorld)
	, WorldBounds(InWorldBounds)
	, CustomMaterialData(InCustomMaterialData)
{
}

bool FRuntimeVirtualTextureFinalizer::IsReady()
{
	return RuntimeVirtualTexture::IsSceneReadyToRender(Scene);
}

void FRuntimeVirtualTextureFinalizer::AddTile(FTileEntry& Tile)
{
	Tiles.Add(Tile);
}

void FRuntimeVirtualTextureFinalizer::RenderFinalize(FRDGBuilder& GraphBuilder, ISceneRenderer* SceneRenderer)
{
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	RuntimeVirtualTexture::FRenderPageBatchDesc RenderPageBatchDesc;
	RenderPageBatchDesc.SceneRenderer = SceneRenderer;
	RenderPageBatchDesc.RuntimeVirtualTextureId = RuntimeVirtualTextureId;
	RenderPageBatchDesc.UVToWorld = UVToWorld;
	RenderPageBatchDesc.WorldBounds = WorldBounds;
	RenderPageBatchDesc.MaterialType = MaterialType;
	RenderPageBatchDesc.MaxLevel = Desc.MaxLevel;
	RenderPageBatchDesc.bClearTextures = bClearTextures;
	RenderPageBatchDesc.bIsThumbnails = false;
	RenderPageBatchDesc.FixedColor = FLinearColor::Transparent;
	RenderPageBatchDesc.CustomMaterialData = CustomMaterialData;
	
	for (int LayerIndex = 0; LayerIndex < RuntimeVirtualTexture::MaxTextureLayers; ++LayerIndex)
	{
		RenderPageBatchDesc.Targets[LayerIndex].PooledRenderTarget = Tiles[0].Targets[LayerIndex].PooledRenderTarget;
	}

	int32 BatchSize = 0;
	for (auto Entry : Tiles)
	{
		bool bBreakBatchForTextures = false;
		for (int LayerIndex = 0; LayerIndex < RuntimeVirtualTexture::MaxTextureLayers; ++LayerIndex)
		{
			// This should never happen which is why we don't bother sorting to maximize batch size
			bBreakBatchForTextures |= (RenderPageBatchDesc.Targets[LayerIndex].PooledRenderTarget != Entry.Targets[LayerIndex].PooledRenderTarget);
		}

		if (BatchSize == RuntimeVirtualTexture::MaxRenderPageBatch || bBreakBatchForTextures)
		{
			RenderPageBatchDesc.NumPageDescs = BatchSize;
			Batches.Add(RuntimeVirtualTexture::InitPageBatch(GraphBuilder, RenderPageBatchDesc));
			BatchSize = 0;
		}

		if (bBreakBatchForTextures)
		{
			for (int LayerIndex = 0; LayerIndex < RuntimeVirtualTexture::MaxTextureLayers; ++LayerIndex)
			{
				RenderPageBatchDesc.Targets[LayerIndex].PooledRenderTarget = Entry.Targets[LayerIndex].PooledRenderTarget;
			}
		}

		RuntimeVirtualTexture::FRenderPageDesc& RenderPageDesc = RenderPageBatchDesc.PageDescs[BatchSize++];

		const float X = (float)FMath::ReverseMortonCode2_64(Entry.vAddress);
		const float Y = (float)FMath::ReverseMortonCode2_64(Entry.vAddress >> 1);
		const float DivisorX = (float)Desc.BlockWidthInTiles / (float)(1 << Entry.vLevel);
		const float DivisorY = (float)Desc.BlockHeightInTiles / (float)(1 << Entry.vLevel);

		const FVector2D UV(X / DivisorX, Y / DivisorY);
		const FVector2D UVSize(1.f / DivisorX, 1.f / DivisorY);
		const FVector2D UVBorder = UVSize * ((float)Desc.TileBorderSize / (float)Desc.TileSize);
		const FBox2D UVRange(UV - UVBorder, UV + UVSize + UVBorder);

		RenderPageDesc.vLevel = Entry.vLevel;
		RenderPageDesc.UVRange = UVRange;

		const int32 TileSize = Desc.TileSize + 2 * Desc.TileBorderSize;
		for (int LayerIndex = 0; LayerIndex < RuntimeVirtualTexture::MaxTextureLayers; ++LayerIndex)
		{
			const FIntPoint DestinationRectStart0(Entry.Targets[LayerIndex].pPageLocation.X * TileSize, Entry.Targets[LayerIndex].pPageLocation.Y * TileSize);
			RenderPageDesc.DestRect[LayerIndex] = FIntRect(DestinationRectStart0, DestinationRectStart0 + FIntPoint(TileSize, TileSize));
		}
	}

	if (BatchSize > 0)
	{
		RenderPageBatchDesc.NumPageDescs = BatchSize;
		Batches.Add(RuntimeVirtualTexture::InitPageBatch(GraphBuilder, RenderPageBatchDesc));
	}

	for (RuntimeVirtualTexture::FBatchRenderContext const* Batch : Batches)
	{
		RuntimeVirtualTexture::RenderPageBatch(GraphBuilder, *Batch);
	}
}

void FRuntimeVirtualTextureFinalizer::Finalize(FRDGBuilder& GraphBuilder)
{
	for (RuntimeVirtualTexture::FBatchRenderContext const* Batch : Batches)
	{
		RuntimeVirtualTexture::FinalizePageBatch(GraphBuilder, *Batch);
	}

	Tiles.SetNumUnsafeInternal(0);
	Batches.SetNumUnsafeInternal(0);
}

FRuntimeVirtualTextureProducer::FRuntimeVirtualTextureProducer(
	FVTProducerDescription const& InDesc,
	int32 InRuntimeVirtualTextureId,
	ERuntimeVirtualTextureMaterialType InMaterialType,
	bool InClearTextures,
	FSceneInterface* InScene,
	FTransform const& InUVToWorld,
	FBox const& InWorldBounds)
	: Finalizer(InDesc, InRuntimeVirtualTextureId, InMaterialType, InClearTextures, InScene, InUVToWorld, InWorldBounds, FVector4f::Zero())
{
}

FRuntimeVirtualTextureProducer::FRuntimeVirtualTextureProducer(
	FVTProducerDescription const& InDesc, 
	int32 InRuntimeVirtualTextureId,
	ERuntimeVirtualTextureMaterialType InMaterialType,
	bool InClearTextures, 
	FSceneInterface* InScene, 
	FTransform const& InUVToWorld,
	FBox const& InWorldBounds,
	FVector4f const& InCustomMaterialData)
	: Finalizer(InDesc, InRuntimeVirtualTextureId, InMaterialType, InClearTextures, InScene, InUVToWorld, InWorldBounds, InCustomMaterialData)
{
}

FVTRequestPageResult FRuntimeVirtualTextureProducer::RequestPageData(
	FRHICommandList& RHICmdList,
	const FVirtualTextureProducerHandle& ProducerHandle,
	uint8 LayerMask,
	uint8 vLevel,
	uint64 vAddress,
	EVTRequestPagePriority Priority)
{
	// Note that when the finalizer is not ready (outside of the Begin/End Scene Render) we return the Saturated status here.
	// This is to indicate that the RVT can't render at this time (because we require the GPU Scene to be up to date).
	// This will happen for DrawTileMesh() style rendering used by material/HLOD baking.
	// It's best to avoid sampling RVT in material baking, but if it is necessary then an option is to have streaming mips built and enabled.
	FVTRequestPageResult result;
	result.Handle = 0;
	result.Status = Finalizer.IsReady() ? EVTRequestPageStatus::Available : EVTRequestPageStatus::Saturated;
	return result;
}

IVirtualTextureFinalizer* FRuntimeVirtualTextureProducer::ProducePageData(
	FRHICommandList& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	EVTProducePageFlags Flags,
	const FVirtualTextureProducerHandle& ProducerHandle,
	uint8 LayerMask,
	uint8 vLevel,
	uint64 vAddress,
	uint64 RequestHandle,
	const FVTProduceTargetLayer* TargetLayers)
{
	FRuntimeVirtualTextureFinalizer::FTileEntry Tile;
	Tile.vAddress = vAddress;
	Tile.vLevel = vLevel;

	// Partial layer masks can happen when one layer has more physical space available so that old pages are evicted at different rates.
	// We currently render all layers even for these partial requests. That might be considered inefficient?
	// But since the problem is avoided by setting bSinglePhysicalSpace on the URuntimeVirtualTexture we can live with it.

	for (int LayerIndex = 0; LayerIndex < RuntimeVirtualTexture::MaxTextureLayers; ++LayerIndex)
	{
		if (TargetLayers[LayerIndex].PooledRenderTarget != nullptr)
		{
			Tile.Targets[LayerIndex] = TargetLayers[LayerIndex];
		}
	}

	Finalizer.AddTile(Tile);

	return &Finalizer;
}
