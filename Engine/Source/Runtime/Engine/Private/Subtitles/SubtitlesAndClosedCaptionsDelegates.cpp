// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"

TDelegate<void(const FQueueSubtitleParameters&, const ESubtitleTiming)> FSubtitlesAndClosedCaptionsDelegates::QueueSubtitle;
TDelegate<bool(const UAssetUserData&)> FSubtitlesAndClosedCaptionsDelegates::IsSubtitleActive;
TDelegate<void(const UAssetUserData&)> FSubtitlesAndClosedCaptionsDelegates::StopSubtitle;
TDelegate<void()> FSubtitlesAndClosedCaptionsDelegates::StopAllSubtitles;