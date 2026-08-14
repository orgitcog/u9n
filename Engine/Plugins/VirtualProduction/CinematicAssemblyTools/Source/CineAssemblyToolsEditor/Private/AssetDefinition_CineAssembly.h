// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTools/AssetDefinition_LevelSequence.h"

#include "AssetDefinition_CineAssembly.generated.h"

/** Asset definition for a UCineAssembly asset */
UCLASS()
class UAssetDefinition_CineAssembly : public UAssetDefinition_LevelSequence
{
	GENERATED_BODY()

protected:
	// Begin UAssetDefinition Interface
	virtual FText GetAssetDisplayName() const override;
	virtual FText GetAssetDisplayName(const FAssetData& AssetData) const override;
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual const FSlateBrush* GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const override;
	virtual TArray<FAssetData> PrepareToActivateAssets(const FAssetActivateArgs& ActivateArgs) const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	// End UAssetDefinition Interface
};
