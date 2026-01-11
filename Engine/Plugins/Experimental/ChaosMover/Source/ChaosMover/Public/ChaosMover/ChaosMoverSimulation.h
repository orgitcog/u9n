// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Declares.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMoverStateMachine.h"
#include "MoveLibrary/MovementMixer.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoverSimulation.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "PhysicsEngine/ConstraintInstance.h"

#include "ChaosMoverSimulation.generated.h"

namespace Chaos
{
	class FCharacterGroundConstraintProxy;
	class FJointConstraintPhysicsProxy;
	class FCollisionContactModifier;
}

UCLASS(MinimalAPI, BlueprintType)
class UChaosMoverSimulation : public UMoverSimulation
{
	GENERATED_BODY()

public:
	CHAOSMOVER_API UChaosMoverSimulation();

	// Returns the local simulation input MoverDataCollection, to read local non networked data passed to the simulation by the gameplay thread
	UFUNCTION(BlueprintPure, Category = Mover)
	CHAOSMOVER_API const FMoverDataCollection& GetLocalSimInput() const;

	// Returns the local simulation input MoverDataCollection, to pass local non networked data to the simulation
	// Only available from the gameplay thread
	UFUNCTION(BlueprintPure, Category = Mover)
	CHAOSMOVER_API FMoverDataCollection& GetLocalSimInput_Mutable();

	UFUNCTION(BlueprintPure, Category = Mover)
	CHAOSMOVER_API const UBaseMovementMode* GetCurrentMovementMode() const;

	UFUNCTION(BlueprintPure, Category = Mover)
	CHAOSMOVER_API const UBaseMovementMode* FindMovementModeByName(const FName& Name) const;

	UFUNCTION(BlueprintCallable, CustomThunk, Category = Mover, meta = (CustomStructureParam = "EffectAsRawData", AllowAbstract = "false", DisplayName = "Queue Instant Movement Effect"))
	void K2_QueueInstantMovementEffect(UPARAM(DisplayName = "Instant Movement Effect") const int32& EffectAsRawData);
	DECLARE_FUNCTION(execK2_QueueInstantMovementEffect);

	// Queue a Instant Movement Effect to take place at the end of this frame or start of the next subtick - whichever happens first
	CHAOSMOVER_API void QueueInstantMovementEffect(TSharedPtr<FInstantMovementEffect> Move);

	struct FInitParams
	{
		TMap<FName, TWeakObjectPtr<UBaseMovementMode>> ModesToRegister;
		TArray<TWeakObjectPtr<UBaseMovementModeTransition>> TransitionsToRegister;
		FMoverSyncState InitialSyncState;
		FName StartingMovementMode = NAME_None;
		TWeakObjectPtr<UNullMovementMode> NullMovementMode = nullptr;
		TWeakObjectPtr<UImmediateMovementModeTransition> ImmediateModeTransition = nullptr;
		TWeakObjectPtr<UMovementMixer> MovementMixer = nullptr;
		Chaos::FCharacterGroundConstraintProxy* CharacterConstraintProxy = nullptr;
		Chaos::FJointConstraintPhysicsProxy* PathTargetConstraintProxy = nullptr;
		Chaos::FSingleParticlePhysicsProxy* PathTargetKinematicEndPointProxy = nullptr;
		Chaos::FConstPhysicsObjectHandle PhysicsObject = nullptr;
		Chaos::FPhysicsSolver* Solver = nullptr;
		UWorld* World = nullptr;
	};

	CHAOSMOVER_API void Init(const FInitParams& InitParams);
	CHAOSMOVER_API void Deinit();
	CHAOSMOVER_API void SimulationTick(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, UE::ChaosMover::FSimulationOutputData& OutputData);
	CHAOSMOVER_API void ModifyContacts(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, const UE::ChaosMover::FSimulationOutputData& OutputData, Chaos::FCollisionContactModifier& Modifier) const;

	CHAOSMOVER_API void AddEvent(TSharedPtr<FMoverSimulationEventData> Event);

	CHAOSMOVER_API void InitNetInputData(const FMoverInputCmdContext& InNetInputCmd);
	CHAOSMOVER_API void ApplyNetInputData(const FMoverInputCmdContext& InNetInputCmd);
	CHAOSMOVER_API void BuildNetInputData(FMoverInputCmdContext& OutNetInputCmd) const;
	CHAOSMOVER_API void ApplyNetStateData(const FMoverSyncState& InNetSyncState);
	CHAOSMOVER_API void BuildNetStateData(FMoverSyncState& OutNetSyncState) const;

