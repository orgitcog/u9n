// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "InstantMovementEffect.h"
#include "MovementMode.h"
#include "Templates/SubclassOf.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"

class UChaosMoverSimulation;

namespace UE::ChaosMover
{
	class CHAOSMOVER_API FMoverStateMachine
	{
	public:
		FMoverStateMachine();
		virtual ~FMoverStateMachine();

		struct FInitParams
		{
			TWeakObjectPtr<UChaosMoverSimulation> Simulation;
			TWeakObjectPtr<UNullMovementMode> NullMovementMode;
			TWeakObjectPtr<UImmediateMovementModeTransition> ImmediateMovementModeTransition;
		};
		void Init(const FInitParams& Params);

		void RegisterMovementMode(FName ModeName, TWeakObjectPtr<UBaseMovementMode> Mode, bool bIsDefaultMode = false);

		void UnregisterMovementMode(FName ModeName);
		void ClearAllMovementModes();

		void RegisterGlobalTransition(TWeakObjectPtr<UBaseMovementModeTransition> Transition);
		void UnregisterGlobalTransition(TWeakObjectPtr<UBaseMovementModeTransition> Transition);
		void ClearAllGlobalTransitions();

		FName GetDefaultModeName() const;
		void SetDefaultMode(FName NewDefaultModeName);

		void QueueNextMode(FName DesiredNextModeName, bool bShouldReenter = false);
		void SetModeImmediately(FName DesiredModeName, bool bShouldReenter = false);
		void ClearQueuedMode();

		void OnSimulationTick(const FMoverTimeStep& TimeStep, const FMoverTickStartData& StartState, UMoverBlackboard* SimBlackboard, UMovementMixer* MovementMixer, FMoverTickEndData& OutputState);
		void OnSimulationPreRollback(const FMoverSyncState* InvalidSyncState, const FMoverSyncState* SyncState, const FMoverAuxStateContext* InvalidAuxState, const FMoverAuxStateContext* AuxState);
		void OnSimulationRollback(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState);

		FName GetCurrentModeName() const
		{
			return CurrentModeName;
		}

		const TWeakObjectPtr<UBaseMovementMode> GetCurrentMode() const;

		const TWeakObjectPtr<UBaseMovementMode> FindMovementMode(FName ModeName) const;

		void QueueLayeredMove(TSharedPtr<FLayeredMoveBase> Move);

		void QueueInstantMovementEffect(TSharedPtr<FInstantMovementEffect> Effect);

		void SetOwnerActorName(const FString& InOwnerActorName);
		void SetOwnerActorLocalNetRole(ENetRole InOwnerActorLocalNetRole);

	protected:

		TMap<FName, TWeakObjectPtr<UBaseMovementMode>> Modes;
		TArray<TWeakObjectPtr<UBaseMovementModeTransition>> GlobalTransitions;

		TWeakObjectPtr<UImmediateMovementModeTransition> QueuedModeTransitionWeakPtr;

		TWeakObjectPtr<UImmediateMovementModeTransition> ImmediateMovementModeTransitionWeakPtr;
		TWeakObjectPtr<UNullMovementMode> NullMovementModeWeakPtr;

		FString OwnerActorName;
		ENetRole OwnerActorLocalNetRole;

		FName DefaultModeName = NAME_None;
		FName CurrentModeName = NAME_None;

		/** Moves that are queued to be added to the simulation at the start of the next sim subtick */
		TArray<TSharedPtr<FLayeredMoveBase>> QueuedLayeredMoves;

		/** Effects that are queued to be applied to the simulation at the start of the next sim subtick or at the end of this tick */
		TArray<TSharedPtr<FInstantMovementEffect>> QueuedInstantEffects;

		TWeakObjectPtr<UChaosMoverSimulation> Simulation;

	private:
		void ConstructDefaultModes();
		void AdvanceToNextMode();
		void FlushQueuedMovesToGroup(FLayeredMoveGroup& Group);
		bool ApplyInstantEffects(const FMoverTickStartData& SubstepStartData, const FMoverTimeStep& SubTimeStep, FMoverSyncState& OutputState);
	
		float InternalSimTimeMs = 0.0f;
	};
}