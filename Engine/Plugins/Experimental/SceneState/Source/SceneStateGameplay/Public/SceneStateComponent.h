// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "SceneStateComponent.generated.h"

class USceneStateObject;
class USceneStateComponentPlayer;
struct FSceneStateComponentInstanceData;

UCLASS(MinimalAPI, BlueprintType, meta=(BlueprintSpawnableComponent))
class USceneStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	SCENESTATEGAMEPLAY_API static const FLazyName SceneStatePlayerName;

	SCENESTATEGAMEPLAY_API USceneStateComponent(const FObjectInitializer& InObjectInitializer);

	USceneStateComponentPlayer* GetSceneStatePlayer() const
	{
		return SceneStatePlayer;
	}

	SCENESTATEGAMEPLAY_API TSubclassOf<USceneStateObject> GetSceneStateClass() const;

	SCENESTATEGAMEPLAY_API void SetSceneStateClass(TSubclassOf<USceneStateObject> InSceneStateClass);

	UFUNCTION(BlueprintCallable, Category = "Scene State")
	SCENESTATEGAMEPLAY_API USceneStateObject* GetSceneState() const;

	void ApplyComponentInstanceData(FSceneStateComponentInstanceData* InComponentInstanceData);

	//~ Begin UActorComponent
	SCENESTATEGAMEPLAY_API virtual void InitializeComponent() override;
	SCENESTATEGAMEPLAY_API virtual void OnRegister() override;
	SCENESTATEGAMEPLAY_API virtual void BeginPlay() override;
	SCENESTATEGAMEPLAY_API virtual void TickComponent(float InDeltaTime, ELevelTick InTickType, FActorComponentTickFunction* InThisTickFunction) override;
	SCENESTATEGAMEPLAY_API virtual void EndPlay(const EEndPlayReason::Type InEndPlayReason) override;
	SCENESTATEGAMEPLAY_API virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	//~ End UActorComponent

private:
	UPROPERTY(VisibleAnywhere, Instanced, Category="Scene State", meta=(ShowInnerProperties))
	TObjectPtr<USceneStateComponentPlayer> SceneStatePlayer;
};
