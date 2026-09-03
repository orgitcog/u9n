// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCache/MaterialCacheVirtualTexture.h"
#include "Components/PrimitiveComponent.h"
#include "EngineModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "RendererInterface.h"
#include "RendererModule.h"
#include "RenderUtils.h"
#include "ScenePrivate.h"
#include "ShaderPlatformCachedIniValue.h"
#include "TextureResource.h"
#include "VirtualTextureEnum.h"
#include "VirtualTexturing.h"
#include "MaterialCache/MaterialCache.h"
#include "MaterialCache/MaterialCacheABuffer.h"
#include "MaterialCache/MaterialCacheMeshProcessor.h"
#include "MaterialCache/MaterialCachePrimitiveData.h"
#include "MaterialCache/MaterialCacheSceneExtension.h"
#include "MaterialCache/MaterialCacheVirtualFinalizer.h"
#include "MaterialCache/MaterialCacheStackProvider.h"
#include "Materials/MaterialRenderProxy.h"
#include "VT/VirtualTextureBuildSettings.h"

struct FMaterialCacheVirtualBaton
{
	FMaterialCacheSceneExtension* SceneExtension = nullptr;
	FPrimitiveComponentId         PrimitiveComponentId;
};

class FMaterialCacheVirtualProducer : public IVirtualTexture
{
public:
	FMaterialCacheVirtualProducer(FScene* Scene, FPrimitiveComponentId InPrimitiveComponentId, const FVTProducerDescription& InProducerDesc)
		: Finalizer(Scene, InPrimitiveComponentId, InProducerDesc)
		, Scene(Scene)
		, PrimitiveComponentId(InPrimitiveComponentId)
		, ProducerDesc(InProducerDesc)
	{
	
	}

public: /** IVirtualTexture */
	virtual ~FMaterialCacheVirtualProducer() override = default;

	virtual bool IsPageStreamed(uint8 vLevel, uint32 vAddress) const override
	{
		return false;
	}

	virtual FVTRequestPageResult RequestPageData(
		FRHICommandList& RHICmdList,
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint64 vAddress,
		EVTRequestPagePriority Priority
	) override
	{
		// Handle all requests in the owning scene's rendering cycle
		if (!Scene->GPUScene.IsRendering())
		{
			return FVTRequestPageResult(EVTRequestPageStatus::Saturated, 0u);
		}
		
#if WITH_EDITOR
		auto& Extension = Scene->GetExtension<FMaterialCacheSceneExtension>();

		// If any material is being cached, handle the request later
		// (Or if the proxy isn't ready, for any reason)
		FPrimitiveSceneProxy* Proxy = Extension.GetSceneProxy(PrimitiveComponentId);
		if (!Proxy || !IsMaterialCacheMaterialReady(Scene->GetFeatureLevel(), Proxy))
		{
			// Note: Used Saturated as Pending may still be processed the same update
			return FVTRequestPageResult(EVTRequestPageStatus::Saturated, 0u);
		}

		// Check with the stack provider
		FMaterialCachePrimitiveData* Data = Extension.GetPrimitiveData(PrimitiveComponentId);
		if (Data->Provider.StackProvider.IsValid() && !Data->Provider.StackProvider->IsMaterialResourcesReady())
		{
			return FVTRequestPageResult(EVTRequestPageStatus::Saturated, 0u);
		}
#endif // WITH_EDITOR
		
		// All pages are implicitly available
		return FVTRequestPageResult(EVTRequestPageStatus::Available, 0u);
	}

	virtual IVirtualTextureFinalizer* ProducePageData(
		FRHICommandList& RHICmdList,
		ERHIFeatureLevel::Type FeatureLevel,
		EVTProducePageFlags Flags,
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint64 vAddress,
		uint64 RequestHandle,
		const FVTProduceTargetLayer* TargetLayers
	) override
	{
		FMaterialCacheTileEntry Tile;
		Tile.Address = vAddress;
		Tile.Level = vLevel;

		for (int32 LayerIndex = 0; LayerIndex < ProducerDesc.NumTextureLayers; LayerIndex++)
		{
			Tile.TargetLayers.Add(TargetLayers[LayerIndex]);
		}
		
		Finalizer.AddTile(Tile);
		
		return &Finalizer;
	}

