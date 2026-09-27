// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanVideoBaseLiveLinkSubjectSettings.h"

#include "MetaHumanVideoLiveLinkSubjectSettings.generated.h"



UCLASS()
class METAHUMANLOCALLIVELINKSOURCE_API UMetaHumanVideoLiveLinkSubjectSettings : public UMetaHumanVideoBaseLiveLinkSubjectSettings
{
public:

	GENERATED_BODY()

	UPROPERTY()
	FMetaHumanMediaSourceCreateParams MediaSourceCreateParams;
};