// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovementModeTransition.h"
#include "PhysicsJumpCheck.generated.h"

#define UE_API MOVER_API

/**
 * Transition that handles jumping based on input for a physics-based character
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType)
class UPhysicsJumpCheck : public UBaseMovementModeTransition
{
	GENERATED_UCLASS_BODY()


public:
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;

	/** Instantaneous speed induced in an actor upon jumping */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float JumpUpwardsSpeed = 500.0f;

	/**
	* Controls how much of the jump impulse the character will apply to the ground.
	* A value of 0 means no impulse will be applied to the ground.
	* A value of 1 means that the full equal and opposite jump impulse will be applied.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters, meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalGroundReactionImpulse = 1.0f;

	/** Name of movement mode to transition to when jumping is activated */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters)
	FName TransitionToMode;

#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR


};

#undef UE_API
