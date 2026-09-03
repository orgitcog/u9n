// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStatePropertyBindingExtension.h"
#include "GameFramework/Actor.h"
#include "SceneStateBlueprintEditorUtils.h"

namespace UE::SceneState::Editor
{

void FBindingExtension::ExtendWidgetRow(FDetailWidgetRow& InWidgetRow, const IDetailLayoutBuilder& InDetailBuilder, const UClass* InObjectClass, TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	// Disable Actor references from being set from within the editor
	// This aligns with actor ref properties which are disabled in template objects (e.g. a bp variable of actor ref type will be disabled within the blueprint)
	// This does not disable the entire row though, to allow the binding extension to still work.
	if (IsObjectPropertyOfClass(InPropertyHandle->GetProperty(), AActor::StaticClass()))
	{
		InWidgetRow.IsValueEnabled(false);
	}
	FPropertyBindingExtension::ExtendWidgetRow(InWidgetRow, InDetailBuilder, InObjectClass, InPropertyHandle);
}

} // UE::SceneState::Editor
