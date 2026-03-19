// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "MovementMode.h"
#include "InstantMovementEffect.h"
#include "MovementModeStateMachine.generated.h"

#define UE_API MOVER_API

struct FProposedMove;
class UImmediateMovementModeTransition;
class UMovementModeTransition;

/**
 * - Any movement modes registered are co-owned by the state machine
 * - There is always an active mode, falling back to a do-nothing 'null' mode
 * - Queuing a mode that is already active will cause it to exit and re-enter
 * - Modes only switch during simulation tick
 */
 UCLASS(MinimalAPI)
class UMovementModeStateMachine : public UObject
{
	 GENERATED_UCLASS_BODY()

public:
	UE_API void RegisterMovementMode(FName ModeName, TObjectPtr<UBaseMovementMode> Mode, bool bIsDefaultMode=false);
	UE_API void RegisterMovementMode(FName ModeName, TSubclassOf<UBaseMovementMode> ModeType, bool bIsDefaultMode=false);

	UE_API void UnregisterMovementMode(FName ModeName);
	UE_API void ClearAllMovementModes();

	UE_API void RegisterGlobalTransition(TObjectPtr<UBaseMovementModeTransition> Transition);
	UE_API void UnregisterGlobalTransition(TObjectPtr<UBaseMovementModeTransition> Transition);
	UE_API void ClearAllGlobalTransitions();

	UE_API void SetDefaultMode(FName NewDefaultModeName);

	UE_API void QueueNextMode(FName DesiredNextModeName, bool bShouldReenter=false);
	UE_API void SetModeImmediately(FName DesiredModeName, bool bShouldReenter=false);
	UE_API void ClearQueuedMode();

	UE_API void OnSimulationTick(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverBlackboard* SimBlackboard, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FMoverTickEndData& OutputState);
 	UE_API void OnSimulationPreRollback(const FMoverSyncState* InvalidSyncState, const FMoverSyncState* SyncState, const FMoverAuxStateContext* InvalidAuxState, const FMoverAuxStateContext* AuxState);
	UE_API void OnSimulationRollback(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState);

	FName GetCurrentModeName() const { return CurrentModeName; }

	UE_API const UBaseMovementMode* GetCurrentMode() const;

	UE_API const UBaseMovementMode* FindMovementMode(FName ModeName) const;

	UE_API void QueueLayeredMove(TSharedPtr<FLayeredMoveBase> Move);
	
 	UE_API void QueueInstantMovementEffect(TSharedPtr<FInstantMovementEffect> Effect);

	UE_API FMovementModifierHandle QueueMovementModifier(TSharedPtr<FMovementModifierBase> Modifier);

 	UE_API void CancelModifierFromHandle(FMovementModifierHandle ModifierHandle);

 	UE_API const FMovementModifierBase* FindQueuedModifier(FMovementModifierHandle ModifierHandle) const;
 	UE_API const FMovementModifierBase* FindQueuedModifierByType(const UScriptStruct* ModifierType) const;
 	
protected:

	UE_API virtual void PostInitProperties() override;

	UPROPERTY()
	TMap<FName, TObjectPtr<UBaseMovementMode>> Modes;
	TArray<TObjectPtr<UBaseMovementModeTransition>> GlobalTransitions;

	UPROPERTY(Transient)
	TObjectPtr<UImmediateMovementModeTransition> QueuedModeTransition;

	FName DefaultModeName = NAME_None;
	FName CurrentModeName = NAME_None;

	/** Moves that are queued to be added to the simulation at the start of the next sim subtick. Access covered by lock. */
	TArray<TSharedPtr<FLayeredMoveBase>> QueuedLayeredMoves;

 	/** Effects that are queued to be applied to the simulation at the start of the next sim subtick or at the end of this tick.  Access covered by lock. */
 	TArray<TSharedPtr<FInstantMovementEffect>> QueuedInstantEffects;

 	/** Modifiers that are queued to be added to the simulation at the start of the next sim subtick. Access covered by lock. */
 	TArray<TSharedPtr<FMovementModifierBase>> QueuedMovementModifiers;

 	/** Modifiers that are to be canceled at the start of the next sim subtick.  Access covered by lock. */
 	TArray<FMovementModifierHandle> ModifiersToCancel;
 	
	// Internal-use-only tick data structs, for efficiency since they typically have the same contents from frame to frame
	FMoverTickStartData WorkingSubstepStartData;
	FSimulationTickParams WorkingSimTickParams;

private:
	// Locks for thread safety on queueing mechanisms
	mutable FRWLock LayeredMoveQueueLock;
	mutable FRWLock InstantEffectsQueueLock;
	mutable FRWLock ModifiersQueueLock;
	mutable FRWLock ModifierCancelQueueLock;

	UE_API void ConstructDefaultModes();
	UE_API void AdvanceToNextMode();
	UE_API void FlushQueuedMovesToGroup(FLayeredMoveGroup& Group);
 	UE_API void FlushQueuedModifiersToGroup(FMovementModifierGroup& ModifierGroup);
 	UE_API void FlushModifierCancellationsToGroup(FMovementModifierGroup& ActiveModifierGroup);
 	UE_API void RollbackModifiers(const FMoverSyncState* InvalidSyncState, const FMoverSyncState* SyncState, const FMoverAuxStateContext* InvalidAuxState, const FMoverAuxStateContext* AuxState);
 	UE_API bool ApplyInstantEffects(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState);
	UE_API AActor* GetOwnerActor() const;
};

#undef UE_API
