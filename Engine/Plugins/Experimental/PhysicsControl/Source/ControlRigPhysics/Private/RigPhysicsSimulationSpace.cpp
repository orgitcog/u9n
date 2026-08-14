// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigPhysicsSimulation.h"
#include "RigPhysicsSolverComponent.h"

#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "ControlRig.h"

#include "PhysicsControlHelpers.h"

#include "Physics/ImmediatePhysics/ImmediatePhysicsActorHandle.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsJointHandle.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsSimulation.h"

#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ShapeInstance.h"
#include "Chaos/Capsule.h"
#include "Chaos/PBDJointConstraintUtilities.h"

#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/BodySetup.h"

//======================================================================================================================
FORCEINLINE_DEBUGGABLE FTransform GetSpaceTransform(
	ERigPhysicsSimulationSpace Space, const FTransform& ComponentTM, const FTransform& BoneTM)
{
	switch (Space)
	{
	case ERigPhysicsSimulationSpace::Component: return ComponentTM;
	case ERigPhysicsSimulationSpace::World: return FTransform::Identity;
	case ERigPhysicsSimulationSpace::SpaceBone: return BoneTM * ComponentTM;
	default: ensureMsgf(false, TEXT("Unsupported Simulation Space")); return FTransform::Identity;
	}
}

//======================================================================================================================
FTransform FRigPhysicsSimulation::GetSimulationSpaceTransform(const FRigPhysicsSolverSettings& SolverSettings) const
{
	switch (SolverSettings.SimulationSpace)
	{
	case ERigPhysicsSimulationSpace::Component: return SimulationSpaceState.ComponentTM;
	case ERigPhysicsSimulationSpace::World: return FTransform::Identity;
	case ERigPhysicsSimulationSpace::SpaceBone: return SimulationSpaceState.BoneRelComponentTM * SimulationSpaceState.ComponentTM;
	default: ensureMsgf(false, TEXT("Unsupported Simulation Space")); return FTransform::Identity;
	}
}

//======================================================================================================================
FTransform FRigPhysicsSimulation::ConvertComponentSpaceTransformToSimSpace(
	const FRigPhysicsSolverSettings& SolverSettings, const FTransform& TM) const
{
	switch (SolverSettings.SimulationSpace)
	{
	case ERigPhysicsSimulationSpace::Component: return TM;
	case ERigPhysicsSimulationSpace::World:  return TM * SimulationSpaceState.ComponentTM;
	case ERigPhysicsSimulationSpace::SpaceBone: return TM.GetRelativeTransform(SimulationSpaceState.BoneRelComponentTM);
	default: ensureMsgf(false, TEXT("Unsupported Simulation Space")); return TM;
	}
}

//======================================================================================================================
FTransform FRigPhysicsSimulation::ConvertSimSpaceTransformToComponentSpace(
	const FRigPhysicsSolverSettings& SolverSettings, const FTransform& TM) const
{
	switch (SolverSettings.SimulationSpace)
	{
	case ERigPhysicsSimulationSpace::Component: return TM;
	case ERigPhysicsSimulationSpace::World:  return TM.GetRelativeTransform(SimulationSpaceState.ComponentTM);
	case ERigPhysicsSimulationSpace::SpaceBone: return TM * SimulationSpaceState.BoneRelComponentTM;
	default: ensureMsgf(false, TEXT("Unsupported Simulation Space")); return TM;
	}
}

//======================================================================================================================
FORCEINLINE_DEBUGGABLE FVector ConvertWorldVectorToSimSpaceNoScale(
	ERigPhysicsSimulationSpace Space, const FVector& WorldVector, 
	const FTransform& ComponentTM, const FTransform& BoneTM)
{
	switch (Space)
	{
	case ERigPhysicsSimulationSpace::Component: return ComponentTM.InverseTransformVectorNoScale(WorldVector);
	case ERigPhysicsSimulationSpace::World: return WorldVector;
	case ERigPhysicsSimulationSpace::SpaceBone:
		return BoneTM.InverseTransformVectorNoScale(ComponentTM.InverseTransformVectorNoScale(WorldVector));
	default: ensureMsgf(false, TEXT("Unsupported Simulation Space")); return WorldVector;
	}
}

