// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LearningAgentsGym.h"
#include "LearningAgentsGymSimple.generated.h"

class UStaticMeshComponent;

/**
 * A simple gym template class that uses a static mesh as its training floor.
 */
UCLASS(ClassGroup = (LearningAgents), BlueprintType, Blueprintable)
class LEARNINGAGENTSTRAINING_API ALearningAgentsGymSimple : public ALearningAgentsGymBase
{
	GENERATED_BODY()
	
public:
	ALearningAgentsGymSimple();

	UPROPERTY(EditDefaultsOnly, Category = "LearningAgents")
	TObjectPtr<UStaticMeshComponent> SimpleGymFloor;

	virtual void GetGymExtents(FVector& OutMinExtents, FVector& OutMaxExtents) const override;

	virtual FRotator GenerateRandomRotationInGym() const override;

	virtual FVector GenerateRandomLocationInGym() const override;

	virtual FVector ProjectPointToGym(const FVector& InPoint) const override;
};
