// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigPhysicsData.h"
#include "PhysicsControlObjectVersion.h"
#include "PhysicsControlHelpers.h"
#include "Chaos/ChaosConstraintSettings.h"

//======================================================================================================================
FArchive& ArchiveConstraintBaseParams(FArchive& Ar, FConstraintBaseParams& Data)
{
	Ar << Data.Stiffness;
	Ar << Data.Damping;
	Ar << Data.Restitution;
	Ar << Data.ContactDistance;
	bool bSoftConstraint = Data.bSoftConstraint;
	Ar << bSoftConstraint;
	Data.bSoftConstraint = bSoftConstraint;
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FLinearConstraint& Data)
{
	ArchiveConstraintBaseParams(Ar, Data);
	Ar << Data.Limit;
	Ar << Data.XMotion;
	Ar << Data.YMotion;
	Ar << Data.ZMotion;
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FConeConstraint& Data)
{
	ArchiveConstraintBaseParams(Ar, Data);
	Ar << Data.Swing1LimitDegrees;
	Ar << Data.Swing2LimitDegrees;
	Ar << Data.Swing1Motion;
	Ar << Data.Swing2Motion;
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FTwistConstraint& Data)
{
	ArchiveConstraintBaseParams(Ar, Data);
	Ar << Data.TwistLimitDegrees;
	Ar << Data.TwistMotion;
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FConstraintDrive& Data)
{
	Ar << Data.Stiffness;
	Ar << Data.Damping;
	Ar << Data.MaxForce;
	bool bEnablePositionDrive = Data.bEnablePositionDrive;
	bool bEnableVelocityDrive = Data.bEnableVelocityDrive;
	Ar << bEnablePositionDrive;
	Ar << bEnableVelocityDrive;
	Data.bEnablePositionDrive = bEnablePositionDrive;
	Data.bEnableVelocityDrive = bEnableVelocityDrive;
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FLinearDriveConstraint& Data)
{
	Ar << Data.PositionTarget;
	Ar << Data.VelocityTarget;
	Ar << Data.XDrive;
	Ar << Data.YDrive;
	Ar << Data.ZDrive;
	Ar << Data.bAccelerationMode;
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FAngularDriveConstraint& Data)
{
	Ar << Data.TwistDrive;
	Ar << Data.SwingDrive;
	Ar << Data.SlerpDrive;
	Ar << Data.OrientationTarget;
	Ar << Data.AngularVelocityTarget;
	Ar << Data.AngularDriveMode;
	Ar << Data.bAccelerationMode;
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FRigPhysicsJointData& Data)
{
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) < FPhysicsControlObjectVersion::ControlRigSeparateOutJointFromBody)
	{
		bool bEnable;
		FRigComponentKey ParentBody;
		Ar << bEnable;
		Ar << ParentBody;
	}
	Ar << Data.bAutoCalculateParentOffset;
	Ar << Data.bAutoCalculateChildOffset;
	Ar << Data.ExtraParentOffset;
	Ar << Data.ExtraChildOffset;
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) < FPhysicsControlObjectVersion::ControlRigSupportFullConstraintData)
	{
		float LinearLimit = 0.0f;
		FVector AngularLimit = FVector(-1.0);
		Ar << LinearLimit;
		Ar << AngularLimit;
		if (Ar.IsLoading())
		{
			Data.LinearConstraint.bSoftConstraint = false;
			Data.ConeConstraint.bSoftConstraint = false;
			Data.TwistConstraint.bSoftConstraint = false;

			Data.LinearConstraint.Limit = LinearLimit;
			Data.TwistConstraint.TwistLimitDegrees = AngularLimit.X;
			Data.ConeConstraint.Swing1LimitDegrees = AngularLimit.Y;
			Data.ConeConstraint.Swing2LimitDegrees = AngularLimit.Z;

			Data.LinearConstraint.XMotion = ELinearConstraintMotion::LCM_Limited;
			Data.LinearConstraint.YMotion = ELinearConstraintMotion::LCM_Limited;
			Data.LinearConstraint.ZMotion = ELinearConstraintMotion::LCM_Limited;

			if (Data.LinearConstraint.Limit == 0)
			{
				Data.LinearConstraint.XMotion = ELinearConstraintMotion::LCM_Locked;
			}
			else if (Data.LinearConstraint.Limit < 0)
			{
				Data.LinearConstraint.XMotion = ELinearConstraintMotion::LCM_Free;
			}

			Data.TwistConstraint.TwistMotion = EAngularConstraintMotion::ACM_Limited;
			Data.ConeConstraint.Swing1Motion = EAngularConstraintMotion::ACM_Limited;
			Data.ConeConstraint.Swing2Motion = EAngularConstraintMotion::ACM_Limited;

			if (Data.TwistConstraint.TwistLimitDegrees == 0)
			{
				Data.TwistConstraint.TwistMotion = EAngularConstraintMotion::ACM_Locked;
			}
			else if (Data.TwistConstraint.TwistLimitDegrees < 0)
			{
				Data.TwistConstraint.TwistMotion = EAngularConstraintMotion::ACM_Free;
			}

			if (Data.ConeConstraint.Swing1LimitDegrees == 0)
			{
				Data.ConeConstraint.Swing1Motion = EAngularConstraintMotion::ACM_Locked;
			}
			else if (Data.ConeConstraint.Swing1LimitDegrees < 0)
			{
				Data.ConeConstraint.Swing1Motion = EAngularConstraintMotion::ACM_Free;
			}

			if (Data.ConeConstraint.Swing2LimitDegrees == 0)
			{
				Data.ConeConstraint.Swing2Motion = EAngularConstraintMotion::ACM_Locked;
			}
			else if (Data.ConeConstraint.Swing2LimitDegrees < 0)
			{
				Data.ConeConstraint.Swing2Motion = EAngularConstraintMotion::ACM_Free;
			}

		}
	}
	else
	{
		Ar << Data.LinearConstraint;
		Ar << Data.ConeConstraint;
		Ar << Data.TwistConstraint;
	}
	Ar << Data.bDisableCollision;
	Ar << Data.LinearProjectionAmount;
	Ar << Data.AngularProjectionAmount;
	Ar << Data.ParentInverseMassScale;
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigIncludeDriveInJoint)
	{
		if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) < FPhysicsControlObjectVersion::ControlRigSeparateOutJointFromBody)
		{
			FRigPhysicsDriveData Drive;
			Ar << Drive;
		}
	}
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FRigPhysicsSimulationSpaceSettings& Data)
{
	Ar << Data.SpaceMovementAmount;
	Ar << Data.VelocityScaleZ;
	Ar << Data.bClampLinearVelocity;
	Ar << Data.MaxLinearVelocity;
	Ar << Data.bClampAngularVelocity;
	Ar << Data.MaxAngularVelocity;
	Ar << Data.bClampLinearAcceleration;
	Ar << Data.MaxLinearAcceleration;
	Ar << Data.bClampAngularAcceleration;
	Ar << Data.MaxAngularAcceleration;
	Ar << Data.LinearAccelerationThresholdForTeleport;
	Ar << Data.AngularAccelerationThresholdForTeleport;
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigDetectTeleportFromDistanceChange)
	{
		Ar << Data.PositionChangeThresholdForTeleport;
		Ar << Data.OrientationChangeThresholdForTeleport;
	}
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigDetectTeleportFromDistanceChange)
	{
		Ar << Data.LinearDragMultiplier;
		Ar << Data.AngularDragMultiplier;
	}
	Ar << Data.ExternalLinearDrag;
	Ar << Data.ExternalLinearVelocity;
	Ar << Data.ExternalAngularVelocity;
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FRigPhysicsSolverSettings& Data)
{
	Ar << Data.SimulationSpace;
	Ar << Data.CollisionSpace;
	Ar << Data.SpaceBone;
	Ar << Data.Collision;
	Ar << Data.Gravity;
	Ar << Data.PositionIterations;
	Ar << Data.VelocityIterations;
	Ar << Data.ProjectionIterations;
	Ar << Data.MaxNumRollingAverageStepTimes;
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigSolverSettingsIncludesCollisionBoundsExpansion)
	{
		Ar << Data.CollisionBoundsExpansion;
		Ar << Data.BoundsVelocityMultiplier;
		Ar << Data.MaxVelocityBoundsExpansion;
	}
	Ar << Data.MaxDepenetrationVelocity;
	Ar << Data.FixedTimeStep;
	Ar << Data.MaxTimeSteps;
	Ar << Data.MaxDeltaTime;
	Ar << Data.bUseLinearJointSolver;
	Ar << Data.bSolveJointPositionsLast;
	Ar << Data.bUseManifolds;
	Ar << Data.PositionThresholdForReset;
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigSpeedThresholdForReset)
	{
		Ar << Data.KinematicSpeedThresholdForReset;
	}
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigAccelerationThresholdForReset)
	{
		Ar << Data.KinematicAccelerationThresholdForReset;
	}
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) < FPhysicsControlObjectVersion::ControlRigRemoveResetCooldownFrames)
	{
		if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigResetCooldownFrames)
		{
			int ResetCooldownFrames = 0;
			Ar << ResetCooldownFrames;
		}
	}
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigAutomaticallyAddPhysicsComponents)
	{
		Ar << Data.bAutomaticallyAddPhysicsComponents;
	}
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FRigPhysicsDriveData& Data)
{
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigSupportFullDriveConstraintData)
	{
		Ar << Data.LinearDriveConstraint;
		Ar << Data.AngularDriveConstraint;
	}
	else
	{
		bool bEnable = false;
		float LinearStrength = 0.0f;
		float LinearDampingRatio = 1.0f;
		float LinearExtraDamping = 0.0f;
		float MaxForce = 0.0;
		float AngularStrength = 0.0f;
		float AngularDampingRatio = 1.0f;
		float AngularExtraDamping = 0.0f;
		float MaxTorque = 0.0f;
		
		Ar << bEnable;
		Ar << LinearStrength;
		Ar << LinearDampingRatio;
		Ar << LinearExtraDamping;
		Ar << MaxForce;
		Ar << AngularStrength;
		Ar << AngularDampingRatio;
		Ar << AngularExtraDamping;
		Ar << MaxTorque;

		if (Ar.IsLoading())
		{
			// Convert to the constraint drive params
			float AngularSpring;
			float AngularDamping;

			float LinearSpring;
			float LinearDamping;

			UE::PhysicsControl::ConvertStrengthToSpringParams(
				AngularSpring, AngularDamping,
				AngularStrength, AngularDampingRatio, AngularExtraDamping);
			UE::PhysicsControl::ConvertStrengthToSpringParams(
				LinearSpring, LinearDamping,
				LinearStrength, LinearDampingRatio, LinearExtraDamping);

			// Unfortunately, the physics engine will apply scalings to these values, so we need to
			// counter that.
			LinearSpring /= Chaos::ConstraintSettings::LinearDriveStiffnessScale();
			LinearDamping /= Chaos::ConstraintSettings::LinearDriveDampingScale();
			AngularSpring /= Chaos::ConstraintSettings::AngularDriveStiffnessScale();
			AngularDamping /= Chaos::ConstraintSettings::AngularDriveDampingScale();

			Data.LinearDriveConstraint.XDrive.Stiffness = LinearSpring;
			Data.LinearDriveConstraint.YDrive.Stiffness = LinearSpring;
			Data.LinearDriveConstraint.ZDrive.Stiffness = LinearSpring;

			Data.LinearDriveConstraint.XDrive.Damping = LinearDamping;
			Data.LinearDriveConstraint.YDrive.Damping = LinearDamping;
			Data.LinearDriveConstraint.ZDrive.Damping = LinearDamping;

			Data.LinearDriveConstraint.XDrive.MaxForce = MaxTorque;
			Data.LinearDriveConstraint.YDrive.MaxForce = MaxTorque;
			Data.LinearDriveConstraint.ZDrive.MaxForce = MaxTorque;

			Data.AngularDriveConstraint.SlerpDrive.Stiffness = AngularSpring;
			Data.AngularDriveConstraint.SwingDrive.Stiffness = AngularSpring;
			Data.AngularDriveConstraint.TwistDrive.Stiffness = AngularSpring;

			Data.AngularDriveConstraint.SlerpDrive.Damping = AngularDamping;
			Data.AngularDriveConstraint.SwingDrive.Damping = AngularDamping;
			Data.AngularDriveConstraint.TwistDrive.Damping = AngularDamping;

			Data.AngularDriveConstraint.SlerpDrive.MaxForce = MaxTorque;
			Data.AngularDriveConstraint.SwingDrive.MaxForce = MaxTorque;
			Data.AngularDriveConstraint.TwistDrive.MaxForce = MaxTorque;

			Data.LinearDriveConstraint.XDrive.bEnablePositionDrive = bEnable;
			Data.LinearDriveConstraint.YDrive.bEnablePositionDrive = bEnable;
			Data.LinearDriveConstraint.ZDrive.bEnablePositionDrive = bEnable;

			Data.LinearDriveConstraint.XDrive.bEnableVelocityDrive = bEnable;
			Data.LinearDriveConstraint.YDrive.bEnableVelocityDrive = bEnable;
			Data.LinearDriveConstraint.ZDrive.bEnableVelocityDrive = bEnable;

			Data.AngularDriveConstraint.SlerpDrive.bEnablePositionDrive = bEnable;
			Data.AngularDriveConstraint.SwingDrive.bEnablePositionDrive = bEnable;
			Data.AngularDriveConstraint.TwistDrive.bEnablePositionDrive = bEnable;

			Data.AngularDriveConstraint.SlerpDrive.bEnableVelocityDrive = bEnable;
			Data.AngularDriveConstraint.SwingDrive.bEnableVelocityDrive = bEnable;
			Data.AngularDriveConstraint.TwistDrive.bEnableVelocityDrive = bEnable;
		}
	}
	Ar << Data.SkeletalAnimationVelocityMultiplier;
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FRigPhysicsBodySolverSettings& Data)
{
	Ar << Data.PhysicsSolverComponentKey;
	Ar << Data.TargetBone;
	Ar << Data.SourceBone;
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigUseAutomaticSolver)
	{
		Ar << Data.bUseAutomaticSolver;
	}
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FRigPhysicsCollision& Data)
{
	Ar << Data.Boxes;
	Ar << Data.Spheres;
	Ar << Data.Capsules;
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigCollisionHasMaterial)
	{
		Ar << Data.Material;
	}
	return Ar;
}

