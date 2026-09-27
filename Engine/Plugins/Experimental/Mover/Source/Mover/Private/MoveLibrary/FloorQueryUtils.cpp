// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/FloorQueryUtils.h"

#include "MoverComponent.h"
#include "Components/CapsuleComponent.h"
#include "MoveLibrary/MovementUtils.h"
#include "Engine/World.h"

namespace UE::FloorQueryUtility
{

	const float MIN_FLOOR_DIST = 1.9f;	// Smallest distance we want our primitive floating above walkable floors while in ground-based movement.
	const float MAX_FLOOR_DIST = 2.4f;	// Largest distance we want our primitive floating above walkable floors while in ground-based movement.

	const float SWEEP_EDGE_REJECT_DISTANCE = 0.15f;
}

void UFloorQueryUtils::FindFloor(const FMovingComponentSet& MovingComps, float FloorSweepDistance, float MaxWalkSlopeCosine, const FVector& Location, FFloorCheckResult& OutFloorResult)
{
	if (!MovingComps.UpdatedComponent->IsQueryCollisionEnabled())
	{
		OutFloorResult.Clear();
		return;
	}

	// Sweep for the floor
	// TODO: Might need to plug in a different value for LineTraceDistance - using the same value as FloorSweepDistance for now - function takes both so we can plug in different values if needed
	ComputeFloorDist(MovingComps,FloorSweepDistance, FloorSweepDistance, MaxWalkSlopeCosine, Location, OutFloorResult);
}

void UFloorQueryUtils::ComputeFloorDist(const FMovingComponentSet& MovingComps, float LineTraceDistance, float FloorSweepDistance, float MaxWalkSlopeCosine, const FVector& Location, FFloorCheckResult& OutFloorResult)
{
	OutFloorResult.Clear();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComputeFloorDist), false, MovingComps.UpdatedPrimitive->GetOwner());
	FCollisionResponseParams ResponseParam;
	UMovementUtils::InitCollisionParams(MovingComps.UpdatedPrimitive.Get(), QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = MovingComps.UpdatedComponent->GetCollisionObjectType();

	// TODO: pluggable shapes
	float PawnRadius = 0.0f;
	float PawnHalfHeight = 0.0f;
	UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(MovingComps.UpdatedComponent);
	CapsuleComponent->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
	//MovingComps.UpdatedPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);
	FVector UpDirection = MovingComps.MoverComponent->GetUpDirection();

	bool bBlockingHit = false;
	
	// Sweep test
	if (FloorSweepDistance > 0.f)
	{
		// Use a shorter height to avoid sweeps giving weird results if we start on a surface.
		// This also allows us to adjust out of penetrations.
		const float ShrinkScale = 0.9f;
		const float ShrinkScaleOverlap = 0.1f;
		float ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScale);
		float TraceDist = FloorSweepDistance + ShrinkHeight;
		FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(PawnRadius, PawnHalfHeight - ShrinkHeight);

		FHitResult Hit(1.f);
		FVector SweepDirection = UpDirection * -TraceDist;
		bBlockingHit = FloorSweepTest(MovingComps, Hit, Location, Location + SweepDirection, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
			// Check 2D distance to impact point, reject if within a tolerance from radius.
			if (Hit.bStartPenetrating || !IsWithinEdgeTolerance(Location, Hit.ImpactPoint, CapsuleShape.Capsule.Radius, UpDirection))
			{
				// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
				// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
				CapsuleShape.Capsule.Radius = FMath::Max(0.f, CapsuleShape.Capsule.Radius - UE::FloorQueryUtility::SWEEP_EDGE_REJECT_DISTANCE - KINDA_SMALL_NUMBER);
				if (!CapsuleShape.IsNearlyZero())
				{
					ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
					TraceDist = FloorSweepDistance + ShrinkHeight;
					SweepDirection = UpDirection * -TraceDist;
					CapsuleShape.Capsule.HalfHeight = FMath::Max(PawnHalfHeight - ShrinkHeight, CapsuleShape.Capsule.Radius);
					Hit.Reset(1.f, false);

					bBlockingHit = FloorSweepTest(MovingComps, Hit, Location, Location + SweepDirection, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
				}
			}

			// Reduce hit distance by ShrinkHeight because we shrank the capsule for the trace.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			// JAH TODO: move magic numbers to a common location
			const float MaxPenetrationAdjust = FMath::Max(UE::FloorQueryUtility::MAX_FLOOR_DIST, PawnRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloorResult.SetFromSweep(Hit, SweepResult, false);
			if (Hit.IsValidBlockingHit() && IsHitSurfaceWalkable(Hit, UpDirection, MaxWalkSlopeCosine))
			{
				if (SweepResult <= FloorSweepDistance)
				{
					// Hit within test distance.
					OutFloorResult.bWalkableFloor = true;
					return;
				}
			}
		}
	}

	// Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything.
	// We do however want to try a line trace if the sweep was stuck in penetration.
	if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
	{
		OutFloorResult.FloorDist = FloorSweepDistance;
		return;
	}

	// Line trace
	if (LineTraceDistance > 0.f)
	{
		const float ShrinkHeight = PawnHalfHeight;
		const FVector LineTraceStart = Location;	
		const float TraceDist = LineTraceDistance + ShrinkHeight;
		const FVector Down = UpDirection * -TraceDist;
		QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

		FHitResult Hit(1.f);
		bBlockingHit = MovingComps.UpdatedComponent->GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, CollisionChannel, QueryParams, ResponseParam);
		
		if (bBlockingHit && Hit.Time > 0.f)
		{
			// Reduce hit distance by ShrinkHeight because we started the trace higher than the base.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			const float MaxPenetrationAdjust = FMath::Max(UE::FloorQueryUtility::MAX_FLOOR_DIST, PawnRadius);
			const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);
			
			OutFloorResult.bBlockingHit = true;
			if (LineResult <= LineTraceDistance && IsHitSurfaceWalkable(Hit, UpDirection, MaxWalkSlopeCosine))
			{
				OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
				return;
			}
		}
	}

	// No hits were acceptable.
	OutFloorResult.bWalkableFloor = false;
}

