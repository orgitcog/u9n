// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PathedMovement/ArcRotationPathPattern.h"

#include "DebugRenderSceneProxy.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsDebugDrawComponent.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsMoverComponent.h"

FVector CalcArcPoint(const FTransform& PathOriginTransform, const FVector& RotationAxis, float ArcRadius, float ArcAngleDeg)
{
	FTransform ArcPointTransform(FQuat(RotationAxis, FMath::DegreesToRadians(ArcAngleDeg)));
	ArcPointTransform.Accumulate(PathOriginTransform);
	return ArcPointTransform.TransformPositionNoScale(FVector(ArcRadius, 0.f, 0.f));
}

void UArcRotationPattern::AppendDebugDrawElements(UPathedPhysicsDebugDrawComponent& DebugDrawComp, FBoxSphereBounds::Builder& InOutDebugBoundsBuilder)
{
	constexpr float BoundsDrawRadius = 500.f;
	const FTransform& PathOriginTransform = GetPathedMoverComp().GetPathOriginTransform();

	// Draw dashed lines for the min and max bounds of the arc
	const FVector ArcMinEndpoint = CalcArcPoint(PathOriginTransform, RotationAxis, BoundsDrawRadius, 0.f);
	InOutDebugBoundsBuilder += ArcMinEndpoint;

	const FVector ArcMaxEndpoint = CalcArcPoint(PathOriginTransform, RotationAxis, BoundsDrawRadius, RotationArcAngle);
	InOutDebugBoundsBuilder += ArcMinEndpoint;
	
	const FVector OriginLoc = PathOriginTransform.GetLocation();
	DebugDrawComp.DebugDashedLines.Emplace(OriginLoc, ArcMinEndpoint, PatternDebugDrawColor, 5.f);
	DebugDrawComp.DebugDashedLines.Emplace(OriginLoc, ArcMaxEndpoint, PatternDebugDrawColor, 5.f);

	// Draw a curve indicating the rotation between the bounds
	constexpr float RotationArrowDrawRadius = BoundsDrawRadius * 0.75f;
	
	FVector PrevArrowPoint = CalcArcPoint(PathOriginTransform, RotationAxis, RotationArrowDrawRadius, 0.f);
	const int32 NumCurvePoints = FMath::CeilToInt32(FMath::Lerp(3.f, 20.f, RotationArcAngle / 360.f));
	for (int32 PointIdx = 1; PointIdx <= NumCurvePoints; ++PointIdx)
	{
		const float Angle = FMath::Lerp(0.f, RotationArcAngle, float(PointIdx) / NumCurvePoints);
		const FVector ArrowPoint = CalcArcPoint(PathOriginTransform, RotationAxis, RotationArrowDrawRadius, Angle);

		DebugDrawComp.DebugLines.Emplace(PrevArrowPoint, ArrowPoint, PatternDebugDrawColor, 2.f);
		if (PointIdx == 1 && PerLoopBehavior == EPathedPhysicsPlaybackBehavior::ThereAndBack)
		{
			DebugDrawComp.DebugArrowLines.Emplace(ArrowPoint, PrevArrowPoint, PatternDebugDrawColor, 5.f);
		}
		else if (PointIdx == NumCurvePoints)
		{
			DebugDrawComp.DebugArrowLines.Emplace(PrevArrowPoint, ArrowPoint, PatternDebugDrawColor, 5.f);
		}

		PrevArrowPoint = ArrowPoint;
	}
}

FTransform UArcRotationPattern::CalcUnmaskedTargetRelativeTransform(float PatternProgress, const FTransform& CurTargetTransform) const
{
	const float TargetAngle = RotationArcAngle * PatternProgress;
	return FTransform(FQuat(RotationAxis, FMath::DegreesToRadians(TargetAngle)));
}
