// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "Backends/MoverNetworkPhysicsLiaisonBase.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#include "MoverNetworkPhysicsLiaison.generated.h"

#define UE_API MOVER_API

class UCommonLegacyMovementSettings;

//@todo DanH: Rename to UMoverCharacterPhysicsLiaisonComponent
//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponent

/**
 * MoverNetworkPhysicsLiaisonComponent: This component acts as a middleman between an actor's Mover component and the chaos physics network prediction system.
 * This class is set on a Mover component as the "back end".
 */
UCLASS(MinimalAPI)
class UMoverNetworkPhysicsLiaisonComponent : public UMoverNetworkPhysicsLiaisonComponentBase
{
	GENERATED_BODY()

public:
	UE_API UMoverNetworkPhysicsLiaisonComponent();

	// UObject interface
	UE_API virtual void OnRegister() override;
	UE_API virtual bool HasValidPhysicsState() const override;
	UE_API virtual void OnCreatePhysicsState() override;
	UE_API virtual void OnDestroyPhysicsState() override;

#if WITH_EDITOR
	UE_API virtual EDataValidationResult ValidateData(FDataValidationContext& Context, const UMoverComponent& ValidationMoverComp) const override;
#endif
	
	UE_API virtual void ConsumeOutput_External(const FPhysicsMoverAsyncOutput& Output, const double OutputTimeInSeconds) override;
	UE_API virtual void PostPhysicsUpdate_External() override;
	UE_API virtual void OnContactModification_Internal(const FPhysicsMoverAsyncInput& Input, Chaos::FCollisionContactModifier& Modifier) const override;

protected:
	UE_API virtual bool HasValidState() const override;

	UE_API virtual bool CanProcessInputs_Internal(const FPhysicsMoverAsyncInput& Input) const override;
	UE_API virtual void PerformProcessInputs_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const override;
	UE_API virtual bool CanSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input) const override;
	UE_API virtual void PerformPreSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, FPhysicsMoverAsyncOutput& Output) const override;
	UE_API virtual Chaos::FPhysicsObject* GetControlledPhysicsObject() const override;
	UE_API virtual void HandleComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange) override;
	
	UE_API void DestroyConstraint();
	UE_API void SetupConstraint();

	UE_API void UpdateConstraintSettings();
	
	TUniquePtr<Chaos::FCharacterGroundConstraint> Constraint;
	TObjectPtr<UCommonLegacyMovementSettings> CommonMovementSettings;
};

#undef UE_API
