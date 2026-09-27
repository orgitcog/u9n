// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

struct METAHUMANCHARACTER_API FMetaHumanCharacterCustomVersion
{
	enum Type : int32
	{
		// Before any version changes
		CharcterBase = 0,
		// High res body textures serialized to character
		BodyTexturesSerialized,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static const FGuid GUID;

private:

	FMetaHumanCharacterCustomVersion() = default;
};