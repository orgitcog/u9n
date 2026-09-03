// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanMediaSection.h"
#include "MetaHumanMovieSceneMediaSection.h"
#include "MetaHumanMovieSceneMediaTrack.h"
#include "SequencerSectionPainter.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MetaHumanMovieSceneChannel.h"
#include "MetaHumanSequence.h"


FMetaHumanMediaSection::FMetaHumanMediaSection(UMovieSceneMediaSection& InSection, TSharedPtr<class FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<class ISequencer> InSequencer)
	: FMediaThumbnailSection{ InSection, InThumbnailPool, InSequencer }
{
	TArrayView<FMetaHumanMovieSceneChannel*> MetaHumanChannels = InSection.GetChannelProxy().GetChannels<FMetaHumanMovieSceneChannel>();
	if (!MetaHumanChannels.IsEmpty())
	{
		KeyContainer = MetaHumanChannels.Last();
	}
}

bool FMetaHumanMediaSection::IsReadOnly() const
{
	return true;
}

bool FMetaHumanMediaSection::SectionIsResizable() const
{
	return false;
}

int32 FMetaHumanMediaSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	int32 LayerId = InPainter.LayerId + 1;
	TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();

	FMediaThumbnailSection::OnPaintSection(InPainter);

	if(KeyContainer && Sequencer.IsValid())
	{
		const FGeometry& Geometry = InPainter.SectionGeometry;
		const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();
		const FVector2f& PaintSize = PaintGeometry.GetLocalSize();
		static const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");

		const FFrameNumber& SectionEndFrame = Section->GetRange().GetUpperBoundValue();
		FFrameRate TickResolution = Sequencer->GetRootTickResolution();
		double FrameLength = TickResolution.AsDecimal() / Sequencer->GetRootDisplayRate().AsDecimal();
		
		for(const FFrameNumber &FrameTime : KeyContainer->GetTimes())
		{
			const float StartFramePosition = FrameTime.Value * PaintSize.X / SectionEndFrame.Value;
			const float EndFramePosition = (FrameTime.Value + FrameLength) * PaintSize.X / SectionEndFrame.Value;
			const float PaintFrameSize = EndFramePosition - StartFramePosition;

			auto BoxGeometry = InPainter.SectionGeometry.ToPaintGeometry({ StartFramePosition, 0.0f }, FSlateLayoutTransform(FVector2f{ PaintFrameSize, PaintSize.Y }));
			FSlateDrawElement::MakeBox(InPainter.DrawElements, LayerId, BoxGeometry, GenericBrush, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		}
	}	

	if (Sequencer.IsValid())
	{
		LayerId = MetaHumanSectionPainterHelper::PaintExcludedFrames(InPainter, LayerId, Sequencer.Get(), Section);
	}

	return LayerId;
}

float FMetaHumanMediaSection::GetSectionHeight(const UE::Sequencer::FViewDensityInfo& InViewDensity) const
{
	const float DefaultHeight = FMediaThumbnailSection::GetSectionHeight(InViewDensity);

	if (UMetaHumanMovieSceneMediaSection* MediaSection = Cast<UMetaHumanMovieSceneMediaSection>(Section))
	{
		if (UMetaHumanMovieSceneMediaTrack* MediaTrack = MediaSection->GetTypedOuter<UMetaHumanMovieSceneMediaTrack>())
		{
			return FMath::Min(DefaultHeight, MediaTrack->GetRowHeight());
		}
	}

	return DefaultHeight;
}

FText FMetaHumanMediaSection::GetSectionTitle() const
{
	return FText();
}


