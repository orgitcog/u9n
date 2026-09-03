// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MaterialCacheMeshProcessor.h"
#include "PrimitiveComponentId.h"

class FPrimitiveSceneProxy;
class UMaterialCacheStackProvider;

struct FMaterialCacheProviderData
{
	IAllocatedVirtualTexture* Texture = nullptr;
	
	TWeakObjectPtr<UMaterialCacheStackProvider> StackProvider;
};

struct FMaterialCachePrimitiveCachedLayerCommands
{
	FMaterialCachePrimitiveCachedLayerCommands() = default;

	/** No copy or move construction */
	UE_NONCOPYABLE(FMaterialCachePrimitiveCachedLayerCommands);
	
	TArray<FMaterialCacheMeshDrawCommand>          StaticMeshBatchCommands;
	TOptional<FMaterialCacheLayerShadingCSCommand> NaniteLayerShadingCommand;
	TOptional<FMaterialCacheLayerShadingCSCommand> VertexInvariantShadingCommand;
};

struct FMaterialCachePrimitiveCachedCommands
{
	/** Lifetime of material tied to the proxy, any change invalidates the proxy, in turn clearing the cache */
	TMap<UMaterialInterface*, TUniquePtr<FMaterialCachePrimitiveCachedLayerCommands>> Layers;
};

struct FMaterialCachePrimitiveData
{
	FMaterialCacheProviderData Provider;

	FMaterialCachePrimitiveCachedCommands CachedCommands;
};
