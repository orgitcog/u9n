// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PathedMovement/PointMovementPathPattern.h"

#include "DebugRenderSceneProxy.h"
#include "Kismet/KismetMathLibrary.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsDebugDrawComponent.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsMoverComponent.h"

void UPointMovementPathPattern::InitializePattern()
{
	Super::InitializePattern();

	RefreshAssignedPointProgress(true);
}

#if WITH_EDITOR
void UPointMovementPathPattern::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshAssignedPointProgress(true);
}
#endif

void UPointMovementPathPattern::AppendDebugDrawElements(UPathedPhysicsDebugDrawComponent& DebugDrawComp, FBoxSphereBounds::Builder& InOutDebugBoundsBuilder)
{
	if (!PathPoints.IsEmpty())
	{
		const FTransform& PathOriginTransform = GetPathedMoverComp().GetPathOriginTransform();
		FVector PrevPointLocation = PathOriginTransform.GetLocation();
		for (const FPointMovementPathPoint& PathPoint : PathPoints)
		{
			DebugDrawComp.DebugSpheres.Emplace(4.f, PrevPointLocation, PatternDebugDrawColor);

			FVector PointLocation = PathPoint.WorldLoc;
			if (PathPoint.Basis == EPointMovementLocationBasis::PreviousPoint)
			{
				PointLocation = PrevPointLocation + PathPoint.Location;	
			}
			else if (PathPoint.Basis == EPointMovementLocationBasis::PathOrigin)
			{
				PointLocation = PathOriginTransform.TransformPositionNoScale(PathPoint.Location);
			}
			
			DebugDrawComp.DebugLines.Emplace(PrevPointLocation, PointLocation, PatternDebugDrawColor, 1.f);
			InOutDebugBoundsBuilder += PointLocation;
			
			PrevPointLocation = PointLocation;
		}

		DebugDrawComp.DebugSpheres.Emplace(4.f, PrevPointLocation, PatternDebugDrawColor);
	}
}

FTransform UPointMovementPathPattern::CalcUnmaskedTargetRelativeTransform(float PatternProgress, const FTransform& CurTargetTransform) const
{
	// RefreshAssignedPointProgress();
	
	// The destination point is the first point at or equal to the target progress
	int32 DestPointIdx = 0;
	for (; DestPointIdx < PathPoints.Num() && PathPoints[DestPointIdx].Progress < PatternProgress; ++DestPointIdx) {}

	if (PathPoints.IsValidIndex(DestPointIdx))
	{
		const FTransform& PathOrigin = GetPathedMoverComp().GetPathOriginTransform();
		
		const FPointMovementPathPoint& NextPoint = PathPoints[DestPointIdx];
		FPointMovementPathPoint PrevPoint;
		if (DestPointIdx > 0)
		{
			PrevPoint = PathPoints[DestPointIdx - 1];
		}
		else
		{
			PrevPoint.WorldLoc = PathOrigin.GetLocation();
		}

		const float ProgressSinceLastPoint = PatternProgress - PrevPoint.Progress;
		const float Alpha = ProgressSinceLastPoint / (NextPoint.Progress - PrevPoint.Progress);
		
		// Point locations are calculated in world space, but path targets need to be provided as relative transforms
		const FVector TargetWorldLoc = UKismetMathLibrary::VLerp(PrevPoint.WorldLoc, NextPoint.WorldLoc, Alpha);
		const FVector TargetRelativeLoc = PathOrigin.InverseTransformPositionNoScale(TargetWorldLoc);

		return FTransform(TargetRelativeLoc);
	}
	return FTransform::Identity;
}

void UPointMovementPathPattern::RefreshAssignedPointProgress(bool bForceRefresh) const
{
	if (!bHasAssignedPointProgress || bForceRefresh)
	{
		TotalPathDistance = 0.f;
		
		const FTransform& RootWorldTransform = GetPathedMoverComp().GetPathOriginTransform();
		FVector PrevPointLocation = RootWorldTransform.GetLocation();

		// Run through and establish point locations and distance
		for (const FPointMovementPathPoint& PathPoint : PathPoints)
		{
			if (PathPoint.Basis == EPointMovementLocationBasis::PreviousPoint)
			{
				PathPoint.WorldLoc = PrevPointLocation + PathPoint.Location;	
			}
			else if (PathPoint.Basis == EPointMovementLocationBasis::PathOrigin)
			{
				PathPoint.WorldLoc = RootWorldTransform.TransformPositionNoScale(PathPoint.Location);
			}
			
			const float DistanceFromPrevious = (PathPoint.WorldLoc - PrevPointLocation).Size();
			TotalPathDistance += DistanceFromPrevious;
			PathPoint.DistanceFromStart = TotalPathDistance;

			PrevPointLocation = PathPoint.WorldLoc;
		}

		// Go through again and assign progress based on distance
		for (const FPointMovementPathPoint& PathPoint : PathPoints)
		{
			PathPoint.Progress = UKismetMathLibrary::SafeDivide(PathPoint.DistanceFromStart, TotalPathDistance);
		}
		
		bHasAssignedPointProgress = true;
	}
}