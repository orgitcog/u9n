// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/ChaosMovementMode.h"

#include "ChaosMover/ChaosMovementModeTransition.h"
#include "ChaosMover/ChaosMoverLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosMovementMode)

UChaosMovementMode::UChaosMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Simulation(nullptr)
{
}

void UChaosMovementMode::SetSimulation(UChaosMoverSimulation* InSimulation)
{
	Simulation = InSimulation;
	for (TObjectPtr<UBaseMovementModeTransition>& Transition : Transitions)
	{
		if (UChaosMovementModeTransition* ChaosTransition = Cast<UChaosMovementModeTransition>(Transition.Get()))
		{
			ChaosTransition->SetSimulation(InSimulation);
		}
	}
}