// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "MovementMode.h"

#include "ChaosMovementMode.generated.h"

class UChaosMoverSimulation;
namespace Chaos
{
	class FCollisionContactModifier;
}

UENUM()
enum class EChaosMoverIgnoredCollisionMode : uint8
{
	EnableCollisionsWithIgnored,
	DisableCollisionsWithIgnored,
};

/**
 * Base class for all Chaos movement modes
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class CHAOSMOVER_API UChaosMovementMode : public UBaseMovementMode
{
	GENERATED_BODY()

public:
	UChaosMovementMode(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = ChaosMover)
	const UChaosMoverSimulation* GetSimulation() const
	{
		return Simulation;
	}

	void SetSimulation(UChaosMoverSimulation* InSimulation);

	virtual void ModifyContacts(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, const UE::ChaosMover::FSimulationOutputData& OutputData, Chaos::FCollisionContactModifier& Modifier) const
	{
	}

	UPROPERTY(EditAnywhere, Category = Collision)
	EChaosMoverIgnoredCollisionMode IgnoredCollisionMode = EChaosMoverIgnoredCollisionMode::DisableCollisionsWithIgnored;

protected:
	UChaosMoverSimulation* Simulation;
};