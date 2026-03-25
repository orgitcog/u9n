// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_MetaHumanFaceContourTracker.h"
#include "MetaHumanFaceContourTrackerAsset.h"
#include "MetaHumanCoreEditorModule.h"
#include "MetaHumanAuthoringObjects.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "MetaHumanAuthoringObjects"

FText UAssetDefinition_MetaHumanFaceContourTracker::GetAssetDisplayName() const
{
	return NSLOCTEXT("MetaHuman", "MetaHumanFaceContourTrackerAssetName", "Face Contour Tracker");
}

FLinearColor UAssetDefinition_MetaHumanFaceContourTracker::GetAssetColor() const
{
	return FColor::Orange;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaHumanFaceContourTracker::GetAssetClass() const
{
	return UMetaHumanFaceContourTrackerAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaHumanFaceContourTracker::GetAssetCategories() const
{
	return FModuleManager::GetModuleChecked<IMetaHumanCoreEditorModule>(TEXT("MetaHumanCoreEditor")).GetMetaHumanAdvancedAssetCategoryPath();
}

EAssetCommandResult UAssetDefinition_MetaHumanFaceContourTracker::OpenAssets(const FAssetOpenArgs& InOpenArgs) const
{
	if (FMetaHumanAuthoringObjects::ArePresent())
	{
		return Super::OpenAssets(InOpenArgs);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("MissingAuthoringObjects", "Can not open editor without MetaHuman authoring objects present"));
		return EAssetCommandResult::Handled;
	}
}

#undef LOCTEXT_NAMESPACE
