// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Containers/Array.h"

#include "InterchangeEditorScriptLibrary.generated.h"

class AActor;
class ALevelInstance;

class UWorld;
class UInterchangeSceneImportAsset;

UCLASS()
class INTERCHANGEEDITOR_API UInterchangeEditorScriptLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Performs Interchange Reset on a Level Asset.
	 * @param World is the level asset to reset.
	 */
	UFUNCTION(BlueprintCallable, Category="Interchange Utilities | Reset")
	static void ResetLevelAsset(UWorld* World);

	/**
	 * Performs Interchange Reset on an Interchange Scene Import Asset.
	 * Resets all the actors added to the level and assets imported.
	 * @param SceneImportAsset to reset
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange Utilities | Reset")
	static void ResetSceneImportAsset(UInterchangeSceneImportAsset* SceneImportAsset);

	/**
	 * Performs Interchange Reset on Actors.
	 * Resets all qualifying actors. Does nothing to actors that cannot be reset.
	 * @param Actors array containing actors to reset
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange Utilities | Reset")
	static void ResetActors(TArray<AActor*> Actors);

	/**
	 * Checks if an actor can be reset.
	 * @param Actor to check
	 * @return can actor be reset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange Utilities | Reset")
	static bool CanResetActor(const AActor* Actor);

	/**
	 * Checks if an world can be reset.
	 * @param World to check
	 * @return can world be reset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange Utilities | Reset")
	static bool CanResetWorld(const UWorld* World);

	/**
	 * Make Level Instance Actor editable.
	 * @param LevelInstance actor to edit
	 * @return whether putting level instance in edit mode was successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange Utilities | Reset")
	static bool LevelInstanceEnterEditMode(ALevelInstance* LevelInstance);

	/**
	 * Apply/Discard the changes to Level Instance Actor.
	 * @param LevelInstance actor to commit
	 * @param bDiscardChanges should changes be applied or discarded.
	 * @return whether committing level instance was successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange Utilities | Reset")
	static bool LevelInstanceCommit(ALevelInstance* LevelInstance, bool bDiscardChanges);

	/**
	 * Returns array of actors that are editable in the editor when the level instance is put in edit mode.
	 * NOTE: This will return a non-empty array if the LevelInstance is put in the edit mode.
	 * 
	 * @param LevelInstance to get editable actors from
	 * @return Array of editable actors.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange Utilities | Reset")
	static const TArray<AActor*> LevelInstanceGetEditableActors(ALevelInstance* LevelInstance);
};