bool UFloorQueryUtils::FloorSweepTest(const FMovingComponentSet& MovingComps, FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionShape& CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam)
{
	bool bBlockingHit = false;

	if (MovingComps.UpdatedPrimitive.IsValid())
	{
		bBlockingHit = MovingComps.UpdatedPrimitive->GetWorld()->SweepSingleByChannel(OutHit, Start, End, MovingComps.UpdatedComponent->GetComponentRotation().Quaternion(), TraceChannel, CollisionShape, Params, ResponseParam);
	}

	return bBlockingHit;
}

bool UFloorQueryUtils::IsHitSurfaceWalkable(const FHitResult& Hit, const FVector& UpDirection, float MaxWalkSlopeCosine)
{
	if (!Hit.IsValidBlockingHit())
	{
		// No hit, or starting in penetration
		return false;
	}

	// Never walk up vertical surfaces.
	if (Hit.ImpactNormal.Dot(UpDirection) < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	float TestWalkableSlopeCosine = MaxWalkSlopeCosine;

	// See if this component overrides the walkable floor slope cosine.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (HitComponent)
	{
		const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
		TestWalkableSlopeCosine = SlopeOverride.ModifyWalkableFloorZ(TestWalkableSlopeCosine);
	}

	// Can't walk on this surface if it is too steep.
	if (Hit.ImpactNormal.Dot(UpDirection) < TestWalkableSlopeCosine)
	{
		return false;
	}

	return true;
}

bool UFloorQueryUtils::IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, float CapsuleRadius, const FVector& UpDirection)
{
	const float DistFromCenterSq = FVector::VectorPlaneProject((TestImpactPoint - CapsuleLocation), -UpDirection).SizeSquared();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(UE::FloorQueryUtility::SWEEP_EDGE_REJECT_DISTANCE + UE_KINDA_SMALL_NUMBER, CapsuleRadius - UE::FloorQueryUtility::SWEEP_EDGE_REJECT_DISTANCE));
	return DistFromCenterSq < ReducedRadiusSq;
}

void FFloorCheckResult::SetFromSweep(const FHitResult& InHit, const float InSweepFloorDist, const bool bIsWalkableFloor)
{
	bBlockingHit = InHit.IsValidBlockingHit();
	bWalkableFloor = bIsWalkableFloor;
	FloorDist = InSweepFloorDist;
	HitResult = InHit;
	bLineTrace = false;
	LineDist = 0.f;
}

void FFloorCheckResult::SetFromLineTrace(const FHitResult& InHit, const float InSweepFloorDist, const float InLineDist, const bool bIsWalkableFloor)
{
	// We require a sweep that hit if we are going to use a line result.
	ensure(HitResult.bBlockingHit);
	if (HitResult.bBlockingHit && InHit.bBlockingHit)
	{
		// Override most of the sweep result with the line result, but save some values
		FHitResult OldHit(HitResult);
		HitResult = InHit;

		// Restore some of the old values. We want the new normals and hit actor, however.
		HitResult.Time = OldHit.Time;
		HitResult.ImpactPoint = OldHit.ImpactPoint;
		HitResult.Location = OldHit.Location;
		HitResult.TraceStart = OldHit.TraceStart;
		HitResult.TraceEnd = OldHit.TraceEnd;
		bLineTrace = true;
		LineDist = InLineDist;
		
		FloorDist = InSweepFloorDist;
		bWalkableFloor = bIsWalkableFloor;
	}
}
