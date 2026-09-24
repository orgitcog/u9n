// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigPhysics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigPhysics)

////////////////////////////////////////////////////////////////////////////////
// FRigPhysicsSolverDescription
////////////////////////////////////////////////////////////////////////////////

void FRigPhysicsSolverDescription::Serialize(FArchive& Ar)
{
	Ar << ID;
	Ar << Name;
}
