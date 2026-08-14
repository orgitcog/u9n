// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataWrappers/ChaosVDParticleDataWrapper.h"

#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/FortniteSeasonBranchObjectVersion.h"

// @note: Tracing an scene with 1000 particles moving, Manually serializing the structs is ~20% faster
// than normal UStruct serialization in unversioned mode. As we do this at runtime when tracing in development builds, this is important.
// One of the downside is that this will be more involved to maintain as any versioning needs to be done by hand.

bool FChaosVDFRigidParticleControlFlags::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFortniteSeasonBranchObjectVersion::GUID);
	if (Ar.CustomVer(FFortniteSeasonBranchObjectVersion::GUID) >= FFortniteSeasonBranchObjectVersion::CVDSerializationFixMissingSerializationProperties)
	{
		Ar << bHasValidData;

		if (!bHasValidData)
		{
			return !Ar.IsError();
		}
	}
	
	Ar << bGravityEnabled;
	Ar << bCCDEnabled;
	Ar << bOneWayInteractionEnabled;
	Ar << bInertiaConditioningEnabled;
	Ar << GravityGroupIndex;
	Ar << bMACDEnabled;

	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::SolverIterationsDataSupportInChaosVisualDebugger)
	{
		Ar << PositionSolverIterationCount;
		Ar << VelocitySolverIterationCount;
		Ar << ProjectionSolverIterationCount;
	}

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::GyroscopicTorquesSupportInChaosVisualDebugger)
	{
		Ar << bGyroscopicTorqueEnabled;
	}

	return !Ar.IsError();
}

bool FChaosVDParticlePositionRotation::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;
	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << MX;
	Ar << MR;

	return !Ar.IsError();
}

bool FChaosVDParticleVelocities::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;
	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << MV;
	Ar << MW;

	return !Ar.IsError();
}

bool FChaosVDParticleBounds::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;

	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << MMin;
	Ar << MMax;

	return !Ar.IsError();
}

bool FChaosVDParticleDynamics::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;
	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << MAcceleration;
	Ar << MAngularAcceleration;
	Ar << MAngularImpulseVelocity;
	Ar << MLinearImpulseVelocity;

	return !Ar.IsError();
}

bool FChaosVDParticleMassProps::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;
	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << MCenterOfMass;
	Ar << MRotationOfMass;
	Ar << MI;
	Ar << MInvI;
	Ar << MM;
	Ar << MInvM;

	return !Ar.IsError();
}

bool FChaosVDParticleDynamicMisc::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;
	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << MAngularEtherDrag;
	Ar << MMaxLinearSpeedSq;
	Ar << MMaxAngularSpeedSq;
	Ar << MInitialOverlapDepenetrationVelocity;
	Ar << MCollisionGroup;
	Ar << MObjectState;
	Ar << MSleepType;
	Ar << bDisabled;

	MControlFlags.Serialize(Ar);

	return true;
}

bool FChaosVDParticleCluster::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;
	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << ParentParticleID;	
	Ar << NumChildren;
	Ar << ChildToParent;
	Ar << ClusterGroupIndex;
	Ar << bInternalCluster;
	Ar << CollisionImpulse;
	Ar << ExternalStrains;
	Ar << InternalStrains;
	Ar << Strain;
	Ar << ConnectivityEdges;
	Ar << bIsAnchored;
	Ar << bUnbreakable;
	Ar << bIsChildToParentLocked;

	return !Ar.IsError();
}

bool FChaosVDKinematicTarget::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;
	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << Rotation;
	Ar << Position;
	Ar << Mode;

	return !Ar.IsError();
}

bool FChaosVDVSmooth::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;
	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << MV;
	Ar << MW;

	return !Ar.IsError();
}

bool FChaosVDParticleDataWrapper::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	Ar << bHasValidData;
	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << Type;
	Ar << GeometryHash;

	bHasDebugName = DebugNamePtr != nullptr;
	Ar << bHasDebugName;

	if (bHasDebugName)
	{
		if (Ar.IsLoading())
		{
			Ar << DebugName;
		}
		else
		{
			// Keep the static analyser happy, we already checked this above
			checkSlow(DebugNamePtr != nullptr);

			FString& DebugNameRef = *DebugNamePtr;
			Ar << DebugNameRef;
		}
	}

	Ar << ParticleIndex;
	Ar << SolverID;

	Ar << ParticlePositionRotation;
	Ar << ParticleVelocities;

	// Bounds data was not exported prior to this version
	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::ParticleInflatedBoundsInChaosVisualDebugger)
	{
		Ar << ParticleInflatedBounds;
	}

	Ar << ParticleDynamics;
	Ar << ParticleDynamicsMisc;
	Ar << ParticleMassProps;

	Ar << CollisionDataPerShape;

	Ar << ParticleCluster;
	
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::AdditionalGameThreadDataSupportInChaosVisualDebugger)
	{
		Ar << ParticleContext;
		Ar << ParticleKinematicTarget;
		Ar << ParticleVWSmooth;
	}

	return !Ar.IsError();
}