	/** Single finalizer per producer */
	FMaterialCacheVirtualFinalizer Finalizer;
	
private:
	/** Render scene, lifetime tied to the parent game virtual texture */
	FScene* Scene = nullptr;

	/** Owning component id, lifetime tied to the parent game virtual texture */
	FPrimitiveComponentId PrimitiveComponentId;

	FVTProducerDescription ProducerDesc;
};

class FMaterialCacheVirtualTextureResource : public FVirtualTexture2DResource
{
public:
	FMaterialCacheVirtualTextureResource(FSceneInterface* Scene, FPrimitiveComponentId InPrimitiveComponentId, FIntPoint InTileCount, int32 InTileSize, int32 InTileBorderSize)
		: Scene(Scene)
		, PrimitiveComponentId(InPrimitiveComponentId)
		, TileCount(InTileCount)
		, TileSize(InTileSize)
		, TileBorderSize(InTileBorderSize)
	{
		MaxLevel = FMath::CeilLogTwo(FMath::Max(InTileCount.X, InTileCount.Y));
		
		GetMaterialCacheABufferFormats({}, ABufferFormats);

		// Share the page table across all physical textures
		bSinglePhysicalSpace = true;
	}

	virtual uint32 GetNumLayers() const override
	{
		return ABufferFormats.Num();
	}
	
	virtual EPixelFormat GetFormat(uint32 LayerIndex) const override
	{
		return ABufferFormats[LayerIndex];
	}
	
	virtual uint32 GetTileSize() const override
	{
		return TileSize;
	}
	
	virtual uint32 GetBorderSize() const override
	{
		return TileBorderSize;
	}
	
	virtual uint32 GetNumTilesX() const override
	{
		return TileCount.X;
	}
	
	virtual uint32 GetNumTilesY() const override
	{
		return TileCount.Y;
	}

	virtual uint32 GetNumMips() const override
	{
		return MaxLevel + 1;
	}

	virtual FIntPoint GetSizeInBlocks() const override
	{
		return 1;
	}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FSamplerStateInitializerRHI SamplerStateInitializer;
		SamplerStateInitializer.Filter = SF_Bilinear;
		SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);

		// Create underlying producer
		FVTProducerDescription ProducerDesc;
		ProducerDesc.Name               = TextureName;
		ProducerDesc.FullNameHash       = GetTypeHash(TextureName);
		ProducerDesc.bContinuousUpdate  = false;
		ProducerDesc.Dimensions         = 2;
		ProducerDesc.TileSize           = TileSize;
		ProducerDesc.TileBorderSize     = TileBorderSize;
		ProducerDesc.BlockWidthInTiles  = TileCount.X;
		ProducerDesc.BlockHeightInTiles = TileCount.Y;
		ProducerDesc.DepthInTiles       = 1u;
		ProducerDesc.MaxLevel           = MaxLevel;
		ProducerDesc.NumTextureLayers   = ABufferFormats.Num();
		ProducerDesc.NumPhysicalGroups  = 1;
		ProducerDesc.Priority           = EVTProducerPriority::Normal;

		for (int32 LayerIndex = 0; LayerIndex < ABufferFormats.Num(); LayerIndex++)
		{
			ProducerDesc.LayerFormat[LayerIndex] = ABufferFormats[LayerIndex];
			ProducerDesc.PhysicalGroupIndex[LayerIndex] = 0;
			ProducerDesc.bIsLayerSRGB[LayerIndex] = false;
		}
		
		// Register producer on page feedback
		FMaterialCacheVirtualProducer* Producer = new FMaterialCacheVirtualProducer(Scene->GetRenderScene(), PrimitiveComponentId, ProducerDesc);
		ProducerHandle = GetRendererModule().RegisterVirtualTextureProducer(RHICmdList, ProducerDesc, Producer);
	}

