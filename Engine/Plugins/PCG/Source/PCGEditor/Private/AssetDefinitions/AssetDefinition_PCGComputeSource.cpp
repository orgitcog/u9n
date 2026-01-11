// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_PCGComputeSource.h"

#include "Compute/PCGComputeSource.h"

#include "PCGEditorCommon.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_PCGComputeSource"

FText UAssetDefinition_PCGComputeSource::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "PCG Compute Source");
}

FLinearColor UAssetDefinition_PCGComputeSource::GetAssetColor() const
{
	return FColor::Orange;
}

TSoftClassPtr<UObject> UAssetDefinition_PCGComputeSource::GetAssetClass() const
{
	return UPCGComputeSource::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_PCGComputeSource::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FPCGEditorCommon::PCGAdvancedAssetCategoryPath };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
