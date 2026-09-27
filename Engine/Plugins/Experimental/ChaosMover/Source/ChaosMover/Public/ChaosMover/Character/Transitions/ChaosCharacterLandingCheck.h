// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosMover/ChaosMovementModeTransition.h"

#include "ChaosCharacterLandingCheck.generated.h"


UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class CHAOSMOVER_API UChaosCharacterLandingCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UChaosCharacterLandingCheck(const FObjectInitializer& ObjectInitializer);

	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;

	/** Depth at which the pawn starts swimming */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters, meta = (Units = "cm"))
	float SwimmingStartImmersionDepth = 64.5f;

	/** Height at which we consider the character to be on the ground */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters, meta = (Units = "cm"))
	float FloorDistanceTolerance = 0.5f;

	/** Name of movement mode to transition to when landing on ground */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters)
	FName TransitionToGroundMode;
};