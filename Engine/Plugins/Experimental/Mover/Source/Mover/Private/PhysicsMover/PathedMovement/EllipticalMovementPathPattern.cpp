// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PathedMovement/EllipticalMovementPathPattern.h"

#include "HAL/IConsoleManager.h"
#include "PhysicsMover/PathedMovement/PathedMovementMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EllipticalMovementPathPattern)

void UEllipticalMovementPathPattern::InitializePattern()
{
	Super::InitializePattern();
}

FTransform UEllipticalMovementPathPattern::CalcUnmaskedTargetRelativeTransform(float PatternProgress, const FTransform& CurTargetTransform) const
{
	// Calc the ellipse point
	const float Angle = PatternProgress * FMath::DegreesToRadians(UsableArcAngle);
	FVector2f TargetLocationXY(RadiusX * FMath::Cos(Angle), RadiusY * FMath::Sin(Angle));

	// Scooch the center over so 0 progress means no movement
	TargetLocationXY.X -= RadiusX;

	// Spin about the path origin as desired
	TargetLocationXY = TargetLocationXY.GetRotated(OriginAngle);

	// Rotate the ellipse from the XY plane to whatever 3D plane is desired
	const FVector TargetLocation = EllipsePlaneRotation.RotateVector(FVector(TargetLocationXY.X, TargetLocationXY.Y, 0.f));
	return FTransform(TargetLocation);
}
