// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"

#include "AssetDefinition_MetaHumanWardrobeItem.generated.h"

UCLASS()
class UAssetDefinition_MetaHumanWardrobeItem : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	//~Begin UAssetDefinitionDefault interface
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual UThumbnailInfo* LoadThumbnailInfo(const FAssetData& InAssetData) const override;
	//~End UAssetDefinitionDefault interface
};
