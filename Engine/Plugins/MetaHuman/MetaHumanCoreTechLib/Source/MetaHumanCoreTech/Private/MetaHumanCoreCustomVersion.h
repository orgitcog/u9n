// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

// Custom serialization version for MetaHumanCore asset types
// TODO leaving this with the same name for now but needs revisiting now we have moved the class
struct FMetaHumanCoreCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,

		// Added audio processing type to frame data
		AddAudioProcessingTypeToFrameData = 1,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FMetaHumanCoreCustomVersion() {}
};
