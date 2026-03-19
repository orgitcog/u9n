// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubtitleSequencerSection.h"

#include "MovieSceneSection.h"
#include "MovieSceneSubtitleSection.h"
#include "SequencerSectionPainter.h"
#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"

FText FSubtitleSequencerSection::GetSectionTitle() const
{
	if (TStrongObjectPtr<const UMovieSceneSection> SectionPin = WeakSection.Pin())
	{
		const UMovieSceneSubtitleSection& SubtitlesSection = *CastChecked<const UMovieSceneSubtitleSection>(SectionPin.Get());
		if (const USubtitleAssetUserData* Subtitle = SubtitlesSection.GetSubtitle())
		{
			return Subtitle->Text;
		}
	}

	return NSLOCTEXT("FSubtitlesSequencerSection", "NoSubtitleName", "No Subtitle");
}

int32 FSubtitleSequencerSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	int32 LayerId = Painter.PaintSectionBackground();
	return LayerId;
}
