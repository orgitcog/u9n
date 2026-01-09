// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoverCVDDataWrappers.h"

bool FMoverCVDSimDataWrapper::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;

	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << SolverID;
	Ar << ParticleID;
	Ar << SyncStateBytes;
	Ar << SyncStateDataCollectionBytes;
	Ar << InputCmdBytes;
	Ar << InputMoverDataCollectionBytes;
	Ar << LocalSimDataBytes;

	return !Ar.IsError();
}
