// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubtitleTextBlock.h"
#include "SubtitlesAndClosedCaptionsModule.h"
#include "Components/TextBlock.h"

void USubtitleTextBlock::NativeConstruct()
{
	Super::NativeConstruct();

	// Ensure the text blocks don't start visible.
	if (IsValid(DialogSubtitleBlock))
	{
		DialogSubtitleBlock->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (IsValid(CaptionSubtitleBlock))
	{
		CaptionSubtitleBlock->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (IsValid(DescriptionSubtitleBlock))
	{
		DescriptionSubtitleBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USubtitleTextBlock::StartDisplayingSubtitle(const USubtitleAssetUserData& Subtitle)
{
	// Pick which UextBlock is relevant by category....
	UTextBlock* TextToModify = nullptr;

	switch (Subtitle.SubtitleType)
	{
	default:
	{
		UE_LOG(LogSubtitlesAndClosedCaptions, Warning, TEXT("An unrecognized subtitle type was queued for display. Using the standard Subtitle text block as a fallback."));
		// Fallthrough, no break.
	}

	case ESubtitleType::Subtitle:
	{
		TextToModify = DialogSubtitleBlock;
		break;
	}

	case ESubtitleType::ClosedCaption:
	{
		TextToModify = CaptionSubtitleBlock;
		break;
	}

	case ESubtitleType::AudioDescription:
	{
		TextToModify = DescriptionSubtitleBlock;
		break;
	}

	}

	// ...Then modify and display it
	if (IsValid(TextToModify))
	{
		TextToModify->SetText(Subtitle.Text);
		TextToModify->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void USubtitleTextBlock::StopDisplayingSubtitle(const ESubtitleType SubtitleType)
{

	UTextBlock* TextToModify = nullptr;
	switch (SubtitleType)
	{
	default: // Fallthrough, no break (see ::StartDisplayingSubtitle)
	case ESubtitleType::Subtitle:
	{
		TextToModify = DialogSubtitleBlock;
		break;
	}

	case ESubtitleType::ClosedCaption:
	{
		TextToModify = CaptionSubtitleBlock;
		break;
	}

	case ESubtitleType::AudioDescription:
	{
		TextToModify = DescriptionSubtitleBlock;
		break;
	}

	}


	if (IsValid(TextToModify))
	{
		TextToModify->SetVisibility(ESlateVisibility::Collapsed);
	}
}
