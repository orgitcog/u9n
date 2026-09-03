// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

struct FWidgetReference;

class FMenuBuilder;
class FWidgetBlueprintEditor;
class UWidgetBlueprint;

namespace UE::MVVM
{
struct FMVVMBindingEditorHelper
{
public:
	static bool CreateWidgetBindings(UWidgetBlueprint* Blueprint, TSet<FWidgetReference> Widgets, TArray<FGuid>& OutBindingIds);

};
} // namespace UE::MVVM