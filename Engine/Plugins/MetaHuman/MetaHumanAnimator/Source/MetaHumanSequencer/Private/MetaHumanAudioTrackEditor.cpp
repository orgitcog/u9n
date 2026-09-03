// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioTrackEditor.h"

#include "MetaHumanSequence.h"
#include "MetaHumanAudioSection.h"
#include "Tracks/MovieSceneAudioTrack.h"

FMetaHumanAudioTrackEditor::FMetaHumanAudioTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FAudioTrackEditor{ InSequencer }
{
}

bool FMetaHumanAudioTrackEditor::SupportsSequence(class UMovieSceneSequence* InSequence) const
{
	return InSequence && InSequence->IsA(UMetaHumanSceneSequence::StaticClass());
}

bool FMetaHumanAudioTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass.Get() && TrackClass.Get()->IsChildOf(UMovieSceneAudioTrack::StaticClass());
}

void FMetaHumanAudioTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
}

TSharedPtr<SWidget> FMetaHumanAudioTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	return TSharedPtr<SWidget>();
}

void FMetaHumanAudioTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
}

TSharedRef<ISequencerSection> FMetaHumanAudioTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShared<FMetaHumanAudioSection>(SectionObject, GetSequencer());
}