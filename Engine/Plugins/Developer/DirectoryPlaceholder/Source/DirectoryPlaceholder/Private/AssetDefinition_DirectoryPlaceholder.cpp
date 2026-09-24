// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_DirectoryPlaceholder.h"
#include "DirectoryPlaceholder.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText UAssetDefinition_DirectoryPlaceholder::GetAssetDisplayName() const
{
	return LOCTEXT("AssetTypeActions_DirectoryPlaceholder", "Directory Placeholder");
}

FLinearColor UAssetDefinition_DirectoryPlaceholder::GetAssetColor() const
{
	return FLinearColor::Gray;
}

TSoftClassPtr<> UAssetDefinition_DirectoryPlaceholder::GetAssetClass() const
{
	return UDirectoryPlaceholder::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_DirectoryPlaceholder::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::Misc };
	return Categories;
}

EAssetCommandResult UAssetDefinition_DirectoryPlaceholder::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	return EAssetCommandResult::Unhandled;
}

#undef LOCTEXT_NAMESPACE
