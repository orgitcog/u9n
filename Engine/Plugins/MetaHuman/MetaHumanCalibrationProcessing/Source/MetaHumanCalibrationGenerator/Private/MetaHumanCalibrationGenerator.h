// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"

#include "MetaHumanStereoCalibrator.h"

#include "Widgets/MetaHumanCalibrationGeneratorOptions.h"

#include "CaptureData.h"

#include "MetaHumanCalibrationGenerator.generated.h"

UCLASS(BlueprintType, Blueprintable)
class UMetaHumanCalibrationGenerator : public UObject
{
	GENERATED_BODY()

public:

	UMetaHumanCalibrationGenerator();

	UFUNCTION(BlueprintCallable, Category = "MetaHuman | Calibration Generator")
	bool Process(UFootageCaptureData* InCaptureData, const UMetaHumanCalibrationGeneratorOptions* InOptions);

	bool Process(UFootageCaptureData* InCaptureData);

private:
	using FDetectedFrames = TArray<TMap<FString, TArray<FVector2D>>>;
	FDetectedFrames DetectPatterns(const UFootageCaptureData* InCaptureData,
								   const UMetaHumanCalibrationGeneratorOptions* InOptions);

	TUniquePtr<UE::Wrappers::FMetaHumanStereoCalibrator> StereoCalibrator;
};
