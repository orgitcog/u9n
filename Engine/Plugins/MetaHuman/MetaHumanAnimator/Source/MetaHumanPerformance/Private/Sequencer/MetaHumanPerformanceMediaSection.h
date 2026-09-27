// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanMediaSection.h"

class UMetaHumanPerformance;

/**
 * Extends FMediaThumbnailSection to allow painting on top of the sequencer section
 */
class FMetaHumanPerformanceMediaSection
	: public FMetaHumanMediaSection
{
public:
	FMetaHumanPerformanceMediaSection(UMovieSceneMediaSection& InSection, TSharedPtr<class FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<class ISequencer> InSequencer);

	//~ ISequencerSection interface
	virtual bool IsReadOnly() const override;
	virtual bool SectionIsResizable() const override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;
};


namespace MetaHumanPerformanceSectionPainterHelper
{
	int32 PaintAnimationResults(FSequencerSectionPainter& InPainter, int32 InLayerId, const ISequencer* InSequencer, const UMovieSceneSection* InSection, const UMetaHumanPerformance* InPerformance, bool bInPaintAudioSection);
}