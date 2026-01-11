// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PathedMovementPatternBase.h"
#include "Engine/EngineTypes.h"

#include "SplineMovementPathPattern.generated.h"

class USplineComponent;

/** This isn't formal "spline metadata" because that requires a custom spline and component - we want to add info to vanilla splines */
USTRUCT()
struct FSplinePathPatternPointData
{
	GENERATED_BODY()

	/** Key of the spline point (or somewhere between two points) where this metadata applies */
	UPROPERTY()
	float SplinePointKey = 0.f;

	//@todo DanH: Speed cap, pausing, per-section easing, (show a true readonly total duration that accounts for them too)
};

UCLASS()
class USplineMovementPathPattern : public UPathedMovementPatternBase
{
	GENERATED_BODY()
	
public:
	virtual void InitializePattern() override;

	//@todo DanH: Allow patterns to state a desired duration and either complain if there are multiple or go with whatever is longest
	//virtual TOptional<float> GetDesiredPathDuration() const override;

	// The spline is already drawn, no need to draw it again at lower rez
	virtual bool DebugDrawUsingStepSamples() const override { return false; }
	
	bool HasValidBounds() const { return UpperBound > LowerBound; }
	
protected:
	virtual FTransform CalcUnmaskedTargetRelativeTransform(float PatternProgress, const FTransform& CurTargetTransform) const override;

	/** How far into the spline the movement path actually begins.  */
	UPROPERTY(EditAnywhere, Category = SplinePathPattern, meta = (UIMin = 0.f, ClampMin = 0.f, UIMax = 1.f, ClampMax = 1.f))
	float LowerBound = 0.f;

	/** How far into the spline the movement path ends. Must be greater than the lower bound. */
	UPROPERTY(EditAnywhere, Category = SplinePathPattern, meta = (UIMin = 0.f, ClampMin = 0.f, UIMax = 1.f, ClampMax = 1.f))
	float UpperBound = 1.f;

	//@todo DanH: Expose this as an alternative to an explicit time - when true it effectively sets the optional desired time
	/** If true, the duration property on the spline will be used as the path duration */
	// UPROPERTY(EditAnywhere, Category = SplinePathPattern)
	// bool bUseSplineDuration = false;

	/**
	 * If true, the path duration will be shortened according to how much of the spline is not being followed.
	 * If false (default), the path duration is unchanged, so the object will move slower when the usable spline range is reduced.
	 */
	UPROPERTY(EditAnywhere, Category = SplinePathPattern)
	bool bOffsetsModifyDuration = false;

	UPROPERTY(EditAnywhere, Category = SplinePathPattern)
	bool bApplySplineScaling = false;
	
	/**
	 * Optional property to specify the spline component that defines the path to follow. If blank, we'll use the first spline component we find in this actor.
	 * This is only necessary to set if you have multiple spline components on the actor, or want to follow a spline on an external actor.
	 */
	UPROPERTY(EditAnywhere, DisplayName = "Spline Component", Category = SplinePathPattern, meta = (AllowAnyActor, UseComponentPicker, AllowedClasses = "/Script/Engine.SplineComponent"))
	FComponentReference SplineComponentRef;

	UPROPERTY(Transient)
	TObjectPtr<const USplineComponent> SplineComp;
};
