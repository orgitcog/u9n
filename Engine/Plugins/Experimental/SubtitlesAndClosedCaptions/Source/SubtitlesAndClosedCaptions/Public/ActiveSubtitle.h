// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

class USubtitleAssetUserData;

#include "Engine/TimerHandle.h"
#include "UObject/ObjectPtr.h"

#include "ActiveSubtitle.generated.h"

USTRUCT()
struct FActiveSubtitle
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const USubtitleAssetUserData> Subtitle = nullptr;

	FTimerHandle DurationTimerHandle;
};
