// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "PCapViewmodel.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, Abstract, DisplayName="PCap Viewmodel",meta = (ShowWorldContextPin))
class PERFORMANCECAPTUREWORKFLOW_API UPCapViewmodel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:

	/** Initialise the viewmodel. Implemented in Blueprint. */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category="Performance Capture|Viewmodel")
	void Initialize();
};
