// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_MetaHumanFaceFittingSolver.h"
#include "MetaHumanFaceFittingSolver.h"
#include "MetaHumanCoreEditorModule.h"

FText UAssetDefinition_MetaHumanFaceFittingSolver::GetAssetDisplayName() const
{
	return NSLOCTEXT("MetaHuman", "MetaHumanFaceFittingSolverAssetName", "Face Fitting Solver");
}

FLinearColor UAssetDefinition_MetaHumanFaceFittingSolver::GetAssetColor() const
{
	return FColor::Orange;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaHumanFaceFittingSolver::GetAssetClass() const
{
	return UMetaHumanFaceFittingSolver::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaHumanFaceFittingSolver::GetAssetCategories() const
{
	return FModuleManager::GetModuleChecked<IMetaHumanCoreEditorModule>(TEXT("MetaHumanCoreEditor")).GetMetaHumanAdvancedAssetCategoryPath();
}
