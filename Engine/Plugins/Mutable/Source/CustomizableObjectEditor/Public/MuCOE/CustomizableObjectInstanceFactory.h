// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "CustomizableObjectInstanceFactory.generated.h"

class AActor;
class FText;
class UObject;
class USkeletalMesh;
struct FAssetData;

UCLASS(MinimalAPI, config=Editor)
class UCustomizableObjectInstanceFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

protected:
	// UActorFactory interface
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual FQuat AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const override;
};