private:
	/** Owning scene, lifetime tied to the parent game virtual texture */
	FSceneInterface* Scene = nullptr;
	
	/** Owning component id, lifetime tied to the parent game virtual texture */
	FPrimitiveComponentId PrimitiveComponentId;
	
	/** Physical formats */
	TArray<EPixelFormat, TInlineAllocator<MaterialCacheMaxABuffers>> ABufferFormats;

	/** Tiled properties */
	FIntPoint TileCount;
	uint32    TileSize       = 0;
	uint32    TileBorderSize = 0;
	uint32    MaxLevel       = 0;
	uint32    NumSourceMips  = 1;
};

UMaterialCacheVirtualTexture::UMaterialCacheVirtualTexture(const FObjectInitializer& ObjectInitializer) : UTexture(ObjectInitializer)
{
	VirtualTextureStreaming = false;

#if WITH_EDITORONLY_DATA
	CompressionNone = true;
	CompressionForceAlpha = true;
#endif // WITH_EDITORONLY_DATA
}

UMaterialCacheVirtualTexture::~UMaterialCacheVirtualTexture()
{
	
}

void UMaterialCacheVirtualTexture::Flush()
{
	// Get the resource on the game thread
	FVirtualTexture2DResource* VTResource = GetVirtualTexture2DResource();
	if (!VTResource)
	{
		return;
	}

	// Flush the full UV-range
	ENQUEUE_RENDER_COMMAND(MaterialCacheFlush)([VTResource](FRHICommandListBase&)
	{
		if (IAllocatedVirtualTexture* AllocatedVT = VTResource->GetAllocatedVT())
		{
			GetRendererModule().FlushVirtualTextureCache(AllocatedVT, FVector2f(0, 0), FVector2f(1, 1));
		}
	});
}

void UMaterialCacheVirtualTexture::Unregister()
{
	// May not exist if the owning component isnt associated with a world
	FSceneInterface* Scene = OwningComponent->GetScene();
	if (!Scene)
	{
		return;
	}

	// May not exist if headless
	FScene* RenderScene = Scene->GetRenderScene();
	if (!RenderScene)
	{
		return;
	}

	FPrimitiveComponentId PrimitiveComponentId = OwningComponent->GetSceneData().PrimitiveSceneId;
	
	ENQUEUE_RENDER_COMMAND(ReleaseVT)([this, RenderScene, PrimitiveComponentId](FRHICommandListImmediate&)
	{
		auto& SceneExtension = RenderScene->SceneExtensions.GetExtension<FMaterialCacheSceneExtension>();

		// Register this virtual texture to the scene
		SceneExtension.Unregister(PrimitiveComponentId);

		// Remove pending Batons
		if (DestructionBaton)
		{
			GetRendererModule().RemoveAllVirtualTextureProducerDestroyedCallbacks(DestructionBaton);
			delete DestructionBaton;
		}
	});
}

void UMaterialCacheVirtualTexture::GetVirtualTextureBuildSettings(FVirtualTextureBuildSettings& OutSettings) const
{
	OutSettings.TileSize       = GetMaterialCacheTileWidth();
	OutSettings.TileBorderSize = GetMaterialCacheTileBorderWidth();
}

