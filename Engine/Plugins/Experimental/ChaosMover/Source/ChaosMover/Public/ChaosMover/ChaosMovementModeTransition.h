// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovementModeTransition.h"

#include "ChaosMovementModeTransition.generated.h"

class UChaosMoverSimulation;

/**
 * Base class for all Chaos movement mode transitions
 */
UCLASS(MinimalAPI, Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosMovementModeTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()

public:
	CHAOSMOVER_API UChaosMovementModeTransition(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = ChaosMover)
	const UChaosMoverSimulation* GetSimulation() const
	{
		return Simulation;
	}

	UFUNCTION(BlueprintPure, Category = ChaosMover)
	UChaosMoverSimulation* GetSimulation_Mutable()
	{
		return Simulation;
	}

	void SetSimulation(UChaosMoverSimulation* InSimulation)
	{
		Simulation = InSimulation;
	}

protected:
	UChaosMoverSimulation* Simulation;
};