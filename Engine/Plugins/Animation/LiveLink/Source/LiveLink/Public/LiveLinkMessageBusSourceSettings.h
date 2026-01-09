// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkSourceSettings.h"

#include "LiveLinkMessageBusSourceSettings.generated.h"

#define UE_API LIVELINK_API




/**
 * Settings for LiveLinkMessageBusSource.
 * Used to apply default Evaluation mode from project settings when constructed
 */
UCLASS(MinimalAPI)
class ULiveLinkMessageBusSourceSettings : public ULiveLinkSourceSettings
{
public:
	GENERATED_BODY()

public:
	UE_API ULiveLinkMessageBusSourceSettings();
};

#undef UE_API
