// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanMovieSceneMediaTrack.h"

#include "MetaHumanPerformanceMovieSceneMediaTrack.generated.h"

/**
 * Implements a MovieSceneMediaTrack customized for the MetaHumanPerformance plugin
 */
UCLASS(MinimalAPI)
class UMetaHumanPerformanceMovieSceneMediaTrack
	: public UMetaHumanMovieSceneMediaTrack
{
	GENERATED_BODY()

public:
	UMetaHumanPerformanceMovieSceneMediaTrack(const FObjectInitializer& InObjectInitializer);

	//~ UMovieSceneMediaTrack interface
	virtual UMovieSceneSection* AddNewMediaSourceOnRow(UMediaSource& MediaSource, FFrameNumber Time, int32 RowIndex) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
};
