// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovementModeTransition.h"
#include "PhysicsLaunchCheck.generated.h"

#define UE_API MOVER_API

/**
 * Transition that handles launching based on input for a physics-based character
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType)
class UPhysicsLaunchCheck : public UBaseMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UPhysicsLaunchCheck(const class FObjectInitializer& ObjectInitializer);

	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;

#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	/**
	Optional: Name of movement mode to transition to when launch is activated.
	If set to None launching will not change the current mode
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters)
	FName TransitionToMode;
};

#undef UE_API
