// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PathedMovementPatternBase.h"

#include "LookAtRotationPathPattern.generated.h"

/** Stare at a single fixed point at all times (pairs nicely with ellipse if you want to always look at the center) */
UCLASS()
class ULookAtRotationPattern : public UPathedMovementPatternBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = LookAtPattern)
	void SetRelativeLookAtLocation(const FVector& RelativeLookAt);

	UFUNCTION(BlueprintCallable, Category = LookAtPattern)
	void SetLookAtLocation(const FVector& LookAt);

	virtual bool DebugDrawUsingStepSamples() const override { return false; }
	virtual void AppendDebugDrawElements(UPathedPhysicsDebugDrawComponent& DebugDrawComp, FBoxSphereBounds::Builder& InOutDebugBoundsBuilder) override;
	
protected:
	virtual FTransform CalcUnmaskedTargetRelativeTransform(float PatternProgress, const FTransform& CurTargetTransform) const override;
	
	/** The location relative to the path origin to always look at while moving along the path */
	UPROPERTY(EditAnywhere, Category = LookAtPattern)
	FVector RelativeLookAtLocation = FVector::ZeroVector;

	//@todo DanH: Option to target an actor (or component maybe?)
};
