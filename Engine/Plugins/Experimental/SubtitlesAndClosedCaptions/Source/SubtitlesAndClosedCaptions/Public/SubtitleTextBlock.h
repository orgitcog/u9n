// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Blueprint/UserWidget.h"
#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"

#include "SubtitleTextBlock.generated.h"

class UTextBlock;

UCLASS(Abstract, BlueprintType, Blueprintable, meta=( DontUseGenericSpawnObject="True", DisableNativeTick), MinimalAPI)
class USubtitleTextBlock : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Subtitles", meta = (BindWidget))
	TObjectPtr<UTextBlock> DialogSubtitleBlock;

	UPROPERTY(EditDefaultsOnly, Category = "Subtitles", meta = (BindWidget))
	TObjectPtr<UTextBlock> CaptionSubtitleBlock;

	UPROPERTY(EditDefaultsOnly, Category = "Subtitles", meta = (BindWidget))
	TObjectPtr<UTextBlock> DescriptionSubtitleBlock;

public:
	void StartDisplayingSubtitle(const USubtitleAssetUserData& Subtitle);
	void StopDisplayingSubtitle(const ESubtitleType SubtitleType);
};