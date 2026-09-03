// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCache/MaterialCacheSceneExtension.h"
#include "MaterialCache/MaterialCache.h"
#include "GlobalRenderResources.h"
#include "ScenePrivate.h"
#include "SceneUniformBuffer.h"
#include "ShaderParameterMacros.h"
#include "MaterialCache/MaterialCacheMeshProcessor.h"
#include "MaterialCache/MaterialCachePrimitiveData.h"
#include "Materials/MaterialRenderProxy.h"

IMPLEMENT_SCENE_EXTENSION(FMaterialCacheSceneExtension);

BEGIN_SHADER_PARAMETER_STRUCT(FMaterialCacheTextureParameters, RENDERER_API)
	SHADER_PARAMETER_TEXTURE(Texture2D<uint4>, PageTableTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, PhysicalTexture0)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, PhysicalTexture1)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, PhysicalTexture2)
	SHADER_PARAMETER(FUintVector4, PackedUniform)
END_SHADER_PARAMETER_STRUCT()

DECLARE_SCENE_UB_STRUCT(FMaterialCacheTextureParameters, MaterialCache, RENDERER_API)

static void GetDefaultMaterialCacheParameters(FMaterialCacheTextureParameters& OutParameters, FRDGBuilder& GraphBuilder);
IMPLEMENT_SCENE_UB_STRUCT(FMaterialCacheTextureParameters, MaterialCache, GetDefaultMaterialCacheParameters);

struct FMaterialCacheSceneExtensionData
{
	~FMaterialCacheSceneExtensionData()
	{
		checkf(VirtualTextures.IsEmpty(), TEXT("Released scene extension data with dangling references"));
	}
	
	FCriticalSection CriticalSection;

	/** Render thread scene proxy association map */
	TMap<FPrimitiveComponentId, FPrimitiveSceneProxy*> SceneProxyMap;

	/** Shared primitive data map */
	TMap<FPrimitiveComponentId, FMaterialCachePrimitiveData*> SceneDataMap;

	/** Shared virtual texture set */
	TArray<IAllocatedVirtualTexture*> VirtualTextures;
};

class FMaterialCacheSceneExtensionRenderer : public ISceneExtensionRenderer
{
	DECLARE_SCENE_EXTENSION_RENDERER(FRenderer, FMaterialCacheSceneExtension);

public:
	FMaterialCacheSceneExtensionRenderer(FSceneRendererBase& InSceneRenderer, FMaterialCacheSceneExtensionData& Data)
		: ISceneExtensionRenderer(InSceneRenderer)
		, Data(Data)
	{
		
	}
	
	virtual void UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniformBuffer) override
	{
		// Single producer
		check(IsInRenderingThread());
		
		FMaterialCacheTextureParameters Parameters;
		if (Data.VirtualTextures.IsEmpty())
		{
			GetDefaultMaterialCacheParameters(Parameters, GraphBuilder);
		}
		else
		{
			IAllocatedVirtualTexture* Texture = Data.VirtualTextures[0];

			// Currently we dont support multiple layers, validate it
			for (int32 i = 1; i < Data.VirtualTextures.Num(); i++)
			{
				checkf(Texture->GetPhysicalTexture(0) == Data.VirtualTextures[i]->GetPhysicalTexture(0),
					  TEXT("Multiple physical spaces not supported"));
				
				checkf(Texture->GetPageTableTexture(0) == Data.VirtualTextures[i]->GetPageTableTexture(0),
					  TEXT("Multiple page tables not supported"));
			}

			// TODO[MP]: Code duplication!
			const uint32 PageSize               = Texture->GetVirtualTileSize();
			const uint32 PageBorderSize         = Texture->GetTileBorderSize();
			const float  RcpPhysicalTextureSize = 1.0f / static_cast<float>(Texture->GetPhysicalTextureSize(0));
			const uint32 PageSizeWithBorder     = PageSize + PageBorderSize * 2u;
			const bool   bPageTableExtraBits    = Texture->GetPageTableFormat() == EVTPageTableFormat::UInt32;
			const float  PackedSignBit          = bPageTableExtraBits ? 1.0f : -1.0f;
			
			Parameters.PageTableTexture = Texture->GetPageTableTexture(0);
			Parameters.PhysicalTexture0 = Texture->GetPhysicalTexture(0);
			Parameters.PhysicalTexture1 = Texture->GetPhysicalTexture(1);
			Parameters.PhysicalTexture2 = Texture->GetPhysicalTexture(2);
			Parameters.PackedUniform.X  = 0xFFFFFFFF;
			Parameters.PackedUniform.Y  = FMath::AsUInt(static_cast<float>(PageSize) * RcpPhysicalTextureSize);
			Parameters.PackedUniform.Z  = FMath::AsUInt(static_cast<float>(PageBorderSize) * RcpPhysicalTextureSize);
			Parameters.PackedUniform.W  = FMath::AsUInt(static_cast<float>(PageSizeWithBorder) * RcpPhysicalTextureSize * PackedSignBit);
		}
		
		SceneUniformBuffer.Set(SceneUB::MaterialCache, Parameters);
	}

private:
	FMaterialCacheSceneExtensionData& Data;
};

