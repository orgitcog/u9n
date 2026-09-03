// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CommonUILibrary.generated.h"

#define UE_API COMMONUI_API

class UWidget;
template <typename T> class TSubclassOf;

UCLASS(MinimalAPI)
class UCommonUILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Finds the first parent widget of the given type and returns it, or null if no parent could be found.
	 */
	UFUNCTION(BlueprintCallable, Category="Common UI", meta=(DeterminesOutputType=Type))
	static UE_API UWidget* FindParentWidgetOfType(UWidget* StartingWidget, TSubclassOf<UWidget> Type);
};

#undef UE_API
