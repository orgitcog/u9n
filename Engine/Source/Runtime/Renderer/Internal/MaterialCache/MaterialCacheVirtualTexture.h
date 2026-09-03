// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Texture2D.h"
#include "Runtime/Engine/Internal/MaterialCache/MaterialCacheVirtualTextureDescriptor.h"
#include "MaterialCacheVirtualTexture.generated.h"

#define UE_API RENDERER_API

class UMaterialCacheStackProvider;
struct FMaterialCacheVirtualBaton;

UCLASS(MinimalAPI)
class UMaterialCacheVirtualTexture : public UTexture
{
	GENERATED_UCLASS_BODY()

public:
	UE_API virtual ~UMaterialCacheVirtualTexture() override;
	
	UPROPERTY(VisibleAnywhere, Category="Material Cache")
	TWeakObjectPtr<UPrimitiveComponent> OwningComponent;

	UPROPERTY(VisibleAnywhere, Category="Material Cache")
	TWeakObjectPtr<UMaterialCacheStackProvider> MaterialStackProvider;

	UPROPERTY(VisibleAnywhere, Category="Material Cache")
	FIntPoint TileCount = FIntPoint(8, 8);

	/** Flush all relevant pages and re-composite */
	UE_API void Flush();

	/** Deregister this texture from the scene */
	UE_API void Unregister();

public: /** UTexture */
	UE_API virtual void GetVirtualTextureBuildSettings(FVirtualTextureBuildSettings& OutSettings) const override;
	UE_API virtual void UpdateResourceWithParams(EUpdateResourceFlags InFlags) override;
	UE_API virtual EMaterialValueType GetMaterialType() const override;
	UE_API virtual uint32 GetSurfaceArraySize() const override;
	UE_API virtual float GetSurfaceDepth() const override;
	UE_API virtual float GetSurfaceHeight() const override;
	UE_API virtual float GetSurfaceWidth() const override;
	UE_API virtual ETextureClass GetTextureClass() const override;
	UE_API virtual FTextureResource* CreateResource() override;
	UE_API virtual bool IsCurrentlyVirtualTextured() const override;

private:
	FVirtualTexture2DResource* GetVirtualTexture2DResource();

private:
	FMaterialCacheVirtualBaton* DestructionBaton = nullptr;
};

#undef UE_API
