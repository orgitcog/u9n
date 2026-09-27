// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoveLibrary/FloorQueryUtils.h"

#include "ChaosCharacterMoverComponent.generated.h"

// Fired after the actor lands on a valid surface. First param is the name of the mode this actor is in after landing. Second param is the hit result from hitting the floor.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FChaosMover_OnLanded, const FName&, NextMovementModeName, const FHitResult&, HitResult);


UCLASS(MinimalAPI, BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class UChaosCharacterMoverComponent : public UCharacterMoverComponent
{
	GENERATED_BODY()

public:
	CHAOSMOVER_API UChaosCharacterMoverComponent();

	CHAOSMOVER_API virtual bool TryGetFloorCheckHitResult(FHitResult& OutHitResult) const override;
	CHAOSMOVER_API virtual void ProduceInput(const int32 DeltaTimeMS, FMoverInputCmdContext* Cmd) override;

	// Broadcast when this actor lands on a valid surface.
	UPROPERTY(BlueprintAssignable, Category = "Chaos Mover")
	FChaosMover_OnLanded OnLanded;

	// Launch the character using either impulse or velocity
	// Note: This will only trigger a launch if a launch transition is implemented on the current movement mode
	UFUNCTION(BlueprintCallable, Category = "Chaos Mover")
	void Launch(const FVector& VelocityOrImpulse, EChaosMoverVelocityEffectMode Mode = EChaosMoverVelocityEffectMode::AdditiveVelocity);

protected:
	CHAOSMOVER_API virtual void ProcessSimulationEvent(const FMoverSimulationEventData& EventData) override;
	CHAOSMOVER_API virtual void SetAdditionalSimulationOutput(const FMoverDataCollection& Data) override;

	bool bFloorResultSet = false;
	FFloorCheckResult LatestFloorResult;

	FVector LaunchVelocityOrImpulse = FVector::ZeroVector;
	EChaosMoverVelocityEffectMode LaunchMode = EChaosMoverVelocityEffectMode::AdditiveVelocity;
};
