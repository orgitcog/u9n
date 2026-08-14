// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Backends/MoverBackendLiaison.h"
#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/Declares.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "Components/ActorComponent.h"

#include "ChaosMoverBackend.generated.h"

class UMoverComponent;
class UNetworkPhysicsComponent;

namespace Chaos
{
	class FCharacterGroundConstraint;
	class FCharacterGroundConstraintProxy;

	class FJointConstraint;
	class FJointConstraintPhysicsProxy;
}

UCLASS(MinimalAPI, Within = MoverComponent)
class UChaosMoverBackendComponent : public UActorComponent, public IMoverBackendLiaisonInterface
{
	GENERATED_BODY()

public:
	CHAOSMOVER_API UChaosMoverBackendComponent();

	CHAOSMOVER_API virtual void InitializeComponent() override;
	CHAOSMOVER_API virtual void UninitializeComponent() override;
	CHAOSMOVER_API virtual void BeginPlay() override;
	CHAOSMOVER_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	CHAOSMOVER_API virtual bool IsAsync() const override
	{
		return true;
	}

	// Unused IMoverBackendLiaisonInterface API
	CHAOSMOVER_API virtual float GetCurrentSimTimeMs()
	{
		return 0.0f;
	}
	CHAOSMOVER_API virtual int32 GetCurrentSimFrame()
	{
		return 0;
	}

	CHAOSMOVER_API virtual void ProduceInputData(int32 PhysicsStep, int32 NumSteps, const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationInputData& InputData);
	CHAOSMOVER_API virtual void ConsumeOutputData(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationOutputData& OutputData);
	CHAOSMOVER_API virtual void FinalizeFrame(float ResultsTimeInMs);

	UChaosMoverSimulation* GetSimulation()
	{
		return Simulation;
	}

	const UChaosMoverSimulation* GetSimulation() const
	{
		return Simulation;
	}

protected:
	CHAOSMOVER_API virtual void InitSimulation();
	CHAOSMOVER_API virtual void DeinitSimulation();
	CHAOSMOVER_API virtual void CreatePhysics();
	CHAOSMOVER_API virtual void DestroyPhysics();
	CHAOSMOVER_API void CreateCharacterGroundConstraint();
	CHAOSMOVER_API void DestroyCharacterGroundConstraint();
	CHAOSMOVER_API void CreatePathTargetConstraint();
	CHAOSMOVER_API void DestroyPathTargetConstraint();

	CHAOSMOVER_API virtual void GenerateInput(const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationInputData& InputData);

	CHAOSMOVER_API UMoverComponent& GetMoverComponent() const;
	CHAOSMOVER_API Chaos::FPhysicsSolver* GetPhysicsSolver() const;
	CHAOSMOVER_API Chaos::FPhysicsObject* GetPhysicsObject() const;
	CHAOSMOVER_API Chaos::FPBDRigidParticle* GetControlledParticle() const;

	UFUNCTION()
	CHAOSMOVER_API virtual void HandleUpdatedComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange);

	UFUNCTION()
	CHAOSMOVER_API void HandleOwningPawnControllerChanged_Server(APawn* OwnerPawn, AController* OldController, AController* NewController);

	UPROPERTY(Transient)
	TObjectPtr<UNetworkPhysicsComponent> NetworkPhysicsComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNullMovementMode> NullMovementMode;

	UPROPERTY(Transient)
	TObjectPtr<UImmediateMovementModeTransition> ImmediateModeTransition;

	UPROPERTY(Transient)
	TObjectPtr<UChaosMoverSimulation> Simulation;

	UE::Mover::FSimulationOutputRecord SimOutputRecord;

	// Character ground constraint, for moving on ground like characters
	TUniquePtr<Chaos::FCharacterGroundConstraint> CharacterGroundConstraint;

	// Path target constraint, for moving along a path
	FPhysicsConstraintHandle PathTargetConstraintHandle;
	FConstraintInstance PathTargetConstraintInstance;
	FPhysicsUserData PathTargetConstraintPhysicsUserData;

	bool bIsUsingAsyncPhysics = false;
	bool bWantsDestroySim = false;
	bool bWantsCreateSim = false;
};