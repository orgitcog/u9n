// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ParticleHandleFwd.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ChaosGroundMovementUtils.generated.h"

struct FFloorCheckResult;

UCLASS()
class CHAOSMOVER_API UChaosGroundMovementUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Computes the local velocity at the supplied position of the hit object in floor result */
	UFUNCTION(BlueprintCallable, Category = ChaosMover)
	static FVector ComputeLocalGroundVelocity_Internal(const FVector& Position, const FFloorCheckResult& FloorResult);

	static Chaos::FPBDRigidParticleHandle* GetRigidParticleHandleFromFloorResult_Internal(const FFloorCheckResult& FloorResult);
};