// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tracks/MovieSceneAudioTrack.h"

#include "MetaHumanAudioTrack.generated.h"

/**
 * Implements a UMovieSceneAudioTrack customized for the MetaHuman Plugin
 */
UCLASS(MinimalAPI)
class UMetaHumanAudioTrack :
	public UMovieSceneAudioTrack
{
	GENERATED_BODY()

public:

	UMetaHumanAudioTrack(const FObjectInitializer& InObjectInitializer);
};