// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// UnrealEd
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_MetaHumanCharacter.generated.h"

UCLASS()
class UAssetDefinition_MetaHumanCharacter : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	//~Begin UAssetDefinitionDefault interface
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual UThumbnailInfo* LoadThumbnailInfo(const FAssetData& InAssetData) const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& InOpenArgs) const override;
	//~End UAssetDefinitionDefault interface
};