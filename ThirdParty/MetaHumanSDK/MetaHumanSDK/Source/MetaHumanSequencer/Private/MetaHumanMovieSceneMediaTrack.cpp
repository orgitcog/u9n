// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanMovieSceneMediaTrack.h"
#include "MetaHumanMovieSceneMediaSection.h"
#include "MovieScene.h"

UMetaHumanMovieSceneMediaTrack::UMetaHumanMovieSceneMediaTrack(const FObjectInitializer& InObjectInitializer)
	: Super{ InObjectInitializer }
{
#if WITH_EDITORONLY_DATA
	RowHeight = MinRowHeight;
#endif

	// This disables the "Add Section" entry in the track's context menu
	SupportedBlendTypes = FMovieSceneBlendTypeField::None();
}

UMovieSceneSection* UMetaHumanMovieSceneMediaTrack::AddNewMediaSourceOnRow(UMediaSource& MediaSource, FFrameNumber Time, int32 RowIndex)
{
	const float DefaultMediaSectionDuration = 1.0f;
	FFrameRate TickResolution = GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameTime DurationToUse = DefaultMediaSectionDuration * TickResolution;

	// add the section
	UMetaHumanMovieSceneMediaSection* NewSection = NewObject<UMetaHumanMovieSceneMediaSection>(this, NAME_None, RF_Transactional);
	check(NewSection);

	NewSection->InitialPlacementOnRow(GetAllSections(), Time, DurationToUse.FrameNumber.Value, RowIndex);
	NewSection->SetMediaSource(&MediaSource);

	AddSection(*NewSection);

	NewSection->AddChannelToMovieSceneSection();
	NewSection->ClearFlags(RF_Transactional);

	return NewSection;
}

bool UMetaHumanMovieSceneMediaTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMetaHumanMovieSceneMediaTrack::StaticClass();
}

void UMetaHumanMovieSceneMediaTrack::RemoveAllAnimationData()
{
	const TArray<UMovieSceneSection*> Sections = GetAllSections();
	for (UMovieSceneSection* Section : Sections)
	{
		RemoveSection(*Section);
	}
}

#if WITH_EDITORONLY_DATA
float UMetaHumanMovieSceneMediaTrack::GetRowHeight() const
{
	return RowHeight;
}

void UMetaHumanMovieSceneMediaTrack::SetRowHeight(int32 NewRowHeight)
{
	RowHeight = FMath::Max(MinRowHeight, NewRowHeight);
}
#endif
