// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Backends/MoverNetworkPhysicsLiaisonBase.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#include "PhysicsMover/PathedMovement/PathedMovementTypes.h"

#include "MoverPathedPhysicsLiaison.generated.h"

#define UE_API MOVER_API

class UPathedPhysicsMovementMode;
class UPathedPhysicsMoverComponent;
/**
 * Liaison that works with path-following movement modes.
 * Establishes a joint constraint between the actual component to update and wherever it should be along the path.
 * This allows the platform's velocity and rotation to be affected by physics as it moves along the path, and the joint pulls it back where it should be (think spring).
 * Only compatible with UPathedPhysicsMovementModes, which are responsible for dictating how "loose" the joint between the platform and its ideal position is.
 */
UCLASS(MinimalAPI, Within = PathedPhysicsMoverComponent)
class UMoverPathedPhysicsLiaisonComponent : public UMoverNetworkPhysicsLiaisonComponentBase
{
	GENERATED_BODY()

public:
	UE_API UMoverPathedPhysicsLiaisonComponent();

	UE_API virtual void InitializeComponent() override;
	UE_API virtual bool HasValidPhysicsState() const override;
	UE_API virtual void OnCreatePhysicsState() override;
	UE_API virtual void OnDestroyPhysicsState() override;
	
	UE_API virtual void ConsumeOutput_External(const FPhysicsMoverAsyncOutput& Output, const double OutputTimeInSeconds) override;
	UE_API virtual void PostPhysicsUpdate_External() override;

	const FTransform& GetPathOrigin() const { return Inputs_External.PathOrigin; }
	UE_API void SetPathOrigin(const FTransform& NewPathOrigin);

	bool IsInReverse() const { return Inputs_External.bIsInReverse; }
	UE_API void SetPlaybackDirection (bool bPlayForward);
	
	bool IsMoving() const { return Inputs_External.IsMoving(); }
	UE_API void SetIsMoving(bool bShouldMove, float StartDelay = 0.f);

	bool IsJointEnabled() const { return Inputs_External.bIsJointEnabled; }

	EPathedPhysicsPlaybackBehavior GetPlaybackBehavior() const { return Inputs_External.PlaybackBehavior; }
	UE_API void SetPlaybackBehavior(EPathedPhysicsPlaybackBehavior PlaybackBehavior);

	UE_API UPathedPhysicsMoverComponent& GetPathedMoverComp() const;
	
protected:
	UE_API virtual void HandleComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange) override;

	UE_API virtual void PerformProduceInput_External(float DeltaTime, OUT FPhysicsMoverAsyncInput& Input) override;
	UE_API virtual bool CanProcessInputs_Internal(const FPhysicsMoverAsyncInput& Input) const override;
	UE_API virtual void PerformProcessInputs_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const override;
	UE_API virtual bool CanSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input) const override;
	UE_API virtual void PerformPreSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, FPhysicsMoverAsyncOutput& Output) const override;

	UE_API virtual void ApplyPathModeConfig(const UPathedPhysicsMovementMode& PathedMode);
	
	UE_API Chaos::FJointConstraint* GetJointConstraint() const;
	
	UE_API void CreateTargetJoint();
	UE_API void DestroyTargetJoint();
	
protected:
	FPhysicsUserData PhysicsUserData;
	FConstraintInstance ConstraintInstance;
	FPhysicsConstraintHandle ConstraintHandle;

private:
	UFUNCTION()
	UE_API void HandleMovementModeChanged(const FName& OldModeName, const FName& NewModeName);
	UE_API void HandleIsUsingJointChanged(bool bIsUsingJoint);
	
	FMutablePathedMovementProperties Inputs_External;

	FTimerHandle DelayedStartTimerHandle;
};

#undef UE_API
