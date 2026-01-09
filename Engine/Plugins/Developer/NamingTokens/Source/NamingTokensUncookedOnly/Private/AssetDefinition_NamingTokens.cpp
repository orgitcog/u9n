// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_NamingTokens.h"

#include "NamingTokens.h"
#include "NamingTokensStyle.h"

TSoftClassPtr<UObject> UAssetDefinition_NamingTokens::GetAssetClass() const
{
	return UNamingTokens::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_NamingTokens::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(EAssetCategoryPaths::Misc) };
	return Categories;
}

FText UAssetDefinition_NamingTokens::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetDefinition_NamingTokens", "AssetDefinition_NamingTokens_DisplayName", "Naming Tokens");
}

const FSlateBrush* UAssetDefinition_NamingTokens::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return FNamingTokensStyle::Get().GetBrush("ClassThumbnail.NamingTokens");
}
