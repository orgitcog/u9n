// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanViewportSettings.h"

#include "Misc/FrameTime.h"

#include "MetaHumanPerformanceViewportSettings.generated.h"

/////////////////////////////////////////////////////
// UMetaHumanPerformanceViewportSettings

USTRUCT()
struct FMetaHumanPerformanceViewportState
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 bShowControlRig : 1;
};

UCLASS()
class UMetaHumanPerformanceViewportSettings
	: public UMetaHumanViewportSettings
{
	GENERATED_BODY()

public:

	UMetaHumanPerformanceViewportSettings();

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	bool IsControlRigVisible(EABImageViewMode InView) const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	void ToggleControlRigVisibility(EABImageViewMode InView);

public:

	UPROPERTY()
	FFrameTime CurrentFrameTime;

private:

	UPROPERTY()
	TMap<EABImageViewMode, FMetaHumanPerformanceViewportState> PerformanceViewportState;
};