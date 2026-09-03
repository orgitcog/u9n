// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextureGraph.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_TextureGraphInstance.generated.h"

UCLASS()
class UAssetDefinition_TextureGraphInstance : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureGraphInstance", "Texture Graph Instance"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor::Emerald); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UTextureGraphInstance::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { EAssetCategoryPaths::Texture };
		return Categories;
	}

	virtual FAssetOpenSupport GetAssetOpenSupport(const FAssetOpenSupportArgs& OpenSupportArgs) const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	// UAssetDefinition End
};
