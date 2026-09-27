// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "MetaSoundViewModelConversionFunctions.generated.h"

class UMetaSoundInputViewModel;
class UMetaSoundOutputViewModel;

/**
 * Collection of conversion functions to use with MetaSound Viewmodels.
 */
UCLASS(MinimalAPI)
class UMetaSoundViewModelConversionFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Searches the given array of MetaSoundInputViewModels for the specified input. Returns nullptr if unable to find a matching viewmodel.
	UFUNCTION(BlueprintCallable, BlueprintPure, DisplayName = "Find Input Viewmodel by Name", Category = "TechAudioTools")
	static UMetaSoundInputViewModel* FindInputViewModelByName(const TArray<UMetaSoundInputViewModel*>& MetaSoundInputViewModels, const FName InputName);

	// Searches the given array of MetaSoundOutputViewModels for the specified output. Returns nullptr if unable to find a matching viewmodel.
	UFUNCTION(BlueprintCallable, BlueprintPure, DisplayName = "Find Output Viewmodel by Name", Category = "TechAudioTools")
	static UMetaSoundOutputViewModel* FindOutputViewModelByName(const TArray<UMetaSoundOutputViewModel*>& MetaSoundOutputViewModels, const FName OutputName);
};
