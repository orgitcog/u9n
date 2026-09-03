// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "MoverSimulation.generated.h"

#define UE_API MOVER_API

class UMoverBlackboard;

/**
* WIP Base class for a Mover simulation.
* The simulation is intended to be the thing that updates the Mover
* state and should be safe to run on an async thread
*/
UCLASS(MinimalAPI, BlueprintType)
class UMoverSimulation : public UObject
{
	GENERATED_BODY()

public:
	UE_API UMoverSimulation();

	UFUNCTION(BlueprintPure, Category = Mover)
	UE_API const UMoverBlackboard* GetBlackboard() const;

	UFUNCTION(BlueprintPure, Category = Mover)
	UE_API UMoverBlackboard* GetBlackboard_Mutable();

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMoverBlackboard> Blackboard = nullptr;
};

#undef UE_API
