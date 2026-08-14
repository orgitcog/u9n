// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PathedMovement/SplineMovementPathPattern.h"

#include "Components/SplineComponent.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsMoverComponent.h"

void USplineMovementPathPattern::InitializePattern()
{
	Super::InitializePattern();

	if (!SplineComp)
	{
		if (AActor* OwningActor = GetPathedMoverComp().GetOwner(); ensure(OwningActor))
		{
			SplineComp = Cast<USplineComponent>(SplineComponentRef.GetComponent(OwningActor));
			if (!SplineComp)
			{
				SplineComp = OwningActor->FindComponentByClass<USplineComponent>();
			}
		}
	}
}

//TOptional<float> USplineMovementPathPattern::GetPatternDuration() const
//{
//	TOptional<float> Duration = Super::GetPatternDuration();
//	if (bUseSplineDuration)
//	{
//		if (const USplineComponent* SplineComponent = GetSpline())
//		{
//			Duration = SplineComponent->Duration;
//		}
//	}
//	
//	if (const float UsableRange = UpperBound - LowerBound; bOffsetsModifyDuration && UsableRange < 1.f && UsableRange > 0.f)
//	{
//		const float BaseDuration = Duration.Get(GetMovementMode().GetDefaultPathDuration());
//		return BaseDuration * UsableRange;
//	}
//	return Duration;
//}

FTransform USplineMovementPathPattern::CalcUnmaskedTargetRelativeTransform(float PatternProgress, const FTransform& CurTargetTransform) const
{
	if (SplineComp)
	{
		const float SplineLength = SplineComp->GetSplineLength();

		const bool bHasValidBounds = HasValidBounds();
		const float SplineStartDist = bHasValidBounds ? SplineLength * LowerBound : 0.f;
		const float SplineEndDist = bHasValidBounds ? SplineLength * UpperBound : 1.f;

		const float DistanceAlongSpline = FMath::Lerp(SplineStartDist, SplineEndDist, PatternProgress);
		FTransform TargetToSpline = SplineComp->GetTransformAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local, bApplySplineScaling);

		if (!bOrientComponentToPath)
		{
			TargetToSpline.SetRotation(FQuat::Identity);
		}
		
		// Intentionally not accounting for any offset between the path origin and the spline's transform - zero progress should always equate to the path origin
		return TargetToSpline;
	}
	return FTransform::Identity;
}