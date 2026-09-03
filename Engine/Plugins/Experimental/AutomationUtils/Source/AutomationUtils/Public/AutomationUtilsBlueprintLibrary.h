// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"


#include "AutomationUtilsBlueprintLibrary.generated.h"

#define UE_API AUTOMATIONUTILS_API

UCLASS(MinimalAPI)
class UAutomationUtilsBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = "Automation")
	static UE_API void TakeGameplayAutomationScreenshot(const FString ScreenshotName, float MaxGlobalError = .02, float MaxLocalError = .12, FString MapNameOverride = TEXT(""));
};

#undef UE_API