	//~ Debugging Util functions
	// Collection for holding extra debug data, that will be sent to the Chaos Visual Debugger for debugging
	UFUNCTION(BlueprintPure, Category = Mover)
	CHAOSMOVER_API FMoverDataCollection& GetDebugSimData();
	//~ End of Debugging Util functions

protected:
	CHAOSMOVER_API virtual void OnInit();
	CHAOSMOVER_API virtual void OnDeinit();
	CHAOSMOVER_API virtual void OnPreSimulationTick(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData);
	CHAOSMOVER_API virtual void OnSimulationTick(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, UE::ChaosMover::FSimulationOutputData& OutputData);
	CHAOSMOVER_API virtual void OnPostSimulationTick(const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationOutputData& OutputData);
	CHAOSMOVER_API virtual void OnModifyContacts(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, const UE::ChaosMover::FSimulationOutputData& OutputData, Chaos::FCollisionContactModifier& Modifier) const;

	// Character-like movement sim steps
	CHAOSMOVER_API virtual void PreSimulationTickCharacter(const IChaosCharacterMovementModeInterface& CharacterMode, const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData);
	CHAOSMOVER_API virtual void PostSimulationTickCharacter(const IChaosCharacterMovementModeInterface& CharacterMode, const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationOutputData& OutputData);
	CHAOSMOVER_API virtual void PostSimulationTickCharacterConstraint(const IChaosCharacterConstraintMovementModeInterface& CharacterConstraintMode, const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationOutputData& OutputData);

	// Pathed movement sim steps (kinematic or constrained)
	CHAOSMOVER_API virtual void PostSimulationTickPathedMovement(const IChaosPathedMovementModeInterface& ConstraintMode, const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationOutputData& OutputData);

	CHAOSMOVER_API Chaos::FPBDRigidParticleHandle* GetControlledParticle() const;

	CHAOSMOVER_API virtual void ProcessSimulationEvent(const FMoverSimulationEventData& ModeChangedData);
	CHAOSMOVER_API virtual void OnMovementModeChanged(const FMovementModeChangedEventData& ModeChangedData);

	// Object State.
	// In kinematic mode, the updated primitive component needs both its SimulatePhysics and UpdateKinematicFromSimulation options checked
	CHAOSMOVER_API void SetControlledParticleDynamic();
	CHAOSMOVER_API void SetControlledParticleKinematic();
	CHAOSMOVER_API bool IsControlledParticleDynamic() const;
	CHAOSMOVER_API bool IsControlledParticleKinematic() const;

	// Character ground constraint
	CHAOSMOVER_API void EnableCharacterConstraint();
	CHAOSMOVER_API void DisableCharacterConstraint();

	// Path target constraint
	CHAOSMOVER_API void EnablePathTargetConstraint();
	CHAOSMOVER_API void DisablePathTargetConstraint();

	CHAOSMOVER_API void TraceMoverData(const UE::ChaosMover::FSimulationOutputData& OutputData);

	// State structs
	FMoverSyncState CurrentSyncState;
	// Data internal to the simulation
	FMoverDataCollection InternalSimData;
	// Local input data, usually sent by the gameplay side locally, that is not expected to differ from that on the server so doesn't warrant networking
	FMoverDataCollection LocalSimInput;
	// Debug Data collection, sent to Chaos Visual Debugger when Trace Extra Sim Debug Data is selected
	FMoverDataCollection DebugSimData;

	// Movement mode state machine
	UE::ChaosMover::FMoverStateMachine StateMachine;

	// Optional movement mixer
	TWeakObjectPtr<UMovementMixer> MovementMixerWeakPtr = nullptr;

	// Controlled physics object
	Chaos::FConstPhysicsObjectHandle PhysicsObject = nullptr;

	// Character ground constraint
	Chaos::FCharacterGroundConstraintProxy* CharacterConstraintProxy = nullptr;

	// Path target constraint, for movement following a path in a physics based manner
	Chaos::FJointConstraintPhysicsProxy* PathTargetConstraintProxy = nullptr;
	Chaos::FSingleParticlePhysicsProxy* PathTargetKinematicEndPointProxy = nullptr;

	Chaos::FPhysicsSolver* Solver = nullptr;
	UWorld* World = nullptr;

private:
	FMoverInputCmdContext InputCmd;
	TArray<TSharedPtr<FMoverSimulationEventData>> Events;

	bool bInputCmdOverridden = false;
};
