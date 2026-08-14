// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/MetaHumanGenerateDepthWindowOptions.h"

#include "CaptureData.h"
#include "CameraCalibration.h"

#include "MetaHumanDepthGenerator.generated.h"

UCLASS(BlueprintType, Blueprintable)
class UMetaHumanDepthGenerator : public UObject
{
	GENERATED_BODY()

public:

	bool Process(UFootageCaptureData* InFootageCaptureData);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman | DepthGenerator")
	bool Process(UFootageCaptureData* InFootageCaptureData, UMetaHumanGenerateDepthWindowOptions* InOptions);
};