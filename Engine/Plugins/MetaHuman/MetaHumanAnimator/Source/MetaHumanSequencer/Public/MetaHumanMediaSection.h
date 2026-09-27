// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanMovieSceneChannel.h"
#include "Sequencer/MediaThumbnailSection.h"
#include "SequencerSectionPainter.h"

class ISequencer;
class UMovieSceneSection;


/**
 * Extends FMediaThumbnailSection to allow painting on top of the sequencer section
 */
class METAHUMANSEQUENCER_API FMetaHumanMediaSection
	: public FMediaThumbnailSection
{
public:
	FMetaHumanMediaSection(UMovieSceneMediaSection& InSection, TSharedPtr<class FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<class ISequencer> InSequencer);

	//~ ISequencerSection interface
	virtual bool IsReadOnly() const override;
	virtual bool SectionIsResizable() const override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;
	virtual float GetSectionHeight(const UE::Sequencer::FViewDensityInfo& InViewDensity) const override;
	virtual FText GetSectionTitle() const override;

private:

	FMetaHumanMovieSceneChannel* KeyContainer = nullptr;
};

/**
 * Helper to paint excluded frames using FSequencerSectionPainter
 */
namespace MetaHumanSectionPainterHelper
{
	METAHUMANSEQUENCER_API int32 PaintExcludedFrames(FSequencerSectionPainter& InPainter, int32 InLayerId, ISequencer* InSequencer, UMovieSceneSection* InSection);
};
