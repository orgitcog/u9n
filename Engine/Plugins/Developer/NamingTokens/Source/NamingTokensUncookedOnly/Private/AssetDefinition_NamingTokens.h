// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Script/AssetDefinition_Blueprint.h"

#include "AssetDefinition_NamingTokens.generated.h"

UCLASS()
class UAssetDefinition_NamingTokens : public UAssetDefinition_Blueprint
{
	GENERATED_BODY()
	
public:
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual FText GetAssetDisplayName() const override;
	virtual const FSlateBrush* GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const override;
};
