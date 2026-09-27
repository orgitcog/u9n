// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanLiveLinkSubjectSettings.h"

#include "LiveLinkFaceSubjectSettings.generated.h"

UCLASS()
class ULiveLinkFaceSubjectSettings : public UMetaHumanLiveLinkSubjectSettings
{
public:

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Controls", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	bool bHeadOrientation = true;

	UPROPERTY(EditAnywhere, Category = "Controls", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	bool bHeadTranslation = true;
	
};
