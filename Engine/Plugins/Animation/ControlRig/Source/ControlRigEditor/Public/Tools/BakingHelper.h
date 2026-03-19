// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class ISequencer;

/**
 * FBakingHelper
 */

struct FBakingHelper
{
	UE_DEPRECATED(5.6, "This function was moved to the UE::AnimationEditMode namespace.")
	static TWeakPtr<ISequencer> GetSequencer();
};
