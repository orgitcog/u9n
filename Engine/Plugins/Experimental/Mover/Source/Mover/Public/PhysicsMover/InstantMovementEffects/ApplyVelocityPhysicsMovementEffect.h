// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstantMovementEffect.h"
#include "ApplyVelocityPhysicsMovementEffect.generated.h"

#define UE_API MOVER_API

/** Apply Velocity: provides a velocity to the actor after (optionally) forcing them into a particular movement mode
  * Note: this only applies the velocity for one tick!
  */
USTRUCT(BlueprintType, Category = "Mover|Instant Movement Effect|Physics", DisplayName = "Apply Velocity Physics Instant Movement Effect")
struct FApplyVelocityPhysicsEffect : public FInstantMovementEffect
{
	GENERATED_BODY()

	UE_API FApplyVelocityPhysicsEffect();
	virtual ~FApplyVelocityPhysicsEffect() {}

	// Velocity to apply to the actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ForceUnits = "cm/s"))
	FVector VelocityToApply;

	// If true VelocityToApply will be added to current velocity on this actor. If false velocity will be set directly to VelocityToApply
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bAdditiveVelocity;

	// Optional movement mode name to force the actor into before applying the impulse velocity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FName ForceMovementMode;

	UE_API virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;

	UE_API virtual FInstantMovementEffect* Clone() const override;

	UE_API virtual void NetSerialize(FArchive& Ar) override;

	UE_API virtual UScriptStruct* GetScriptStruct() const override;

	UE_API virtual FString ToSimpleString() const override;

	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

#undef UE_API
