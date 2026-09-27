// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubtitlesBlueprintFunctionLibrary.h"

#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"

void USubtitlesBlueprintFunctionLibrary::QueueSubtitle(const USubtitleAssetUserData* Subtitle, const ESubtitleTiming Timing)
{
	if (!Subtitle)
	{
		return;
	}

	FQueueSubtitleParameters Params{ *CastChecked<const UAssetUserData>(Subtitle) };
	FSubtitlesAndClosedCaptionsDelegates::QueueSubtitle.ExecuteIfBound(Params, Timing);
}

bool USubtitlesBlueprintFunctionLibrary::IsSubtitleActive(const USubtitleAssetUserData* Subtitle)
{
	if (!Subtitle)
	{
		return false;
	}
	if (!FSubtitlesAndClosedCaptionsDelegates::IsSubtitleActive.IsBound())
	{
		return false;
	}
	return FSubtitlesAndClosedCaptionsDelegates::IsSubtitleActive.Execute(*CastChecked<const UAssetUserData>(Subtitle));
}

void USubtitlesBlueprintFunctionLibrary::StopSubtitle(const USubtitleAssetUserData* Subtitle)
{
	if (!Subtitle)
	{
		return;
	}
	FSubtitlesAndClosedCaptionsDelegates::StopSubtitle.ExecuteIfBound(*CastChecked<const UAssetUserData>(Subtitle));
}

void USubtitlesBlueprintFunctionLibrary::StopAllSubtitles()
{
	FSubtitlesAndClosedCaptionsDelegates::StopAllSubtitles.ExecuteIfBound();
}