class FMaterialCacheSceneExtensionUpdater : public ISceneExtensionUpdater
{
	DECLARE_SCENE_EXTENSION_UPDATER(FRenderer, FMaterialCacheSceneExtension);

public:
	FMaterialCacheSceneExtensionUpdater(FScene& InScene, FMaterialCacheSceneExtensionData& Data) : Scene(InScene), Data(Data)
	{
		
	}
	
	virtual void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet, FSceneUniformBuffer& SceneUniforms) override
	{
		for (FPrimitiveSceneInfo* PrimitiveSceneInfo : ChangeSet.RemovedPrimitiveSceneInfos)
		{
			if (FPrimitiveSceneProxy* Proxy = PrimitiveSceneInfo->Proxy)
			{
				if (Proxy->SupportsMaterialCache())
				{
					Data.SceneProxyMap.Remove(Proxy->GetPrimitiveComponentId());

					// If there's an associated primitive data, empty the proxy related caches
					if (FMaterialCachePrimitiveData** PrimitiveData = Data.SceneDataMap.Find(Proxy->GetPrimitiveComponentId()))
					{
						(*PrimitiveData)->CachedCommands = {};
					}
				}
			}
		}
	}

	virtual void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet) override
	{
		for (FPrimitiveSceneInfo* PrimitiveSceneInfo : ChangeSet.AddedPrimitiveSceneInfos)
		{
			if (FPrimitiveSceneProxy* Proxy = PrimitiveSceneInfo->Proxy)
			{
				if (Proxy->SupportsMaterialCache() && Proxy->GetMaterialCacheTextureDescriptor() != FUintVector2::ZeroValue)
				{
					Data.SceneProxyMap.Add(Proxy->GetPrimitiveComponentId(), Proxy);
				}
			}
		}
	}

private:
	FScene& Scene;
	
	FMaterialCacheSceneExtensionData& Data;
};

FMaterialCacheSceneExtension::FMaterialCacheSceneExtension(FScene& InScene) : ISceneExtension(InScene)
{
	Data = MakeUnique<FMaterialCacheSceneExtensionData>();
}

bool FMaterialCacheSceneExtension::ShouldCreateExtension(FScene& Scene)
{
	return IsMaterialCacheEnabled(Scene.GetShaderPlatform());
}

ISceneExtensionRenderer* FMaterialCacheSceneExtension::CreateRenderer(FSceneRendererBase& InSceneRenderer, const FEngineShowFlags& EngineShowFlags)
{
	return new FMaterialCacheSceneExtensionRenderer(InSceneRenderer, *Data);
}

ISceneExtensionUpdater* FMaterialCacheSceneExtension::CreateUpdater()
{
	return new FMaterialCacheSceneExtensionUpdater(Scene, *Data);
}

FMaterialCachePrimitiveData* FMaterialCacheSceneExtension::GetPrimitiveData(FPrimitiveComponentId PrimitiveComponentId) const
{
	// Multi-consumer is fine
	check(IsInParallelRenderingThread());
	
	FMaterialCachePrimitiveData** It = Data->SceneDataMap.Find(PrimitiveComponentId);
	return It ? *It : nullptr;
}

FPrimitiveSceneProxy* FMaterialCacheSceneExtension::GetSceneProxy(FPrimitiveComponentId PrimitiveComponentId) const
{
	// Multi-consumer is fine
	check(IsInParallelRenderingThread());
	
	FPrimitiveSceneProxy** It = Data->SceneProxyMap.Find(PrimitiveComponentId);
	return It ? *It : nullptr;
}

void FMaterialCacheSceneExtension::Register(FPrimitiveComponentId PrimitiveComponentId, const FMaterialCacheProviderData& InProvider)
{
	// Single producer
	check(IsInRenderingThread());
	
	checkf(!Data->VirtualTextures.Contains(InProvider.Texture), TEXT("Virtual texture double registration"));
	checkf(!Data->SceneDataMap.Contains(PrimitiveComponentId), TEXT("Scene data double registration"));

	// Register scene texture set
	Data->VirtualTextures.AddUnique(InProvider.Texture);

	// Assign stack provider
	FMaterialCachePrimitiveData* PrimitiveData = Data->SceneDataMap.Add(PrimitiveComponentId, new FMaterialCachePrimitiveData());
	PrimitiveData->Provider = InProvider;
}

void FMaterialCacheSceneExtension::Unregister(FPrimitiveComponentId PrimitiveComponentId)
{
	// Single producer
	check(IsInRenderingThread());

	FMaterialCachePrimitiveData* SceneData = Data->SceneDataMap[PrimitiveComponentId];
	ensureMsgf(Data->VirtualTextures.RemoveSingle(SceneData->Provider.Texture), TEXT("Virtual texture deregistration on missing entry"));

	Data->SceneDataMap.Remove(PrimitiveComponentId);
}

static void GetDefaultMaterialCacheParameters(FMaterialCacheTextureParameters& OutParameters, FRDGBuilder&)
{
	OutParameters.PageTableTexture = GBlackUintTexture->TextureRHI;
	OutParameters.PhysicalTexture0 = GBlackTextureWithSRV->TextureRHI;
	OutParameters.PhysicalTexture1 = GBlackTextureWithSRV->TextureRHI;
	OutParameters.PhysicalTexture2 = GBlackTextureWithSRV->TextureRHI;
	OutParameters.PackedUniform       = FUintVector4(0, 0, 0, 0);
}
