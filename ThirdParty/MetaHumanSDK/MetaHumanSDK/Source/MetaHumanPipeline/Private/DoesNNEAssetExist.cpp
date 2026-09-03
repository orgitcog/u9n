// Copyright Epic Games, Inc. All Rights Reserved.

#include "DoesNNEAssetExist.h"
#include "CoreMinimal.h"
#include "NNEModelData.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"

bool DoesNNEAssetExist(const FString& InAssetPath)
{
	// Get the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	// Create a filter for the asset you want to check
	FARFilter AssetFilter;
	AssetFilter.PackagePaths.Add("/Game");
	AssetFilter.bRecursivePaths = true;
	AssetFilter.ClassPaths.Add(UNNEModelData::StaticClass()->GetClassPathName());
	AssetFilter.PackageNames.Add(FName(*InAssetPath));
	// Query the asset registry
	TArray<FAssetData> AssetData;
	AssetRegistryModule.Get().GetAssets(AssetFilter, AssetData);
	return AssetData.Num() > 0;
}
