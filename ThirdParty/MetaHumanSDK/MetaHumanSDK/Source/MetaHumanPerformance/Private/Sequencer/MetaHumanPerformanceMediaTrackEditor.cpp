// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceMediaTrackEditor.h"
#include "MetaHumanPerformanceMovieSceneMediaTrack.h"
#include "MetaHumanPerformanceMovieSceneMediaSection.h"
#include "MetaHumanPerformanceMediaSection.h"

#include "MetaHumanSequence.h"

FMetaHumanPerformanceMediaTrackEditor::FMetaHumanPerformanceMediaTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMetaHumanMediaTrackEditor{ InSequencer }
{
}

bool FMetaHumanPerformanceMediaTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass.Get() && TrackClass.Get()->IsChildOf(UMetaHumanPerformanceMovieSceneMediaTrack::StaticClass());
}

TSharedRef<ISequencerSection> FMetaHumanPerformanceMediaTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& /*Track*/, FGuid /*ObjectBinding*/)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	UMetaHumanPerformanceMovieSceneMediaSection* MovieSceneMediaSection = CastChecked<UMetaHumanPerformanceMovieSceneMediaSection>(&SectionObject);
	return MakeShared<FMetaHumanPerformanceMediaSection>(*MovieSceneMediaSection, GetThumbnailPool(), GetSequencer());
}