//======================================================================================================================
FVector FRigPhysicsSimulation::ConvertWorldVectorToSimSpaceNoScale(
	const FRigPhysicsSolverSettings& SolverSettings, const FVector& WorldVector) const
{
	switch (SolverSettings.SimulationSpace)
	{
	case ERigPhysicsSimulationSpace::Component: return SimulationSpaceState.ComponentTM.InverseTransformVectorNoScale(WorldVector);
	case ERigPhysicsSimulationSpace::World: return WorldVector;
	case ERigPhysicsSimulationSpace::SpaceBone:
		return SimulationSpaceState.BoneRelComponentTM.InverseTransformVectorNoScale(
			SimulationSpaceState.ComponentTM.InverseTransformVectorNoScale(WorldVector));
	default: ensureMsgf(false, TEXT("Unsupported Simulation Space")); return WorldVector;
	}
}

//======================================================================================================================
FTransform FRigPhysicsSimulation::ConvertCollisionSpaceTransformToSimSpace(
	const FRigPhysicsSolverSettings& SolverSettings, const FTransform& TM) const
{
	FTransform SimSpaceTM = GetSpaceTransform(
		SolverSettings.SimulationSpace, 
		SimulationSpaceState.ComponentTM, 
		SimulationSpaceState.BoneRelComponentTM);
	FTransform CollisionSpaceTM = GetSpaceTransform(
		SolverSettings.CollisionSpace, 
		SimulationSpaceState.ComponentTM, 
		SimulationSpaceState.BoneRelComponentTM);

	FTransform WorldSpaceTM = TM * CollisionSpaceTM;
	return WorldSpaceTM.GetRelativeTransform(SimSpaceTM);
}

//======================================================================================================================
void FRigPhysicsSimulation::InitSimulationSpace(
	const FTransform& ComponentTM,
	const FTransform& BoneRelComponentTM)
{
	SimulationSpaceState.ComponentTM = ComponentTM;
	SimulationSpaceState.BoneRelComponentTM = BoneRelComponentTM;
}

