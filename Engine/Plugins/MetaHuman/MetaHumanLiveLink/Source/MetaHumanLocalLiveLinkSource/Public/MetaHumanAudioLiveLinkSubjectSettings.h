// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanAudioBaseLiveLinkSubjectSettings.h"

#include "MetaHumanAudioLiveLinkSubjectSettings.generated.h"



UCLASS()
class METAHUMANLOCALLIVELINKSOURCE_API UMetaHumanAudioLiveLinkSubjectSettings : public UMetaHumanAudioBaseLiveLinkSubjectSettings
{
public:

	GENERATED_BODY()

	UPROPERTY()
	FMetaHumanMediaSourceCreateParams MediaSourceCreateParams;
};
