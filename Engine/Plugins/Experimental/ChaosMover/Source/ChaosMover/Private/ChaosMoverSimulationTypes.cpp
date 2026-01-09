// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMover/ChaosMoverSimulation.h"

void FChaosMoverSimulationDefaultInputs::Reset()
{
	CollisionResponseParams = FCollisionResponseParams();
	CollisionQueryParams = FCollisionQueryParams();
	UpDir = FVector::UpVector;
	Gravity = -980.7 * UpDir;
	PhysicsObjectGravity = 0.0f;
	PawnCollisionHalfHeight = 40.0f;
	PawnCollisionRadius = 30.0f;
	PhysicsObject = nullptr;
	OwningActor = nullptr;
	World = nullptr;
	CollisionChannel = ECC_Pawn;
}

FMoverDataCollection& UE::ChaosMover::GetDebugSimData(UChaosMoverSimulation* Simulation)
{
	check(Simulation);
	return Simulation->GetDebugSimData();
}