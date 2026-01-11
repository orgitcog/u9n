// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanLocalLiveLinkSourceSettings.h"
#include "MetaHumanLocalLiveLinkSource.h"



void UMetaHumanLocalLiveLinkSourceSettings::SetSource(FMetaHumanLocalLiveLinkSource* InSource)
{
	Source = InSource;
}

void UMetaHumanLocalLiveLinkSourceSettings::RequestSubjectCreation(const FString& InSubjectName, UMetaHumanLocalLiveLinkSubjectSettings* InMetaHumanLocalLiveLinkSubjectSettings)
{
	Source->RequestSubjectCreation(InSubjectName, InMetaHumanLocalLiveLinkSubjectSettings);
}
