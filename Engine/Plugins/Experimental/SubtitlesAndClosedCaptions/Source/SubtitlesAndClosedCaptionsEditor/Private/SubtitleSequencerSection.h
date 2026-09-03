// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerSection.h"
#include "SubtitleSequencerSection.h"

class FSequencerSectionPainter;

class FSubtitleSequencerSection
	: public FSequencerSection
	, public TSharedFromThis<FSubtitleSequencerSection>
{
public:
	FSubtitleSequencerSection(UMovieSceneSection& InSection)
		: FSequencerSection(InSection)
	{}

	// ISequencerSection interface
	virtual FText GetSectionTitle() const override;
	virtual bool SectionIsResizable() const override { return false; }
	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
};
