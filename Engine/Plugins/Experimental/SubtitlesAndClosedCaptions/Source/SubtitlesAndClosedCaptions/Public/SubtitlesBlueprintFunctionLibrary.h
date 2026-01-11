// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"
#include "SubtitlesBlueprintFunctionLibrary.generated.h"

class USubtitleAssetUserData;

UCLASS(meta = (ScriptName = "SubtitlesLibrary"))
class SUBTITLESANDCLOSEDCAPTIONS_API USubtitlesBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = Subtitles)
	static void QueueSubtitle(const USubtitleAssetUserData* Subtitle, const ESubtitleTiming Timing = ESubtitleTiming::InternallyTimed);

	UFUNCTION(BlueprintCallable, Category = Subtitles)
	static bool IsSubtitleActive(const USubtitleAssetUserData* Subtitle);

	UFUNCTION(BlueprintCallable, Category = Subtitles)
	static void StopSubtitle(const USubtitleAssetUserData* Subtitle);

	UFUNCTION(BlueprintCallable, Category = Subtitles)
	static void StopAllSubtitles();
};