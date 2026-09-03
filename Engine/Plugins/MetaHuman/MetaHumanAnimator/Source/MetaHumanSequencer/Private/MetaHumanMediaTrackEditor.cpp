// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanMediaTrackEditor.h"
#include "MetaHumanSequence.h"
#include "MetaHumanMovieSceneMediaTrack.h"
#include "MetaHumanMovieSceneMediaSection.h"
#include "MetaHumanMediaSection.h"

FMetaHumanMediaTrackEditor::FMetaHumanMediaTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMediaTrackEditor{ InSequencer }
{
}

bool FMetaHumanMediaTrackEditor::SupportsSequence(class UMovieSceneSequence* InSequence) const
{
	return InSequence && InSequence->IsA(UMetaHumanSceneSequence::StaticClass());
}

bool FMetaHumanMediaTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass.Get() && TrackClass.Get()->IsChildOf(UMetaHumanMovieSceneMediaTrack::StaticClass());
}

void FMetaHumanMediaTrackEditor::BuildAddTrackMenu(FMenuBuilder&)
{
	// Doing nothing here prevents the user from adding a MediaTrack manually
}

TSharedPtr<SWidget> FMetaHumanMediaTrackEditor::BuildOutlinerEditWidget(const FGuid&, UMovieSceneTrack*, const FBuildEditWidgetParams&)
{
	// Doing nothing here prevents the user from adding new media through the Sequencer interface
	return TSharedPtr<SWidget>();
}

TSharedRef<ISequencerSection> FMetaHumanMediaTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& /*Track*/, FGuid /*ObjectBinding*/)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	
	UMetaHumanMovieSceneMediaSection* MovieSceneMediaSection = CastChecked<UMetaHumanMovieSceneMediaSection>(&SectionObject);
	return MakeShareable(new FMetaHumanMediaSection{ *MovieSceneMediaSection, GetThumbnailPool(), GetSequencer() });
}

bool FMetaHumanMediaTrackEditor::IsResizable(UMovieSceneTrack* InTrack) const
{
	return true;
}

void FMetaHumanMediaTrackEditor::Resize(float NewSize, UMovieSceneTrack* InTrack)
{
	if (UMetaHumanMovieSceneMediaTrack* MediaTrack = Cast<UMetaHumanMovieSceneMediaTrack>(InTrack))
	{
		MediaTrack->Modify();

		int32 MaxNumRows = 1;
		for (UMovieSceneSection* Section : MediaTrack->GetAllSections())
		{
			MaxNumRows = FMath::Max(MaxNumRows, Section->GetRowIndex() + 1);
		}

		MediaTrack->SetRowHeight(FMath::RoundToInt(NewSize) / MaxNumRows);
	}
}