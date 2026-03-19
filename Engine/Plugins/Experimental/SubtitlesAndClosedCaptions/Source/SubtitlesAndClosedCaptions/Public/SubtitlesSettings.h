// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Math/Color.h"
#include "SubtitleTextBlock.h"
#include "SubtitlesSettings.generated.h"


UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Subtitles And Closed Captions"))
class USubtitlesSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USubtitlesSettings();

	const TSubclassOf<USubtitleTextBlock>& GetWidget() const { return SubtitleWidgetToUse; }
	const TSubclassOf<USubtitleTextBlock>& GetWidgetDefault() const { return SubtitleWidgetToUseDefault; }

protected:

	UPROPERTY(config, EditDefaultsOnly, Category = Subtitles, AdvancedDisplay)
	TSubclassOf<USubtitleTextBlock> SubtitleWidgetToUse;

	UPROPERTY(config)
	TSubclassOf<USubtitleTextBlock> SubtitleWidgetToUseDefault; // fallback for SubtitleWidgetToUse (not set by user)
};