namespace MetaHumanSectionPainterHelper
{

static const TMap<EFrameRangeType, FLinearColor> UEColorExcludedFrames = {
	{ EFrameRangeType::UserExcluded, FLinearColor::FromSRGBColor(FColor::FromHex("#FFFF0080")) },
	{ EFrameRangeType::ProcessingExcluded, FLinearColor::FromSRGBColor(FColor::FromHex("#FFFF0080")) },
	{ EFrameRangeType::CaptureExcluded, FLinearColor::FromSRGBColor(FColor::FromHex("#FFFF0080")) },
	{ EFrameRangeType::RateMatchingExcluded, FLinearColor::FromSRGBColor(FColor::FromHex("#FF990080")) } };


int32 PaintExcludedFrames(FSequencerSectionPainter& InPainter, int32 InLayerId, ISequencer* InSequencer, UMovieSceneSection* InSection)
{
	int32 LayerId = InLayerId;

	if (UMetaHumanSceneSequence* MetaHumanSceneSequence = Cast<UMetaHumanSceneSequence>(InSequencer->GetRootMovieSceneSequence()))
	{
		FFrameRate SourceRate;
		FFrameRangeMap ExcludedFramesMap;
		int32 MediaStartFrame;
		TRange<FFrameNumber> ProcessingLimit;

		if (MetaHumanSceneSequence->GetExcludedFrameInfo.ExecuteIfBound(SourceRate, ExcludedFramesMap, MediaStartFrame, ProcessingLimit))
		{
			const FSlateBrush* SingleFrameBrush = FAppStyle::Get().GetBrush("Sequencer.LayerBar.Background");

			const TRange<FFrameNumber> SectionRange = InSection->GetRange();
			const FFrameNumber& SectionStartFrame = SectionRange.GetLowerBoundValue();
			const FFrameNumber& SectionEndFrame = SectionRange.GetUpperBoundValue();
			const FFrameNumber SectionLength = SectionEndFrame - SectionStartFrame;

			const FFrameTime SectionStartFrameSourceRate = FFrameRate::TransformTime(SectionStartFrame, InSequencer->GetRootTickResolution(), SourceRate);

			const FGeometry& Geometry = InPainter.SectionGeometry;
			const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();
			const FVector2f& PaintSize = PaintGeometry.GetLocalSize();

			for (const TPair<EFrameRangeType, TArray<FFrameRange>>& ExcludedFramesPair : ExcludedFramesMap)
			{
				LayerId++;

				const TArray<FFrameRange>& ExcludedFrames = ExcludedFramesPair.Value;
				const FLinearColor& ExcludedColour = UEColorExcludedFrames[ExcludedFramesPair.Key];

				// Capture-excluded frames are relative to start of the RGB media track
				const int32 FrameOffset = (ExcludedFramesPair.Key == EFrameRangeType::CaptureExcluded ? MediaStartFrame : 0);

				for (int32 Index = 0; Index < ExcludedFrames.Num(); ++Index)
				{
					int32 StartFrame = ExcludedFrames[Index].StartFrame;
					int32 EndFrame = ExcludedFrames[Index].EndFrame;

					if (StartFrame < 0 && EndFrame < 0)
					{
						continue;
					}

					if (StartFrame == -1)
					{
						StartFrame = ProcessingLimit.GetLowerBound().GetValue().Value;
					}
					else
					{
						StartFrame += FrameOffset;
					}

					if (EndFrame == -1)
					{
						EndFrame = ProcessingLimit.GetUpperBound().GetValue().Value - 1;
					}
					else
					{
						EndFrame += FrameOffset;
					}

					const FFrameTime TickStartFrame = FFrameRate::TransformTime(FFrameTime{ StartFrame - SectionStartFrameSourceRate.GetFrame().Value }, SourceRate, InSequencer->GetRootTickResolution());
					const FFrameTime TickEndFrame = FFrameRate::TransformTime(FFrameTime{ EndFrame - SectionStartFrameSourceRate.GetFrame().Value + 1 }, SourceRate, InSequencer->GetRootTickResolution());

					const float StartFramePosition = PaintSize.X * TickStartFrame.FrameNumber.Value / SectionLength.Value;
					const float EndFramePosition = PaintSize.X * TickEndFrame.FrameNumber.Value / SectionLength.Value;

					FSlateDrawElement::MakeBox(
						InPainter.DrawElements,
						LayerId,
						InPainter.SectionGeometry.ToPaintGeometry(
							FVector2f{ EndFramePosition - StartFramePosition, PaintSize.Y }, FSlateLayoutTransform(FVector2f{ StartFramePosition, 0 })),
						SingleFrameBrush,
						ESlateDrawEffect::InvertAlpha,
						ExcludedColour
					);
				}
			}
		}
	}

	return LayerId;
}

} // namespace MetaHumanSectionPainterHelper
