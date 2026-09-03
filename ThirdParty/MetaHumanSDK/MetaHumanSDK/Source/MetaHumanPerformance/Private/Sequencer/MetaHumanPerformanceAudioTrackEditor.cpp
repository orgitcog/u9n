// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceAudioTrackEditor.h"

#include "MetaHumanSequence.h"
#include "MetaHumanPerformanceAudioSection.h"
#include "Tracks/MovieSceneAudioTrack.h"

FMetaHumanPerformanceAudioTrackEditor::FMetaHumanPerformanceAudioTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FAudioTrackEditor{ InSequencer }
{
}

bool FMetaHumanPerformanceAudioTrackEditor::SupportsSequence(class UMovieSceneSequence* InSequence) const
{
	return InSequence && InSequence->IsA(UMetaHumanSceneSequence::StaticClass());
}

bool FMetaHumanPerformanceAudioTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass.Get() && TrackClass.Get()->IsChildOf(UMovieSceneAudioTrack::StaticClass());
}

void FMetaHumanPerformanceAudioTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
}

TSharedPtr<SWidget> FMetaHumanPerformanceAudioTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	return TSharedPtr<SWidget>();
}

void FMetaHumanPerformanceAudioTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
}

TSharedRef<ISequencerSection> FMetaHumanPerformanceAudioTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShareable(new FMetaHumanPerformanceAudioSection(SectionObject, GetSequencer()));
}


