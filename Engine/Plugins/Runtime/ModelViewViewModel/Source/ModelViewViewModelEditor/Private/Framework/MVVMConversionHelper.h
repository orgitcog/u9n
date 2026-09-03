// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

class UMVVMBlueprintViewConversionFunction;
class UWidgetBlueprint;

namespace UE::MVVM
{

struct FConversionHelper
{
	static FString GetBindToDestinationStringFromConversionFunction(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewConversionFunction* ConversionFunction);
};

} // namespace UE::MVVM
