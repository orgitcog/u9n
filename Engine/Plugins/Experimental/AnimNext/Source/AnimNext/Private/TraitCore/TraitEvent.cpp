// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitCore/TraitEvent.h"

bool FAnimNextTraitEvent::DecrementLifetime(UE::AnimNext::FTraitEventList& OutputEventList)
{
	const bool bExpired = Lifetime.Decrement();
	if (bExpired)
	{
		OnExpired(OutputEventList);
	}

	return bExpired;
}
