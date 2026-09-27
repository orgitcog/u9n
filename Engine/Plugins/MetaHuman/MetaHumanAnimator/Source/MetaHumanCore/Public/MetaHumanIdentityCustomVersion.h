// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

struct METAHUMANCORE_API FMetaHumanIdentityCustomVersion
{
	enum Type : int32
	{
		// MetaHuman Identity was updated to use EditorBulkData
		EditorBulkDataUpdate = 0,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static const FGuid GUID;

private:

	FMetaHumanIdentityCustomVersion() = default;
};
