// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/ChaosMovementModeTransition.h"

#include "ChaosMover/ChaosMoverLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosMovementModeTransition)

UChaosMovementModeTransition::UChaosMovementModeTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Simulation(nullptr)
{
}