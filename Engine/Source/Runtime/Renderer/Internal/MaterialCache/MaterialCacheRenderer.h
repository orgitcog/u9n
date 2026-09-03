// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MaterialCacheABuffer.h"
#include "Math/Box2D.h"
#include "Math/IntRect.h"
#include "PrimitiveComponentId.h"

class FRDGBuilder;
class FSceneRenderer;
class FPrimitiveSceneInfo;
struct IPooledRenderTarget;

struct FMaterialCachePageEntry
{
	/** Destination page rectangle */
	FIntRect TileRect;

	/** Primitive UV rectangle associated with a given page */
	FBox2f UVRect;
};

struct FMaterialCacheSetup
{
	/** Persistent primitive id, must have a matching scene proxy */
	FPrimitiveComponentId PrimitiveComponentId;

	/** Destination render targets, must be UAV compatible */
	TArray<IPooledRenderTarget*, TInlineAllocator<MaterialCacheMaxABuffers>> PhysicalRenderTargets;

	/** Page size, includes border */
	FIntPoint TileSize = FIntPoint::ZeroValue;
};

/** Enqueue a set of pages for rendering */
void MaterialCacheEnqueuePages(
	FRDGBuilder& GraphBuilder,
	const FMaterialCacheSetup& Setup,
	const TArrayView<FMaterialCachePageEntry>& Pages
);

/** Process all enqueued pages */
void MaterialCacheRenderPages(FRDGBuilder& GraphBuilder, FSceneRenderer* Renderer);
