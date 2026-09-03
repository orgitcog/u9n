// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MaterialCacheMeshProcessor.h"
#include "SceneExtensions.h"
#include "PrimitiveComponentId.h"

class FPrimitiveSceneProxy;
class IAllocatedVirtualTexture;
class UMaterialCacheStackProvider;
struct FMaterialCacheSceneExtensionData;
struct FMaterialCachePrimitiveData;
struct FMaterialCacheProviderData;

struct FMaterialCacheRegistrationOwner
{
	virtual ~FMaterialCacheRegistrationOwner() { }
};

class FMaterialCacheSceneExtension : public ISceneExtension
{
	DECLARE_SCENE_EXTENSION(RENDERER_API, FMaterialCacheSceneExtension);

public:
	FMaterialCacheSceneExtension(FScene& InScene);

	/** Get the scene proxy associated with a primitive id, nullptr if not found */
	FPrimitiveSceneProxy* GetSceneProxy(FPrimitiveComponentId PrimitiveComponentId) const;

	/** Get the primitive data associated with a primitive id, nullptr if not found */
	FMaterialCachePrimitiveData* GetPrimitiveData(FPrimitiveComponentId PrimitiveComponentId) const;
	
	/** Registration */
	void Register(FPrimitiveComponentId PrimitiveComponentId, const FMaterialCacheProviderData& Data);
	void Unregister(FPrimitiveComponentId PrimitiveComponentId);

public: /** ISceneExtension */
	static bool ShouldCreateExtension(FScene& Scene);
	virtual ISceneExtensionRenderer* CreateRenderer(FSceneRendererBase& InSceneRenderer, const FEngineShowFlags& EngineShowFlags) override;
	virtual ISceneExtensionUpdater* CreateUpdater() override;

private:
	TUniquePtr<FMaterialCacheSceneExtensionData> Data;
};
