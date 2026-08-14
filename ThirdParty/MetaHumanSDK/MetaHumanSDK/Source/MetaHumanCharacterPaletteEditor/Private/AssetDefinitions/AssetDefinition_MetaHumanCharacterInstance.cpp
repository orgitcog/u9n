// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinitions/AssetDefinition_MetaHumanCharacterInstance.h"

#include "PaletteEditor/MetaHumanCharacterPaletteAssetEditor.h"
#include "MetaHumanCharacterInstance.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterPalette"

FText UAssetDefinition_MetaHumanCharacterInstance::GetAssetDisplayName() const
{
	return LOCTEXT("MetaHumanCharacterInstanceDisplayName", "MetaHuman Character Instance");
}

FLinearColor UAssetDefinition_MetaHumanCharacterInstance::GetAssetColor() const
{
	return FColor::Orange;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaHumanCharacterInstance::GetAssetClass() const
{
	return UMetaHumanCharacterInstance::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaHumanCharacterInstance::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath{ LOCTEXT("MetaHumanAssetCategoryPath", "MetaHuman") } };
	return Categories;
}

EAssetCommandResult UAssetDefinition_MetaHumanCharacterInstance::OpenAssets(const FAssetOpenArgs& InOpenArgs) const
{
	if (UMetaHumanCharacterInstance* Instance = InOpenArgs.LoadFirstValid<UMetaHumanCharacterInstance>())
	{
		if (Instance->GetMetaHumanCollection() != nullptr)
		{
			UMetaHumanCharacterPaletteAssetEditor* PaletteEditor = NewObject<UMetaHumanCharacterPaletteAssetEditor>(GetTransientPackage(), NAME_None, RF_Transient);
			PaletteEditor->SetObjectToEdit(Instance);
			PaletteEditor->Initialize();

			return EAssetCommandResult::Handled;
		}
	}

	return EAssetCommandResult::Unhandled;
}

#undef LOCTEXT_NAMESPACE
