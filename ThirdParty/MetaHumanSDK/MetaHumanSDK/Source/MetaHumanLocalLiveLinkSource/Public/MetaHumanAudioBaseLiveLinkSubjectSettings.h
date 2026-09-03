// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanLocalLiveLinkSubjectSettings.h"

#include "MetaHumanAudioBaseLiveLinkSubjectSettings.generated.h"



UCLASS()
class METAHUMANLOCALLIVELINKSOURCE_API UMetaHumanAudioBaseLiveLinkSubjectSettings : public UMetaHumanLocalLiveLinkSubjectSettings
{
public:

	GENERATED_BODY()

	virtual void Setup() override;

	/* A very simplistic volume indicator to show if audio is being received - it is not a true audio level monitoring tool. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Audio", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	float Level = 0;
};
