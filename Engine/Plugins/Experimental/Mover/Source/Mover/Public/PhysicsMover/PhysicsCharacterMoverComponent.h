// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "PhysicsCharacterMoverComponent.generated.h"

#define UE_API MOVER_API


UCLASS(MinimalAPI, BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class UPhysicsCharacterMoverComponent : public UCharacterMoverComponent
{
	GENERATED_BODY()

public:

	UE_API UPhysicsCharacterMoverComponent();
	
	UE_API virtual void BeginPlay() override;

	// Internal Queries on Sync State ////////////

	// Find movement modifier by it's handle. Returns nullptr if the modifier couldn't be found
	UE_API const FMovementModifierBase* FindMovementModifier_Internal(const FMoverSyncState& SyncState, const FMovementModifierHandle& ModifierHandle) const;

	// Find movement modifier by type (returns the first modifier it finds). Returns nullptr if the modifier couldn't be found
	UE_API const FMovementModifierBase* FindMovementModifierByType_Internal(const FMoverSyncState& SyncState, const UScriptStruct* DataStructType) const;

	// Find a movement modifier of a specific type in this components movement modifiers. If not found, null will be returned.
	template <typename ModifierT = FMovementModifierBase UE_REQUIRES(std::is_base_of_v<FMovementModifierBase, ModifierT>)>
	const ModifierT* FindMovementModifierByType_Internal(const FMoverSyncState& SyncState) const { return static_cast<const ModifierT*>(FindMovementModifierByType_Internal(SyncState, ModifierT::StaticStruct())); }

	// Check Mover systems for a gameplay tag.
	UE_API bool HasGameplayTag_Internal(const FMoverSyncState& SyncState, FGameplayTag TagToFind, bool bExactMatch) const;

	// Check if crouching is allowed
	virtual bool CanCrouch_Internal(const FMoverSyncState& SyncState) { return true; }

	// Perform crouch on actor
	UE_API void Crouch_Internal(const FMoverSyncState& SyncState);
	
	// Perform uncrouch on actor
	UE_API void UnCrouch_Internal(const FMoverSyncState& SyncState);

protected:

	UFUNCTION()
	UE_API virtual void OnMoverPreMovement(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState);

	UFUNCTION()
	UE_API virtual void OnMoverPostSimulationTick(const FMoverTimeStep& TimeStep);

	virtual void OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd) override {};

private:

	// Keeps track of post processing that needs to take place on the game thread, after the stance is processed in the simulation tick.
	bool bStancePostProcessed = false;
};

#undef UE_API
