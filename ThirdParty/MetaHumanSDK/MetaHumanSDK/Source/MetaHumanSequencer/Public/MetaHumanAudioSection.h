// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TrackEditors/AudioTrackEditor.h"

class FMetaHumanAudioSection
	: public FAudioSection
{
public:
	FMetaHumanAudioSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer);

	// ISequencerSectionInterface
	virtual bool SectionIsResizable() const;
	virtual bool IsReadOnly() const;
};
