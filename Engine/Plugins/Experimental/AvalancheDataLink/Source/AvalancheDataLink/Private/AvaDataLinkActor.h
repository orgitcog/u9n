// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "AvaDataLinkActor.generated.h"

class UAvaDataLinkInstance;

UCLASS(DisplayName="Motion Design Data Link Actor", HideCategories=(Activation, Actor, AssetUserData, Collision, Cooking, DataLayers, HLOD, Input, LevelInstance, Mobility, Navigation, Networking, Physics, Rendering, Replication, Tags, Transform, WorldPartition))
class AAvaDataLinkActor : public AActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Motion Design Data Link")
	void ExecuteDataLinkInstances();

	UPROPERTY(EditAnywhere, Instanced, Category="Motion Design Data Link")
	TArray<TObjectPtr<UAvaDataLinkInstance>> DataLinkInstances;
};
