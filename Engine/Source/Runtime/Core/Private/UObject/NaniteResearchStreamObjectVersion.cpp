// Copyright Epic Games, Inc. All Rights Reserved.
#include "UObject/NaniteResearchStreamObjectVersion.h"
#include "UObject/DevObjectVersion.h"
#include "Containers/Map.h"
#include "Misc/Guid.h"


TMap<FGuid, FGuid> FNaniteResearchStreamObjectVersion::GetSystemGuids()
{
	TMap<FGuid, FGuid> SystemGuids;
	const FDevSystemGuids& DevGuids = FDevSystemGuids::Get();

	SystemGuids.Add(DevGuids.NANITE_DERIVEDDATA_VER, FGuid("D22CB0E4-8349-458A-ADA4-A6765ECCB187"));
	SystemGuids.Add(DevGuids.STATICMESH_DERIVEDDATA_VER, FGuid("656D74A3-C705-42F0-8F21-9205DCCD53EB"));

	return SystemGuids;
}
