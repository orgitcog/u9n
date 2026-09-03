// Copyright Epic Games, Inc. All Rights Reserved.
#include "Engine/ExternalObjectAndActorDependencyGatherer.h"

#if WITH_EDITOR

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "ExternalPackageHelper.h"

void FExternalObjectAndActorDependencyGatherer::GatherDependencies(const FAssetData& AssetData,
	const FAssetRegistryState& AssetRegistryState,TFunctionRef<FARCompiledFilter(const FARFilter&)> CompileFilterFunc,
	TArray<IAssetDependencyGatherer::FGathereredDependency>& OutDependencies,
	TArray<FString>& OutDependencyDirectories) const
{
	FARFilter Filter = GetQueryFilter(AssetData.PackageName, &OutDependencyDirectories);

	TArray<FAssetData> FilteredAssets;
	AssetRegistryState.GetAssets(CompileFilterFunc(Filter), {}, FilteredAssets, true);

	for (const FAssetData& FilteredAsset : FilteredAssets)
	{
		OutDependencies.Emplace(IAssetDependencyGatherer::FGathereredDependency{ FilteredAsset.PackageName, UE::AssetRegistry::EDependencyProperty::Game | UE::AssetRegistry::EDependencyProperty::Build });
	}
}

FARFilter FExternalObjectAndActorDependencyGatherer::GetQueryFilter(FName PackageName, TArray<FString>* OutQueryDirectories)
{
	const FString ExternalActorsPath = ULevel::GetExternalActorsPath(PackageName.ToString());
	const FString ExternalObjectPath = FExternalPackageHelper::GetExternalObjectsPath(PackageName.ToString());
	if (OutQueryDirectories)
	{
		OutQueryDirectories->Add(ExternalActorsPath);
		OutQueryDirectories->Add(ExternalObjectPath);
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bIncludeOnlyOnDiskAssets = true;
	Filter.PackagePaths.Add(FName(ExternalActorsPath));
	Filter.PackagePaths.Add(FName(ExternalObjectPath));
	return Filter;
}

REGISTER_ASSETDEPENDENCY_GATHERER(FExternalObjectAndActorDependencyGatherer, UWorld);

#endif

