// Copyright Epic Games, Inc. All Rights Reserved.


#include "Tools/BakingHelper.h"

#include "Sequencer/EditModeAnimationUtil.h"

TWeakPtr<ISequencer> FBakingHelper::GetSequencer()
{
	return UE::AnimationEditMode::GetSequencer();
}
