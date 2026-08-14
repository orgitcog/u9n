// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PathedMovementPatternBase.h"

#include "ArcRotationPathPattern.generated.h"

class UPathedPhysicsDebugDrawComponent;

UCLASS()
class UArcRotationPattern : public UPathedMovementPatternBase
{
	GENERATED_BODY()

public:
protected:
	virtual bool DebugDrawUsingStepSamples() const override { return false; }
	virtual void AppendDebugDrawElements(UPathedPhysicsDebugDrawComponent& DebugDrawComp, FBoxSphereBounds::Builder& InOutDebugBoundsBuilder) override;
	
	virtual FTransform CalcUnmaskedTargetRelativeTransform(float PatternProgress, const FTransform& CurTargetTransform) const override;

	/** The size of the arc angle to rotate along */
	UPROPERTY(EditAnywhere, Category = EllipticalPathPattern, meta = (Units = Degrees, UIMin = 0, ClampMin = 0, UIMax = 360, ClampMax = 360))
	float RotationArcAngle = 360.f;

	/** The axis to rotate about */
	UPROPERTY(EditAnywhere, Category = EllipticalPathPattern, meta = (UIMin = 0, ClampMin = 0, UIMax = 1.f, ClampMax = 1.f))
	FVector RotationAxis = FVector::UpVector;
};