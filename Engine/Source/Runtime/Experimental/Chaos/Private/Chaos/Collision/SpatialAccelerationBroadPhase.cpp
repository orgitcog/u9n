// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Collision/SpatialAccelerationBroadPhase.h"

namespace Chaos
{
	namespace CVars
	{
		int32 NumWorkerCollisionFactor = 2;
		FAutoConsoleVariableRef CVarNumWorkerCollisionFactor(TEXT("p.Chaos.NumWorkerCollisionFactor"), NumWorkerCollisionFactor, TEXT("Set the number of tasks created for collision detection per worker."));
	}
}