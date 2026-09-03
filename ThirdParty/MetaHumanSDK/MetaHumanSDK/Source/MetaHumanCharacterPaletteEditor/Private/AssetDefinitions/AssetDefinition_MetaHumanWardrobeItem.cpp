// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinitions/AssetDefinition_MetaHumanWardrobeItem.h"

#include "MetaHumanWardrobeItem.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterPaletteEditor"

FText UAssetDefinition_MetaHumanWardrobeItem::GetAssetDisplayName() const
{
	return LOCTEXT("WardrobeItemDisplayName", "MetaHuman Wardrobe Item");
}

FLinearColor UAssetDefinition_MetaHumanWardrobeItem::GetAssetColor() const
{
	return FColor::Orange;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaHumanWardrobeItem::GetAssetClass() const
{
	return UMetaHumanWardrobeItem::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaHumanWardrobeItem::GetAssetCategories() const
{
	static const FAssetCategoryPath Path(LOCTEXT("MetaHumanAssetCategoryPath", "MetaHuman"), LOCTEXT("MetaHumanAdvancedAssetCategoryLabel", "Advanced"));
	static const FAssetCategoryPath Categories[] = { Path };

	return Categories;
}

UThumbnailInfo* UAssetDefinition_MetaHumanWardrobeItem::LoadThumbnailInfo(const FAssetData& InAssetData) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAssetData.GetAsset(), USceneThumbnailInfo::StaticClass());
}

#undef LOCTEXT_NAMESPACE
