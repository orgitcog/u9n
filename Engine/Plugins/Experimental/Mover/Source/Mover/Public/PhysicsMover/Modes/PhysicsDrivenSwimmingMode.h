// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/SwimmingMode.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#include "PhysicsDrivenSwimmingMode.generated.h"

#define UE_API MOVER_API


namespace Chaos { class FCharacterGroundConstraint; }

/**
 * PhysicsDrivenSwimmingMode: Override base kinematic Swimming mode for physics based motion.
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType)
class UPhysicsDrivenSwimmingMode : public USwimmingMode, public IPhysicsCharacterMovementModeInterface
{
	GENERATED_UCLASS_BODY()

public:
	UE_API virtual void OnRegistered(const FName ModeName) override;
	UE_API virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	UE_API virtual bool AttemptJump(float UpwardsSpeed, FMoverTickEndData& Output) override;

	UE_API virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const override;

#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	virtual float GetTargetHeight() const override { return TargetHeight; }

	UE_API virtual void SetTargetHeightOverride(float InTargetHeight) override;
	UE_API virtual void ClearTargetHeightOverride() override;

protected:
	/**
	 * Optional override target height for the character (the desired distance from the center of the capsule to the floor).
	 * If left blank, the -Z offset of the owning character's skeletal mesh comp will be used automatically.
	 */
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	TOptional<float> TargetHeightOverride;

	// Radius used for ground queries
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float QueryRadius = 30.0f;
	
	float TargetHeight = 95.f;
};

#undef UE_API
