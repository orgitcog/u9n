// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/Utilities/ChaosGroundMovementUtils.h"

#include "Chaos/PhysicsObjectInternalInterface.h"
#include "MoveLibrary/FloorQueryUtils.h"

FVector UChaosGroundMovementUtils::ComputeLocalGroundVelocity_Internal(const FVector& Position, const FFloorCheckResult& FloorResult)
{
	FVector GroundVelocity = FVector::ZeroVector;
	if (const Chaos::FPBDRigidParticleHandle* Rigid = GetRigidParticleHandleFromFloorResult_Internal(FloorResult))
	{
		Chaos::FRigidTransform3 ComTransform = Rigid->GetTransformXRCom();
		FVector Offset = Position - ComTransform.GetLocation();
		Offset -= Offset.ProjectOnToNormal(FloorResult.HitResult.ImpactNormal);
		GroundVelocity = Rigid->GetV() + Rigid->GetW().Cross(Offset);
	}
	return GroundVelocity;
}

Chaos::FPBDRigidParticleHandle* UChaosGroundMovementUtils::GetRigidParticleHandleFromFloorResult_Internal(const FFloorCheckResult& FloorResult)
{
	if (Chaos::FPhysicsObjectHandle PhysicsObject = FloorResult.HitResult.PhysicsObject)
	{
		Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
		return Interface.GetRigidParticle(PhysicsObject);
	}

	return nullptr;
}