//======================================================================================================================
void FRigPhysicsBodySolverSettings::OnRigHierarchyKeyChanged(
	const FRigHierarchyKey& InOldKey, const FRigHierarchyKey& InNewKey)
{
	if(InOldKey.IsComponent() && InNewKey.IsComponent())
	{
		if(PhysicsSolverComponentKey == InOldKey.GetComponent())
		{
			PhysicsSolverComponentKey = InNewKey.GetComponent();
		}
	}
	if(InOldKey.IsElement() && InNewKey.IsElement())
	{
		if(SourceBone == InOldKey.GetElement())
		{
			SourceBone = InNewKey.GetElement();
		}
		if(TargetBone == InOldKey.GetElement())
		{
			TargetBone = InNewKey.GetElement();
		}
	}
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FRigPhysicsDynamics& Data)
{
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigBodyDynamicsHasDensity)
	{
		Ar << Data.Density;
	}
	Ar << Data.MassOverride;
	Ar << Data.bOverrideCentreOfMass;
	Ar << Data.CentreOfMassOverride;
	Ar << Data.bOverrideMomentsOfInertia;
	Ar << Data.MomentsOfInertiaOverride;
	if (Ar.CustomVer(FPhysicsControlObjectVersion::GUID) >= FPhysicsControlObjectVersion::ControlRigSupportBodyDamping)
	{
		Ar << Data.LinearDamping;
		Ar << Data.AngularDamping;
	}
	return Ar;
}

//======================================================================================================================
FArchive& operator <<(FArchive& Ar, FRigPhysicsMaterial& Data)
{
	Ar << Data.Friction;
	Ar << Data.Restitution;
	Ar << Data.FrictionCombineMode;
	Ar << Data.RestitutionCombineMode;
	return Ar;
}