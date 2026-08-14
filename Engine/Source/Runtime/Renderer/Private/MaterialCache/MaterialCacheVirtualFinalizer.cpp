// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCache/MaterialCacheVirtualFinalizer.h"
#include "MaterialCache/MaterialCacheRenderer.h"
#include "Components/PrimitiveComponent.h"
#include "Interfaces/ITargetPlatform.h"
#include "RendererInterface.h"
#include "RenderUtils.h"
#include "ShaderPlatformCachedIniValue.h"
#include "TextureResource.h"
#include "VirtualTexturing.h"
#include "MeshPassProcessor.inl"
#include "ScenePrivate.h"
#include "MaterialCache/MaterialCacheSceneExtension.h"

FMaterialCacheVirtualFinalizer::FMaterialCacheVirtualFinalizer(FScene* InScene, FPrimitiveComponentId InPrimitiveComponentId, const FVTProducerDescription& InProducerDesc)
	: Scene(InScene)
	, PrimitiveComponentId(InPrimitiveComponentId)
	, ProducerDesc(InProducerDesc)
{
	SourceFormat       = PF_B8G8R8A8;
	IntermediateFormat = PF_B8G8R8A8;
	DestFormat         = ProducerDesc.LayerFormat[0];
}

void FMaterialCacheVirtualFinalizer::AddTile(const FMaterialCacheTileEntry& InEntry)
{
	FBucket& Bucket = Buckets.FindOrAdd(InEntry.TargetLayers[0].PooledRenderTarget);
	Bucket.TilesToRender.Add(InEntry);
}

void FMaterialCacheVirtualFinalizer::Finalize(FRDGBuilder& GraphBuilder) 
{
	// Append all pending buckets to the pass
	for (auto&& [PhysicalRenderTarget, Bucket] : Buckets)
	{
		TArray<FMaterialCachePageEntry, SceneRenderingAllocator> Pages;

		// Fill all tiles
		for (const FMaterialCacheTileEntry& Tile : Bucket.TilesToRender)
		{
			const float X        = static_cast<float>(FMath::ReverseMortonCode2_64(Tile.Address));
			const float Y        = static_cast<float>(FMath::ReverseMortonCode2_64(Tile.Address >> 1));
			const float DivisorX = static_cast<float>(ProducerDesc.BlockWidthInTiles) / (float)(1 << Tile.Level);
			const float DivisorY = static_cast<float>(ProducerDesc.BlockHeightInTiles) / (float)(1 << Tile.Level);

			// Virtual UV range
			const FVector2f UV(X / DivisorX, Y / DivisorY);
			const FVector2f UVSize(1.0f / DivisorX, 1.0f / DivisorY);
			const FVector2f UVBorder = UVSize * ((float)ProducerDesc.TileBorderSize / (float)ProducerDesc.TileSize);
			const FBox2f    UVRect(UV - UVBorder, UV + UVSize + UVBorder);

			// Layers within the same space share the tile table, so just get the first one
			FIntVector PageLocation = Tile.TargetLayers[0].pPageLocation;

			// Physical tile location
			const int32     TileSize = ProducerDesc.TileSize + 2 * ProducerDesc.TileBorderSize;
			const FIntPoint DestinationPos(PageLocation.X * TileSize, PageLocation.Y * TileSize);
			const FIntRect  DestRect(DestinationPos, DestinationPos + FIntPoint(TileSize, TileSize));

			FMaterialCachePageEntry& Entry = Pages.Emplace_GetRef();
			Entry.TileRect = DestRect;
			Entry.UVRect = UVRect;
		}
	
		FMaterialCacheSetup Setup;
		Setup.PrimitiveComponentId = PrimitiveComponentId;
		Setup.TileSize             = ProducerDesc.TileSize + 2 * ProducerDesc.TileBorderSize;

		// All tiles share the same targets in a bucket
		for (int32 i = 0; i < Bucket.TilesToRender[0].TargetLayers.Num(); i++)
		{
			Setup.PhysicalRenderTargets.Add(Bucket.TilesToRender[0].TargetLayers[i].PooledRenderTarget);
		}
		
		MaterialCacheEnqueuePages(GraphBuilder, Setup, Pages);
	}

	// Cleanup
	Buckets.Empty();
}
