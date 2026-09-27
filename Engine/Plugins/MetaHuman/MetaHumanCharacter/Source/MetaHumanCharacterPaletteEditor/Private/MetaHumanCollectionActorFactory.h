// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "MetaHumanCollectionActorFactory.generated.h"

class AActor;
class FText;
class UObject;
class USkeletalMesh;
struct FAssetData;

/**
 * Allows a MetaHuman Collection or Instance asset to be dragged from the Content Browser into a level
 * viewport to spawn the appropriate actor
 */
UCLASS(MinimalAPI, config=Editor)
class UMetaHumanCollectionActorFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

protected:
	//~ Begin UActorFactory Interface
	virtual UClass* GetDefaultActorClass(const FAssetData& AssetData) override;
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	//~ End UActorFactory Interface
};
