// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "SceneStateActor.h"
#include "AvaSceneStateActor.generated.h"

#if WITH_EDITOR
class UBlueprint;
class USceneStateBlueprint;
#endif

/** Scene State Actor that has additional editor-only data holding the Scene State Blueprint */
UCLASS(MinimalAPI, NotBlueprintable, NotPlaceable, DisplayName="Motion Design Scene State Actor", HideCategories=(Actor, Collision, Cooking, DataLayers, HLOD, Input, LevelInstance, Networking, Physics, Rendering, Replication, WorldPartition))
class AAvaSceneStateActor : public ASceneStateActor
{
	GENERATED_BODY()

public:
	AAvaSceneStateActor(const FObjectInitializer& InObjectInitializer);

protected:
#if WITH_EDITOR
	//~ Begin AActor
	virtual FString GetDefaultActorLabel() const override;
	virtual bool IsUserManaged() const override { return false; }
	virtual bool SupportsExternalPackaging() const override { return false; }
	//~ End AActor

	//~ Begin UObject
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bInDuplicateForPIE) override;
	virtual void BeginDestroy() override;
	//~ End UObject

	AVALANCHESCENESTATE_API void UpdateSceneStateClass();

	AVALANCHESCENESTATE_API void SetSceneStateBlueprint(USceneStateBlueprint* InSceneStateBlueprint);

	void OnSceneStateRecompiled(UBlueprint* InCompiledBlueprint);

	void OnWorldRenamed(UWorld* InNewWorld);
	void OnWorldCleanup(UWorld* InWorld, bool bInSessionEnded, bool bInCleanupResources);
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBlueprint> SceneStateBlueprint;
#endif

	friend class FAvaSceneStateExtension;
};
