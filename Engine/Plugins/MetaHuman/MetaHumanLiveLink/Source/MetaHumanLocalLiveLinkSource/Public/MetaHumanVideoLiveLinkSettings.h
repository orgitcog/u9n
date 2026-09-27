// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "Nodes/HyprsenseRealtimeNode.h"

#include "MetaHumanVideoLiveLinkSettings.generated.h"



UCLASS(config = Editor, defaultconfig)
class METAHUMANLOCALLIVELINKSOURCE_API UMetaHumanVideoLiveLinkSettings : public UObject
{
public:

	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category = "Subject")
	bool bHeadOrientation = true;

	UPROPERTY(config, EditAnywhere, Category = "Subject")
	bool bHeadTranslation = true;

	UPROPERTY(config, EditAnywhere, Category = "Subject")
	EHyprsenseRealtimeNodeDebugImage MonitorImage = EHyprsenseRealtimeNodeDebugImage::None;
};