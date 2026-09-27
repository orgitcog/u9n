// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"

#include "AssetDefinition_MetaHumanIdentity.generated.h"

UCLASS()
class UAssetDefinition_MetaHumanIdentity
	: public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	//~Begin UAssetDefinitionDefault interface
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual UThumbnailInfo* LoadThumbnailInfo(const FAssetData& InAssetData) const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& InOpenArgs) const;
	//~End UAssetDefinitionDefault interface

private:

	void ResolveContourDataCompatibility(const class UMetaHumanIdentityFace* InFacePart) const;
	bool ContourDataIsCompatible(const class UMetaHumanIdentityFace* InFacePart, bool& bOutUpdateRequired) const;
	FText GetCompatibilityMessage(bool bRigCompatible, bool bContoursCompatible) const;
};