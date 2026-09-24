// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothEditorOptions.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothEditorOptions)

UChaosClothEditorOptions::UChaosClothEditorOptions(class FObjectInitializer const& ObjectInitializer)
{
	bClothAssetsOpenInDataflowEditor = false;
	ConstructionViewportMousePanButton = EConstructionViewportMousePanButton::Right;
}

FName UChaosClothEditorOptions::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR

FText UChaosClothEditorOptions::GetSectionText() const
{
	return NSLOCTEXT("ChaosClothEditorPlugin", "ChaosClothEditorSettingsSection", "Chaos Cloth Editor");
}

#endif