//======================================================================================================================
// Note - don't use the space conversion functions here as the state won't have been set yet.
FRigPhysicsSimulation::FSimulationSpaceData FRigPhysicsSimulation::UpdateSimulationSpaceStateAndCalculateData(
	const FRigPhysicsSolverComponent* SolverComponent, const float Dt)
{
	const FRigPhysicsSolverSettings& SolverSettings = SolverComponent->SolverSettings;
	const FRigPhysicsSimulationSpaceSettings& SimulationSpaceSettings = SolverComponent->SimulationSpaceSettings;

	if (const USceneComponent* OwningSceneComponent = OwningControlRig->GetOwningSceneComponent())
	{
		SimulationSpaceState.ComponentTM = OwningSceneComponent->GetComponentTransform();
	}
	else 
	{
		SimulationSpaceState.ComponentTM.SetIdentity();
	}

	if (SolverSettings.SimulationSpace == ERigPhysicsSimulationSpace::SpaceBone &&  SolverSettings.SpaceBone.IsValid())
	{
		URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();
		SimulationSpaceState.BoneRelComponentTM = Hierarchy->GetGlobalTransform(SolverSettings.SpaceBone);
	}

	// Record the history - but avoid polluting it with zero Dt updates. What that means is - if we
	// get a zero-Dt update, then just update our current sim space TM, which means the time delta
	// from the previous state is actually the current Dt (i.e. don't overwrite the current Dt).
	if (Dt > SMALL_NUMBER)
	{
		SimulationSpaceState.PrevDt = SimulationSpaceState.Dt;
		SimulationSpaceState.Dt = Dt;

		SimulationSpaceState.PrevPrevSimulationSpaceTM = SimulationSpaceState.PrevSimulationSpaceTM;
		SimulationSpaceState.PrevSimulationSpaceTM = SimulationSpaceState.SimulationSpaceTM;
	}
	SimulationSpaceState.SimulationSpaceTM = GetSpaceTransform(
		SolverSettings.SimulationSpace, SimulationSpaceState.ComponentTM, SimulationSpaceState.BoneRelComponentTM);

	SimulationSpaceData = FSimulationSpaceData();
	SimulationSpaceData.Gravity = ::ConvertWorldVectorToSimSpaceNoScale(
		SolverSettings.SimulationSpace, SolverSettings.Gravity, 
		SimulationSpaceState.ComponentTM, SimulationSpaceState.BoneRelComponentTM);

	if (SolverSettings.SimulationSpace == ERigPhysicsSimulationSpace::World)
	{
		// Danny TODO This is probably redundant unless we support runtime switching of the space
		InitSimulationSpace(SimulationSpaceState.ComponentTM, SimulationSpaceState.BoneRelComponentTM);
		SimulationSpaceData.LinearVelocity = SimulationSpaceSettings.ExternalLinearVelocity;
		SimulationSpaceData.AngularVelocity = SimulationSpaceSettings.ExternalAngularVelocity;
		return SimulationSpaceData;
	}

	// If the timestep is zero, then it doesn't actually matter what the velocity is - but make sure
	// it doesn't corrupt anything.
	if (SimulationSpaceState.Dt < SMALL_NUMBER)
	{
		SimulationSpaceData.LinearVelocity = FVector::ZeroVector;
		SimulationSpaceData.AngularVelocity = FVector::ZeroVector;
		SimulationSpaceData.LinearAcceleration = FVector::ZeroVector;
		SimulationSpaceData.AngularAcceleration = FVector::ZeroVector;
		return SimulationSpaceData;
	}

	// We calculate velocities etc in world space first, and then subsequently convert them into
	// simulation space.

	// Note that the velocity/accel calculations are intended to track the world/simulation behavior
	// - not necessarily be the most accurate calculations! For example, we could use one-sided
	// finite difference approximations, but this wouldn't necessarily be correct.

	// World-space component linear velocity and acceleration
	SimulationSpaceData.LinearVelocity = UE::PhysicsControl::CalculateLinearVelocity(
		SimulationSpaceState.PrevSimulationSpaceTM.GetTranslation(),
		SimulationSpaceState.SimulationSpaceTM.GetTranslation(), SimulationSpaceState.Dt);
	const FVector PrevSpaceLinearVel = 
		SimulationSpaceState.PrevDt < SMALL_NUMBER 
		? SimulationSpaceData.LinearVelocity 
		: UE::PhysicsControl::CalculateLinearVelocity(
			SimulationSpaceState.PrevPrevSimulationSpaceTM.GetTranslation(),
			SimulationSpaceState.PrevSimulationSpaceTM.GetTranslation(), SimulationSpaceState.PrevDt);
	SimulationSpaceData.LinearAcceleration =
		(SimulationSpaceData.LinearVelocity - PrevSpaceLinearVel) / SimulationSpaceState.Dt;

	// World-space component angular velocity and acceleration
	SimulationSpaceData.AngularVelocity = UE::PhysicsControl::CalculateAngularVelocity(
		SimulationSpaceState.PrevSimulationSpaceTM.GetRotation(),
		SimulationSpaceState.SimulationSpaceTM.GetRotation(), SimulationSpaceState.Dt);
	const FVector PrevSpaceAngularVel = 
		SimulationSpaceState.PrevDt < SMALL_NUMBER 
		? SimulationSpaceData.AngularVelocity
		: UE::PhysicsControl::CalculateAngularVelocity(
			SimulationSpaceState.PrevPrevSimulationSpaceTM.GetRotation(),
			SimulationSpaceState.PrevSimulationSpaceTM.GetRotation(), SimulationSpaceState.PrevDt);
	SimulationSpaceData.AngularAcceleration =
		(SimulationSpaceData.AngularVelocity - PrevSpaceAngularVel) / SimulationSpaceState.Dt;

	// Apply Z scale
	SimulationSpaceData.LinearVelocity.Z *= SimulationSpaceSettings.VelocityScaleZ;
	SimulationSpaceData.LinearAcceleration.Z *= SimulationSpaceSettings.VelocityScaleZ;

	bool bLinearAccelerationTrigger = SimulationSpaceSettings.LinearAccelerationThresholdForTeleport > 0 &&
		SimulationSpaceData.LinearAcceleration.SquaredLength() >
		FMath::Square(SimulationSpaceSettings.LinearAccelerationThresholdForTeleport);
	bool bAngularAccelerationTrigger = SimulationSpaceSettings.AngularAccelerationThresholdForTeleport > 0 &&
		SimulationSpaceData.AngularAcceleration.SquaredLength() >
		FMath::Square(FMath::DegreesToRadians(SimulationSpaceSettings.AngularAccelerationThresholdForTeleport));
	bool bPositionTrigger = SimulationSpaceSettings.PositionChangeThresholdForTeleport > 0 &&
		SimulationSpaceData.LinearVelocity.SquaredLength() >
		FMath::Square(SimulationSpaceSettings.PositionChangeThresholdForTeleport / SimulationSpaceState.Dt);
	bool bOrientationTrigger = SimulationSpaceSettings.OrientationChangeThresholdForTeleport > 0 &&
		SimulationSpaceData.AngularVelocity.SquaredLength() >
		FMath::Square(FMath::DegreesToRadians(
			SimulationSpaceSettings.OrientationChangeThresholdForTeleport / SimulationSpaceState.Dt));

	// Clamped world-space motion of the simulation space
	if (bLinearAccelerationTrigger || bAngularAccelerationTrigger || bPositionTrigger || bOrientationTrigger)
	{
		if (bLinearAccelerationTrigger)
		{
			UE_LOG(LogRigPhysics, Log, TEXT("Detected linear Acceleration (%f > %f) teleport in %s"),
				SimulationSpaceData.LinearAcceleration.Length(), 
				SimulationSpaceSettings.LinearAccelerationThresholdForTeleport,
				*OwningControlRig->GetName());
		}
		if (bAngularAccelerationTrigger)
		{
			UE_LOG(LogRigPhysics, Log, TEXT("Detected angular Acceleration (%f > %f) teleport in %s"),
				SimulationSpaceData.AngularAcceleration.Length(),
				SimulationSpaceSettings.AngularAccelerationThresholdForTeleport,
				*OwningControlRig->GetName());
		}
		if (bPositionTrigger)
		{
			UE_LOG(LogRigPhysics, Log, TEXT("Detected position (%f > %f) teleport in %s"),
				SimulationSpaceData.LinearVelocity.Length() * SimulationSpaceState.Dt,
				SimulationSpaceSettings.PositionChangeThresholdForTeleport,
				*OwningControlRig->GetName());
		}
		if (bOrientationTrigger)
		{
			UE_LOG(LogRigPhysics, Log, TEXT("Detected orientation (%f > %f) teleport in %s"),
				FMath::RadiansToDegrees(SimulationSpaceData.AngularVelocity.Length() * SimulationSpaceState.Dt),
				SimulationSpaceSettings.OrientationChangeThresholdForTeleport,
				*OwningControlRig->GetName());
		}

		// Note that a teleport detection shouldn't change the pose, or the current motion. We just
		// don't want to bring in that unwanted global motion.
		SimulationSpaceData.LinearVelocity = FVector::ZeroVector;
		SimulationSpaceData.AngularVelocity = FVector::ZeroVector;
		SimulationSpaceData.LinearAcceleration = FVector::ZeroVector;
		SimulationSpaceData.AngularAcceleration = FVector::ZeroVector;

		// This will stop the next step from using bogus values too.
		SimulationSpaceState.PrevSimulationSpaceTM = SimulationSpaceState.SimulationSpaceTM;
		SimulationSpaceState.PrevPrevSimulationSpaceTM = SimulationSpaceState.SimulationSpaceTM;
		SimulationSpaceState.PrevDt = 0;
		SimulationSpaceState.Dt = 0;
		// Avoid cached transforms being used in controls by bumping the update counter. 
		UpdateCounter += 1;
	}
	else
	{
		if (SimulationSpaceSettings.bClampLinearVelocity)
		{
			SimulationSpaceData.LinearVelocity =
				SimulationSpaceData.LinearVelocity.GetClampedToMaxSize(SimulationSpaceSettings.MaxLinearVelocity);
		}
		if (SimulationSpaceSettings.bClampAngularVelocity)
		{
			SimulationSpaceData.AngularVelocity =
				SimulationSpaceData.AngularVelocity.GetClampedToMaxSize(SimulationSpaceSettings.MaxAngularVelocity);
		}
		if (SimulationSpaceSettings.bClampLinearAcceleration)
		{
			SimulationSpaceData.LinearAcceleration =
				SimulationSpaceData.LinearAcceleration.GetClampedToMaxSize(SimulationSpaceSettings.MaxLinearAcceleration);
		}
		if (SimulationSpaceSettings.bClampAngularAcceleration)
		{
			SimulationSpaceData.AngularAcceleration =
				SimulationSpaceData.AngularAcceleration.GetClampedToMaxSize(SimulationSpaceSettings.MaxAngularAcceleration);
		}
	}

	SimulationSpaceData.LinearVelocity += SimulationSpaceSettings.ExternalLinearVelocity;
	SimulationSpaceData.AngularVelocity += SimulationSpaceSettings.ExternalAngularVelocity;

	// Transform world-space motion into simulation space Danny TODO note that this matches the code
	// in RBAN, and is doing what the interface requires (i.e. movement of the space in the space of
	// the space!). 
	SimulationSpaceData.LinearVelocity =
		SimulationSpaceState.SimulationSpaceTM.InverseTransformVector(SimulationSpaceData.LinearVelocity);
	SimulationSpaceData.AngularVelocity =
		SimulationSpaceState.SimulationSpaceTM.InverseTransformVector(SimulationSpaceData.AngularVelocity);
	SimulationSpaceData.LinearAcceleration =
		SimulationSpaceState.SimulationSpaceTM.InverseTransformVector(SimulationSpaceData.LinearAcceleration);
	SimulationSpaceData.AngularAcceleration =
		SimulationSpaceState.SimulationSpaceTM.InverseTransformVector(SimulationSpaceData.AngularAcceleration);

	return SimulationSpaceData;
}
