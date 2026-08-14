// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceAudioSection.h"
#include "MetaHumanPerformance.h"
#include "Sequencer/MetaHumanPerformanceMovieSceneAudioSection.h"
#include "ISequencer.h"
#include "MetaHumanPerformanceMediaSection.h"

FMetaHumanPerformanceAudioSection::FMetaHumanPerformanceAudioSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
	: FAudioSection{ InSection, InSequencer }
	, SequencerPtr(InSequencer)
	, Section(&InSection)
{
}

bool FMetaHumanPerformanceAudioSection::SectionIsResizable() const
{
	return false;
}

bool FMetaHumanPerformanceAudioSection::IsReadOnly() const
{
	return true;
}

int32 FMetaHumanPerformanceAudioSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	// Paint the section as is
	int32 LayerId = FAudioSection::OnPaintSection(InPainter);
	
	const UMetaHumanPerformanceMovieSceneAudioSection* MHSection = CastChecked<UMetaHumanPerformanceMovieSceneAudioSection>(Section);
	const UMetaHumanPerformance* Performance = MHSection->PerformanceShot;

	TSharedPtr<ISequencer> ISequencer = SequencerPtr.Pin();
	if (ISequencer.IsValid() && Performance)
	{
		LayerId = MetaHumanSectionPainterHelper::PaintExcludedFrames(InPainter, LayerId, ISequencer.Get(), Section);

		LayerId = MetaHumanPerformanceSectionPainterHelper::PaintAnimationResults(InPainter, LayerId, ISequencer.Get(), Section, Performance, true);
	}

	return LayerId;
}
