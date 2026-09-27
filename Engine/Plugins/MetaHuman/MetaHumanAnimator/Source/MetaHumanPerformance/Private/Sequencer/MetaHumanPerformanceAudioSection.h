// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TrackEditors/AudioTrackEditor.h"

class FMetaHumanPerformanceAudioSection
	: public FAudioSection
{
public:
	FMetaHumanPerformanceAudioSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer);

	// ISequencerSectionInterface
	virtual bool SectionIsResizable() const;
	virtual bool IsReadOnly() const;
	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;

private:
	TWeakPtr<ISequencer> SequencerPtr;
	UMovieSceneSection* Section = nullptr;
};
