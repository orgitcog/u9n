// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/WalkingMode.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#include "PhysicsDrivenWalkingMode.generated.h"

#define UE_API MOVER_API


namespace Chaos { class FCharacterGroundConstraint; }
struct FWaterCheckResult;

/**
 * PhysicsDrivenWalkingMode: Override base kinematic walking mode for physics based motion.
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType)
class UPhysicsDrivenWalkingMode : public UWalkingMode, public IPhysicsCharacterMovementModeInterface
{
	GENERATED_UCLASS_BODY()

public:
	UE_API virtual void OnRegistered(const FName ModeName) override;
	UE_API virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	UE_API virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const override;
	UE_API virtual void OnContactModification_Internal(const FPhysicsMoverSimulationContactModifierParams& Params, Chaos::FCollisionContactModifier& Modifier) const override;

#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	virtual float GetTargetHeight() const override { return TargetHeight; }

	UE_API virtual void SetTargetHeightOverride(float InTargetHeight) override;
	UE_API virtual void ClearTargetHeightOverride() override;
	
protected:
	// Maximum force the character can apply to reach the motion target
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "Newtons"))
	float RadialForceLimit = 1500.0f;

	// Maximum force the character can apply to hold in place while standing on an unwalkable incline
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "Newtons"))
	float FrictionForceLimit = 100.0f;

	// Maximum torque the character can apply to rotate in air about the vertical axis
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "NewtonMeters"))
	float TwistTorqueLimit = 1000.0f;

	// Maximum torque the character can apply to remain upright
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "NewtonMeters"))
	float SwingTorqueLimit = 3000.0f;

	/**
	 * Optional override target height for the character (the desired distance from the center of the capsule to the floor).
	 * If left blank, the -Z offset of the owning character's skeletal mesh comp will be used automatically.
	 */
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	TOptional<float> TargetHeightOverride;

	// Radius used for ground queries
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float QueryRadius = 30.0f;

	// Damping factor to control the softness of the interaction between the character and the ground
	// Set to 0 for no damping and 1 for maximum damping
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float GroundDamping = 0.0f;

	// Scaling applied to the radial force limit to raise the limit to always allow the character to
	// reach the motion target/
	// A value of 1 means that the radial force limit will be increased as needed to match the force
	// required to achieve the motion target.
	// A value of 0 means no scaling will be applied.
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalRadialForceLimitScaling = 1.0f;

	// Controls the reaction force applied to the ground in the ground plane when the character is moving
	// A value of 1 means that the full reaction force is applied
	// A value of 0 means the character only applies a normal force to the ground and no tangential force
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalGroundReaction = 1.0f;

	// Controls how much downward velocity is applied to keep the character rooted to the ground when the character
	// is within MaxStepHeight of the ground surface.
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalDownwardVelocityToTarget = 1.0f;

	// Time limit for being unsupported before moving from a walking to a falling state.
	// This provides some grace period when walking off of an edge during which locomotion
	// and jumping are still possible even though the character has started falling under gravity
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "s"))
	float MaxUnsupportedTimeBeforeFalling = 0.06f;

	// This setting is relevant for pawns landing on sloped surfaces. When enabled, no sliding occurs
	// for vertical landing velocities. When disabled, the sliding is governed by the friction forces.
	UPROPERTY(EditAnywhere, Category = "Physics Mover")
	bool bHandleVerticalLandingSeparately = true;

	float TargetHeight = 95.f;
	
protected:
	UE_API void SwitchToState(const FName& StateName, const FSimulationTickParams& Params, FMoverTickEndData& OutputState);

	UE_API bool CanStepUpOnHitSurface(const FFloorCheckResult& FloorResult) const;

	UE_API void FloorCheck(const FMoverDefaultSyncState& SyncState, const FProposedMove& ProposedMove, UPrimitiveComponent* UpdatedPrimitive, float DeltaSeconds,
		FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult, FVector& OutDeltaPos) const;
};

#undef UE_API
