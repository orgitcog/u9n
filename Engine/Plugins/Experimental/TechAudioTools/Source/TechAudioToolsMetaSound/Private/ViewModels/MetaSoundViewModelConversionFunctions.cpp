// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/MetaSoundViewModelConversionFunctions.h"

#include "ViewModels/MetaSoundViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaSoundViewModelConversionFunctions)

UMetaSoundInputViewModel* UMetaSoundViewModelConversionFunctions::FindInputViewModelByName(const TArray<UMetaSoundInputViewModel*>& MetaSoundInputViewModels, const FName InputName)
{
	for (UMetaSoundInputViewModel* InputViewModel : MetaSoundInputViewModels)
	{
		if (InputViewModel->GetInputName() == InputName)
		{
			return InputViewModel;
		}
	}

	return nullptr;
}

UMetaSoundOutputViewModel* UMetaSoundViewModelConversionFunctions::FindOutputViewModelByName(const TArray<UMetaSoundOutputViewModel*>& MetaSoundOutputViewModels, const FName OutputName)
{
	for (UMetaSoundOutputViewModel* OutputViewModel : MetaSoundOutputViewModels)
	{
		if (OutputViewModel->GetOutputName() == OutputName)
		{
			return OutputViewModel;
		}
	}

	return nullptr;
}
