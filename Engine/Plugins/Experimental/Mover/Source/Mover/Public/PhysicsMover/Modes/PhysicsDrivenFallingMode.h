// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/FallingMode.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#include "PhysicsDrivenFallingMode.generated.h"

#define UE_API MOVER_API


namespace Chaos { class FCharacterGroundConstraint; }

/**
 * PhysicsDrivenFallingMode: Override base kinematic falling mode for physics based motion.
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType)
class UPhysicsDrivenFallingMode : public UFallingMode, public IPhysicsCharacterMovementModeInterface
{
	GENERATED_UCLASS_BODY()

public:
	UE_API virtual void OnRegistered(const FName ModeName) override;
	UE_API virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	UE_API virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const override;

#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	virtual float GetTargetHeight() const override { return TargetHeight; }

	UE_API virtual void SetTargetHeightOverride(float InTargetHeight) override;
	UE_API virtual void ClearTargetHeightOverride() override;

protected:
	// Maximum torque the character can apply to rotate in air about the vertical axis
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "NewtonMeters"))
	float TwistTorqueLimit = 0.0f;

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
	float QueryRadius = 20.0f;

	float TargetHeight = 95.f;
};

#undef UE_API
