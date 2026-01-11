// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PrimitiveComponent.h"
#include "SmartObjectRenderingComponent.generated.h"

#define UE_API SMARTOBJECTSMODULE_API


class FPrimitiveSceneProxy;

UCLASS(MinimalAPI, hidecategories = (Object, LOD, Lighting, VirtualTexture, Transform, HLOD, Collision, TextureStreaming, Mobile, Physics, Tags, AssetUserData, Activation, Cooking, Rendering), editinlinenew, meta = (BlueprintSpawnableComponent))
class USmartObjectRenderingComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	UE_API USmartObjectRenderingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UPrimitiveComponent Interface
	UE_API virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override { return true; }
	//~ End UPrimitiveComponent Interface

	//~ Begin USceneComponent Interface
	UE_API virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	//~ End USceneComponent Interface

#if WITH_EDITOR
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};

#undef UE_API
