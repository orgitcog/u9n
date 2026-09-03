// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioLiveLinkSource.h"
#include "MetaHumanAudioLiveLinkSourceSettings.h"
#include "MetaHumanAudioLiveLinkSubject.h"
#include "MetaHumanAudioLiveLinkSubjectSettings.h"

#define LOCTEXT_NAMESPACE "MetaHumanAudioLiveLinkSource"



FText FMetaHumanAudioLiveLinkSource::GetSourceType() const
{
	return LOCTEXT("MetaHumanAudio", "MetaHuman (Audio)");
}

TSubclassOf<ULiveLinkSourceSettings> FMetaHumanAudioLiveLinkSource::GetSettingsClass() const
{
	return UMetaHumanAudioLiveLinkSourceSettings::StaticClass();
}

TSharedPtr<FMetaHumanLocalLiveLinkSubject> FMetaHumanAudioLiveLinkSource::CreateSubject(const FName& InSubjectName, UMetaHumanLocalLiveLinkSubjectSettings* InSettings)
{
	return MakeShared<FMetaHumanAudioLiveLinkSubject>(LiveLinkClient, SourceGuid, InSubjectName, Cast<UMetaHumanAudioLiveLinkSubjectSettings>(InSettings));
}

#undef LOCTEXT_NAMESPACE
