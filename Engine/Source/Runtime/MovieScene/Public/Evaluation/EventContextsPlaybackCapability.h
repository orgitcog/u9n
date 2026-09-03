// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Evaluation/MovieScenePlaybackCapabilities.h"

class UObject;

namespace UE::MovieScene
{

/** Playback capability for controlling how events are triggered */
struct IEventContextsPlaybackCapability
{
	UE_DECLARE_MOVIESCENE_PLAYBACK_CAPABILITY_API(MOVIESCENE_API, IEventContextsPlaybackCapability)

	/** Virtual destructor */
	virtual ~IEventContextsPlaybackCapability() {}

	/** Get the contexts used for triggering events */
	virtual TArray<UObject*> GetEventContexts() const = 0;
};

}  // namespace UE::MovieScene

