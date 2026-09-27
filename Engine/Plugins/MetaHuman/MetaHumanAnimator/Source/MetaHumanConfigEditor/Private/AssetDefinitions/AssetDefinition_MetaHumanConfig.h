// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"

#include "AssetDefinition_MetaHumanConfig.generated.h"

UCLASS()
class UAssetDefinition_MetaHumanConfig
	: public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	//~Begin UAssetDefinitionDefault interface
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	//~End UAssetDefinitionDefault interface
};