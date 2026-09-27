// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sequencer/MediaTrackEditor.h"

/**
 * MediaTrackEditor that can be added to MetaHumanSequences
 * This can be used to customize the behavior of the sequencer track editor
 * Right now this relies on the functionality available in FMediaTrackEditor
 */
class METAHUMANSEQUENCER_API FMetaHumanMediaTrackEditor
	: public FMediaTrackEditor
{
public:

	/**
	 * Create a new track editor instance. This is called by ISequencerModule::RegisterPropertyTrackEditor when
	 * registering this editor
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InOwningSequencer)
	{
		return MakeShared<FMetaHumanMediaTrackEditor>(InOwningSequencer);
	}

	FMetaHumanMediaTrackEditor(TSharedRef<ISequencer> InSequencer);

	//~ FMovieSceneTrackEditor interface
	virtual bool SupportsSequence(class UMovieSceneSequence* InSequence) const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool IsResizable(UMovieSceneTrack* InTrack) const override;
	virtual void Resize(float NewSize, UMovieSceneTrack* InTrack) override;
};
