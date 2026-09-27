// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubtitlesAndClosedCaptionsModule.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"

DEFINE_LOG_CATEGORY(LogSubtitlesAndClosedCaptions);

void FSubtitlesAndClosedCaptionsModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(ISubtitlesAndClosedCaptionsModule::GetModularFeatureName(), this);
}

void FSubtitlesAndClosedCaptionsModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(ISubtitlesAndClosedCaptionsModule::GetModularFeatureName(), this);
}

TSubclassOf<UAssetUserData> FSubtitlesAndClosedCaptionsModule::GetAssetUserDataClass() const
{
	return USubtitleAssetUserData::StaticClass();
}

IMPLEMENT_MODULE(FSubtitlesAndClosedCaptionsModule, SubtitlesAndClosedCaptions);
