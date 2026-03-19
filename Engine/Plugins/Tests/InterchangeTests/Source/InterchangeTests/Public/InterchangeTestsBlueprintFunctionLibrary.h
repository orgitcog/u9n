// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "InterchangePipelineBase.h"
#include "InterchangeTestsBlueprintFunctionLibrary.generated.h"

UCLASS(MinimalAPI)
class UInterchangeTestsBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="Tests")
	static FString GetPipelinePropertiesAsJSON(UInterchangePipelineBase* Pipeline);
};