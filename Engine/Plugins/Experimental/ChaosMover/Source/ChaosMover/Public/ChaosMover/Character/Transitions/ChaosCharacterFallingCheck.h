// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosMover/ChaosMovementModeTransition.h"

#include "ChaosCharacterFallingCheck.generated.h"


UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class CHAOSMOVER_API UChaosCharacterFallingCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UChaosCharacterFallingCheck(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegistered() override;
	virtual void OnUnregistered() override;

	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;

	/** Name of movement mode to transition to when landing on ground */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters)
	FName TransitionToFallingMode;

	// Time limit for being unsupported before moving from a walking to a falling state.
	// This provides some grace period when walking off of an edge during which locomotion
	// and jumping are still possible even though the character has started falling under gravity
	UPROPERTY(EditAnywhere, Category = Parameters, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "s"))
	float MaxUnsupportedTimeBeforeFalling = 0.06f;

protected:
	TObjectPtr<const class USharedChaosCharacterMovementSettings> SharedSettings;
};