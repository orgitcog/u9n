// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoverSimulation.h"

#include "MoveLibrary/MoverBlackboard.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverSimulation)

UMoverSimulation::UMoverSimulation()
{
	Blackboard = CreateDefaultSubobject<UMoverBlackboard>(TEXT("MoverSimulationBlackboard"));
}

const UMoverBlackboard* UMoverSimulation::GetBlackboard() const
{
	return Blackboard;
}

UMoverBlackboard* UMoverSimulation::GetBlackboard_Mutable()
{
	return Blackboard;
}