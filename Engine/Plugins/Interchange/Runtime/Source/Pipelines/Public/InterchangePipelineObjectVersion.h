// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Guid.h"

struct FFortniteMainInterchangePipelineObjectVersion
{
	// Not instantiable.
	FFortniteMainInterchangePipelineObjectVersion() = delete;

	enum Type
	{
		InitialVersion,

		// Interchange Move generic animation pipeline property bAddCurveMetadataToSkeleton to the generic shared skeletalmesh and animation pipeline
		InterchangeAddCurveMetadataToSkeletonPropertyMove,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	INTERCHANGEPIPELINES_API const static FGuid GUID;
};
