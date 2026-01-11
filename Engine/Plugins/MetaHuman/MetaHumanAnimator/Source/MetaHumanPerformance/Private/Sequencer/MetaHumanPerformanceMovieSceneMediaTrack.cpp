// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceMovieSceneMediaTrack.h"
#include "MetaHumanPerformanceMovieSceneMediaSection.h"
#include "MovieScene.h"

UMetaHumanPerformanceMovieSceneMediaTrack::UMetaHumanPerformanceMovieSceneMediaTrack(const FObjectInitializer& InObjectInitializer)
	: Super{ InObjectInitializer }
{
}

UMovieSceneSection* UMetaHumanPerformanceMovieSceneMediaTrack::AddNewMediaSourceOnRow(UMediaSource& MediaSource, FFrameNumber Time, int32 RowIndex)
{
	const float DefaultMediaSectionDuration = 1.0f;
	FFrameRate TickResolution = GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameTime DurationToUse = DefaultMediaSectionDuration * TickResolution;

	// add the section
	UMetaHumanPerformanceMovieSceneMediaSection* NewSection = NewObject<UMetaHumanPerformanceMovieSceneMediaSection>(this, NAME_None, RF_Transactional);
	check(NewSection);

	NewSection->InitialPlacementOnRow(GetAllSections(), Time, DurationToUse.FrameNumber.Value, RowIndex);
	NewSection->SetMediaSource(&MediaSource);

	AddSection(*NewSection);

	NewSection->AddChannelToMovieSceneSection();

	return NewSection;
}

bool UMetaHumanPerformanceMovieSceneMediaTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMetaHumanPerformanceMovieSceneMediaSection::StaticClass();
}
