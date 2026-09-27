// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanMediaTrackEditor.h"

/**
 * MediaTrackEditor that can be added to MetaHumanPerformanceSequences
 * This can be used to customize the behavior of the sequencer track editor
 * Right now this relies on the functionality available in FMediaTrackEditor
 */
class FMetaHumanPerformanceMediaTrackEditor
	: public FMetaHumanMediaTrackEditor
{
public:

	/**
	 * Create a new track editor instance. This is called by ISequencerModule::RegisterPropertyTrackEditor when
	 * registering this editor
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InOwningSequencer)
	{
		return MakeShared<FMetaHumanPerformanceMediaTrackEditor>(InOwningSequencer);
	}

	FMetaHumanPerformanceMediaTrackEditor(TSharedRef<ISequencer> InSequencer);

	//~ FMovieSceneTrackEditor interface
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
};
