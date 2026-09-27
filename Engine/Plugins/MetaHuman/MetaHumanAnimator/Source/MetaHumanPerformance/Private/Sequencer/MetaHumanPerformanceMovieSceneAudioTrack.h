// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tracks/MovieSceneAudioTrack.h"

#include "MetaHumanPerformanceMovieSceneAudioTrack.generated.h"

/**
 * Implements a UMovieSceneAudioTrack customized for the MetaHumanPerformance plugin
 */
UCLASS(MinimalAPI)
class UMetaHumanPerformanceMovieSceneAudioTrack :
	 public UMovieSceneAudioTrack
 {
	GENERATED_BODY()

public:

	UMetaHumanPerformanceMovieSceneAudioTrack(const FObjectInitializer& InObjectInitializer);

	//~ UMovieSceneTrack interface
	virtual UMovieSceneSection* CreateNewSection() override;
};