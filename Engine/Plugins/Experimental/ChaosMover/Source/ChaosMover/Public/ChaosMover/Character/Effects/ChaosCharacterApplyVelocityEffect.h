// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "InstantMovementEffect.h"

#include "ChaosCharacterApplyVelocityEffect.generated.h"


/** Applies a velocity or impulse for a single tick */
USTRUCT(BlueprintInternalUseOnly)
struct CHAOSMOVER_API FChaosCharacterApplyVelocityEffect : public FInstantMovementEffect
{
	GENERATED_BODY()

	// Velocity or impulse to apply to the actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FVector VelocityOrImpulseToApply = FVector::ZeroVector;

	// Controls whether to apply velocity or impulse and if the velocity will be additive
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	EChaosMoverVelocityEffectMode Mode = EChaosMoverVelocityEffectMode::AdditiveVelocity;

	virtual bool ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState);
	virtual FInstantMovementEffect* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};