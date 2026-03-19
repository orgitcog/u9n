// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Guid.h"

#define UE_API HARMONIXDSP_API

struct FFusionPatchCustomVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,

		KeyzonesUseMappedParameters = 1,

		PitchShifterNameRedirects = 2,

		PanImportingFromDTAFixed = 3,

		DeprecatedPresets = 4,

		DeprecatedUnusedEffectsSettings = 5,

		DeprecateTypedSettingsArray = 6,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static UE_API const FGuid GUID;

private:

	FFusionPatchCustomVersion() {}
};

#undef UE_API
