// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialReadinessSignatures.h"

struct FSpatialReadinessAPIDelegates final
{
	// Constructor which binds TFunctions
	FSpatialReadinessAPIDelegates(
		FAddUnreadyVolume_Function AddUnreadyVolume,
		FRemoveUnreadyVolume_Function RemoveUnreadyVolume);

	~FSpatialReadinessAPIDelegates() = default;

	FAddUnreadyVolume_Delegate AddUnreadyVolumeDelegate;
	FRemoveUnreadyVolume_Delegate RemoveUnreadyVolumeDelegate;
};
