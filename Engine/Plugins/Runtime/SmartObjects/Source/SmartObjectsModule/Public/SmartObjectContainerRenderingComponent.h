// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PrimitiveComponent.h"
#include "SmartObjectContainerRenderingComponent.generated.h"

#define UE_API SMARTOBJECTSMODULE_API


class FPrimitiveSceneProxy;

UCLASS(MinimalAPI, hidecategories = (Object, LOD, Lighting, VirtualTexture, Transform, HLOD, Collision, TextureStreaming, Mobile, Physics, Tags, AssetUserData, Activation, Cooking, Rendering), editinlinenew, meta = (BlueprintSpawnableComponent))
class USmartObjectContainerRenderingComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UE_API USmartObjectContainerRenderingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UPrimitiveComponent Interface
	UE_API virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override { return true; }
	//~ End UPrimitiveComponent Interface

	//~ Begin USceneComponent Interface
	UE_API virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	//~ End USceneComponent Interface
};

#undef UE_API
