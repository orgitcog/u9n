// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanMediaSourceCreateParams.h"

#include "LiveLinkSourceSettings.h"

#include "MetaHumanLocalLiveLinkSourceSettings.generated.h"



UCLASS()
class METAHUMANLOCALLIVELINKSOURCE_API UMetaHumanLocalLiveLinkSourceSettings : public ULiveLinkSourceSettings
{
public:

	GENERATED_BODY()

	void SetSource(class FMetaHumanLocalLiveLinkSource* InSource);
	void RequestSubjectCreation(const FString& InSubjectName, class UMetaHumanLocalLiveLinkSubjectSettings* InMetaHumanLocalLiveLinkSubjectSettings);

	UPROPERTY()
	bool bIsPreset = false;

private:

	class FMetaHumanLocalLiveLinkSource* Source = nullptr;
};