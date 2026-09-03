// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/EventTriggerControlPlaybackCapability.h"

namespace UE::MovieScene
{

UE_DEFINE_MOVIESCENE_PLAYBACK_CAPABILITY(FEventTriggerControlPlaybackCapability)

bool FEventTriggerControlPlaybackCapability::IsDisablingEventTriggers(FFrameTime& OutDisabledUntilTime) const
{
	if (DisableEventTriggersUntilTime.IsSet())
	{
		OutDisabledUntilTime = DisableEventTriggersUntilTime.GetValue();
		return true;
	}
	return false;
}

}  // namespace UE::MovieScene

