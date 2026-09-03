// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "SceneStateActor.generated.h"

class USceneStateComponent;
class USceneStateObject;

UCLASS(MinimalAPI)
class ASceneStateActor : public AActor
{
	GENERATED_BODY()

public:
	SCENESTATEGAMEPLAY_API static const FLazyName SceneStateComponentName;

	SCENESTATEGAMEPLAY_API ASceneStateActor(const FObjectInitializer& InObjectInitializer);

	SCENESTATEGAMEPLAY_API void SetSceneStateClass(TSubclassOf<USceneStateObject> InSceneStateClass);

	SCENESTATEGAMEPLAY_API TSubclassOf<USceneStateObject> GetSceneStateClass() const;

	UFUNCTION(BlueprintCallable, Category = "Scene State")
	SCENESTATEGAMEPLAY_API USceneStateObject* GetSceneState() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scene State", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneStateComponent> SceneStateComponent;
};