void UMaterialCacheVirtualTexture::UpdateResourceWithParams(EUpdateResourceFlags InFlags)
{	
	Super::UpdateResourceWithParams(InFlags);

	// Get the resource on the game thread
	FVirtualTexture2DResource* VTResource = GetVirtualTexture2DResource();
	if (!VTResource)
	{
		return;
	}

	// May not exist if the owning component isnt associated with a world
	FSceneInterface* Scene = OwningComponent->GetScene();
	if (!Scene)
	{
		return;
	}

	// May not exist if headless
	FScene* RenderScene = Scene->GetRenderScene();
	if (!RenderScene)
	{
		return;
	}

	FPrimitiveComponentId PrimitiveComponentId = OwningComponent->GetSceneData().PrimitiveSceneId;

	ENQUEUE_RENDER_COMMAND(AcquireVT)([this, VTResource, RenderScene, PrimitiveComponentId](FRHICommandListImmediate&)
	{
		auto& SceneExtension = RenderScene->SceneExtensions.GetExtension<FMaterialCacheSceneExtension>();
		
		// If already allocated, ignore
		if (VTResource->GetAllocatedVT())
		{
			return;
		}

		// Attempt to allocate
		IAllocatedVirtualTexture* AllocatedVT = VTResource->AcquireAllocatedVT();
		if (!ensure(AllocatedVT))
		{
			return;
		}

		// Register this virtual texture to the scene
		FMaterialCacheProviderData PrimitiveData;
		PrimitiveData.Texture = AllocatedVT;
		PrimitiveData.StackProvider = MaterialStackProvider;
		SceneExtension.Register(PrimitiveComponentId, PrimitiveData);

		// Baton for destruction
		DestructionBaton = new FMaterialCacheVirtualBaton();
		DestructionBaton->SceneExtension = &SceneExtension;
		DestructionBaton->PrimitiveComponentId = PrimitiveComponentId;

		GetRendererModule().AddVirtualTextureProducerDestroyedCallback(
			AllocatedVT->GetProducerHandle(0),
			[](const FVirtualTextureProducerHandle&, void* InBaton)
			{
				auto* Baton = static_cast<const FMaterialCacheVirtualBaton*>(InBaton);
				Baton->SceneExtension->Unregister(Baton->PrimitiveComponentId);
				delete Baton;
			},
			DestructionBaton
		);
	});

	// Recreate the owning components scene proxy to update the relevant descriptor
	if (UPrimitiveComponent* PrimitiveComponent = OwningComponent.Get())
	{
		PrimitiveComponent->MarkRenderStateDirty();
	}
}

EMaterialValueType UMaterialCacheVirtualTexture::GetMaterialType() const
{
	return MCT_TextureVirtual;
}

float UMaterialCacheVirtualTexture::GetSurfaceWidth() const
{
	return GetMaterialCacheTileWidth() * TileCount.X;
}

float UMaterialCacheVirtualTexture::GetSurfaceHeight() const
{
	return GetMaterialCacheTileWidth() * TileCount.Y;
}

uint32 UMaterialCacheVirtualTexture::GetSurfaceArraySize() const
{
	return 1;
}

float UMaterialCacheVirtualTexture::GetSurfaceDepth() const
{
	return 1;
}

ETextureClass UMaterialCacheVirtualTexture::GetTextureClass() const
{
	return ETextureClass::TwoD;
}

FTextureResource* UMaterialCacheVirtualTexture::CreateResource()
{
	check(IsInGameThread());
	
	if (!OwningComponent.Get())
	{
		UE_LOG(LogRenderer, Error, TEXT("Object space virtual texture requires an owning component"));
		return nullptr;
	}
	
	FVirtualTextureBuildSettings DefaultSettings;
	DefaultSettings.Init();
	GetVirtualTextureBuildSettings(DefaultSettings);

	return new FMaterialCacheVirtualTextureResource(
		OwningComponent->GetScene(),
		OwningComponent->GetPrimitiveSceneId(),
		TileCount,
		DefaultSettings.TileSize,
		DefaultSettings.TileBorderSize
	);
}

bool UMaterialCacheVirtualTexture::IsCurrentlyVirtualTextured() const
{
	return true;
}

FVirtualTexture2DResource* UMaterialCacheVirtualTexture::GetVirtualTexture2DResource()
{
	FTextureResource* Resource = GetResource();
	if (!Resource)
	{
		return nullptr;
	}
	
	return Resource->GetVirtualTexture2DResource();
}

