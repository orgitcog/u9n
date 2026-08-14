// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_MetaHumanCaptureSource.h"
#include "MetaHumanCaptureSource.h"
#include "MetaHumanCoreEditorModule.h"

FText UAssetDefinition_MetaHumanCaptureSource::GetAssetDisplayName() const
{
	return NSLOCTEXT("MetaHuman", "MetaHumanCaptureSourceAssetName", "Capture Source");
}

FLinearColor UAssetDefinition_MetaHumanCaptureSource::GetAssetColor() const
{
	return FColor::Yellow;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaHumanCaptureSource::GetAssetClass() const
{
	return UMetaHumanCaptureSource::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaHumanCaptureSource::GetAssetCategories() const
{
	return FModuleManager::GetModuleChecked<IMetaHumanCoreEditorModule>(TEXT("MetaHumanCoreEditor")).GetMetaHumanAssetCategoryPath();
}