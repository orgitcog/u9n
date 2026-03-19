// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSubtitlesTrack.h"

#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneSubtitleSection.h"
#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSubtitlesTrack)

#define LOCTEXT_NAMESPACE "MovieSceneSubtitlesTrack"


UMovieSceneSubtitlesTrack::UMovieSceneSubtitlesTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UMovieSceneSection* UMovieSceneSubtitlesTrack::AddNewSubtitle(const USubtitleAssetUserData& Subtitle, FFrameNumber InStartTime)
{
	return AddNewSubtitleOnRow(Subtitle, InStartTime, INDEX_NONE);
}

UMovieSceneSection* UMovieSceneSubtitlesTrack::AddNewSubtitleOnRow(const USubtitleAssetUserData& Subtitle, FFrameNumber InStartTime, int32 InRowIndex)
{
	UMovieSceneSubtitleSection* NewSection = Cast<UMovieSceneSubtitleSection>(CreateNewSection());

	const FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();
	const FFrameTime DurationToUse = Subtitle.Duration * FrameRate;

	ensure(DurationToUse.FrameNumber.Value > 0);
	NewSection->InitialPlacementOnRow(SubtitleSections, InStartTime, DurationToUse.FrameNumber.Value, InRowIndex);
	NewSection->SetSubtitle(Subtitle);

	SubtitleSections.Add(NewSection);

	return NewSection;
}

bool UMovieSceneSubtitlesTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneSubtitleSection::StaticClass();
}

void UMovieSceneSubtitlesTrack::RemoveAllAnimationData()
{
	return SubtitleSections.Empty();
}

bool UMovieSceneSubtitlesTrack::HasSection(const UMovieSceneSection& Section) const
{
	return SubtitleSections.Contains(&Section);
}

void UMovieSceneSubtitlesTrack::AddSection(UMovieSceneSection& Section)
{
	SubtitleSections.Add(&Section);
}

void UMovieSceneSubtitlesTrack::RemoveSection(UMovieSceneSection& Section)
{
	SubtitleSections.Remove(&Section);
}

void UMovieSceneSubtitlesTrack::RemoveSectionAt(int32 SectionIndex)
{
	SubtitleSections.RemoveAt(SectionIndex);
}

bool UMovieSceneSubtitlesTrack::IsEmpty() const
{
	return SubtitleSections.IsEmpty();
}

const TArray<UMovieSceneSection*>& UMovieSceneSubtitlesTrack::GetAllSections() const
{
	return SubtitleSections;
}

bool UMovieSceneSubtitlesTrack::SupportsMultipleRows() const
{
	return true;
}

UMovieSceneSection* UMovieSceneSubtitlesTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSubtitleSection>(this, NAME_None, RF_Transactional);
}

#undef LOCTEXT_NAMESPACE
