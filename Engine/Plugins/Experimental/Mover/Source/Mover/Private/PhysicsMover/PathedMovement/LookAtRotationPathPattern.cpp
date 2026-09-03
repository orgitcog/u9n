// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PathedMovement/LookAtRotationPathPattern.h"

#include "DebugRenderSceneProxy.h"
#include "Kismet/KismetMathLibrary.h"
#include "PhysicsMover/PathedMovement/PathedMovementMode.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsDebugDrawComponent.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsMoverComponent.h"


void ULookAtRotationPattern::AppendDebugDrawElements(UPathedPhysicsDebugDrawComponent& DebugDrawComp, FBoxSphereBounds::Builder& InOutDebugBoundsBuilder)
{
	const FVector LookAtLocation = GetPathedMoverComp().GetPathOriginTransform().TransformPositionNoScale(RelativeLookAtLocation);
	DebugDrawComp.DebugStars.Emplace(LookAtLocation, PatternDebugDrawColor, 20.f);
	InOutDebugBoundsBuilder += LookAtLocation;
}

FTransform ULookAtRotationPattern::CalcUnmaskedTargetRelativeTransform(float PatternProgress, const FTransform& CurTargetTransform) const
{
	//@todo DanH: So this is a modifier that has nothing to do with our progress... is that ok as a pattern or should it be something/somewhere else?
	//		Maybe it needs to be on the mode itself? Depending on how far the component is lagging behind the target, the actual rotation is gonna be different, and we might want to force it?
	
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(CurTargetTransform.GetLocation(), RelativeLookAtLocation);
	return FTransform(LookAtRotation);
}

void ULookAtRotationPattern::SetRelativeLookAtLocation(const FVector& RelativeLookAt)
{
	RelativeLookAtLocation = RelativeLookAt;
}

void ULookAtRotationPattern::SetLookAtLocation(const FVector& WorldLookAt)
{
	const FVector RelativeLookAt = GetPathedMoverComp().GetPathOriginTransform().InverseTransformPosition(WorldLookAt);
	SetRelativeLookAtLocation(RelativeLookAt);
}