// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinitions/AssetDefinition_MetaHumanCollection.h"

#include "MetaHumanCollection.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterPaletteEditor"

FText UAssetDefinition_MetaHumanCollection::GetAssetDisplayName() const
{
	return LOCTEXT("MetaHumanCollectionDisplayName", "MetaHuman Collection");
}

FLinearColor UAssetDefinition_MetaHumanCollection::GetAssetColor() const
{
	return FColor::Orange;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaHumanCollection::GetAssetClass() const
{
	return UMetaHumanCollection::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaHumanCollection::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath{ LOCTEXT("MetaHumanAssetCategoryPath", "MetaHuman") } };
	return Categories;
}

#undef LOCTEXT_NAMESPACE