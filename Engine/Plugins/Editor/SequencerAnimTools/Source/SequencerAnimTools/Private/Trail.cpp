// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trail.h"

//MZ TODO LOOK AT REIMPLEMENTING CONSTANT TRAILS
namespace UE
{
namespace SequencerAnimTools
{
bool FTrail::HandleObjectsChanged(const TMap<UObject*, UObject*>& ReplacementMap)
{
	if (UObject*const* NewObject = ReplacementMap.Find(Owner.Get()))
	{
		Owner = *NewObject;
		return true;
	}
	return false;
}
}
}