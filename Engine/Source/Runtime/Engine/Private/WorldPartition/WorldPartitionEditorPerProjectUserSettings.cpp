// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionEditorPerProjectUserSettings.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Algo/Transform.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionEditorPerProjectUserSettings)

#if WITH_EDITOR

void UWorldPartitionEditorPerProjectUserSettings::SetWorldDataLayersNonDefaultEditorLoadStates(UWorld* InWorld, const TArray<const UDataLayerInstance*>& InDataLayersLoadedInEditor, const TArray<const UDataLayerInstance*>& InDataLayersNotLoadedInEditor)
{
	if (ShouldSaveSettings(InWorld))
	{
		FWorldPartitionPerWorldSettings& PerWorldSettings = PerWorldEditorSettings.FindOrAdd(TSoftObjectPtr<UWorld>(InWorld));
		PerWorldSettings.LoadedDataLayers.Reset(InDataLayersLoadedInEditor.Num());
		PerWorldSettings.NotLoadedDataLayers.Reset(InDataLayersNotLoadedInEditor.Num());
		auto IsValidDataLayerInstance = [](const UDataLayerInstance* DataLayerInstance) { return DataLayerInstance && DataLayerInstance->GetAsset(); };
		auto GetDataLayerInstanceAsset = [](const UDataLayerInstance* DataLayerInstance) { return DataLayerInstance->GetAsset(); };
		Algo::TransformIf(InDataLayersLoadedInEditor, PerWorldSettings.LoadedDataLayers, IsValidDataLayerInstance, GetDataLayerInstanceAsset);
		Algo::TransformIf(InDataLayersNotLoadedInEditor, PerWorldSettings.NotLoadedDataLayers, IsValidDataLayerInstance, GetDataLayerInstanceAsset);
		SaveConfig();
	}
}

void UWorldPartitionEditorPerProjectUserSettings::SetEditorLoadedRegions(UWorld* InWorld, const TArray<FBox>& InEditorLoadedRegions)
{
	if (ShouldSaveSettings(InWorld))
	{
		FWorldPartitionPerWorldSettings& PerWorldSettings = PerWorldEditorSettings.FindOrAdd(TSoftObjectPtr<UWorld>(InWorld));
		PerWorldSettings.LoadedEditorRegions.Empty();
		Algo::TransformIf(InEditorLoadedRegions, PerWorldSettings.LoadedEditorRegions, [](const FBox& InBox) { return InBox.IsValid; }, [](const FBox& InBox) { return InBox; });		
		SaveConfig();
	}
}

TArray<FBox> UWorldPartitionEditorPerProjectUserSettings::GetEditorLoadedRegions(UWorld* InWorld) const
{
	if (const FWorldPartitionPerWorldSettings* PerWorldSettings = GetWorldPartitionPerWorldSettings(InWorld))
	{
		return PerWorldSettings->LoadedEditorRegions;
	}

	return TArray<FBox>();
}

void UWorldPartitionEditorPerProjectUserSettings::SetEditorLoadedLocationVolumes(UWorld* InWorld, const TArray<FName>& InEditorLoadedLocationVolumes)
{
	if (ShouldSaveSettings(InWorld))
	{
		FWorldPartitionPerWorldSettings& PerWorldSettings = PerWorldEditorSettings.FindOrAdd(TSoftObjectPtr<UWorld>(InWorld));
		PerWorldSettings.LoadedEditorLocationVolumes = InEditorLoadedLocationVolumes;
		
		SaveConfig();
	}
}

TArray<FName> UWorldPartitionEditorPerProjectUserSettings::GetEditorLoadedLocationVolumes(UWorld* InWorld) const
{
	if (const FWorldPartitionPerWorldSettings* PerWorldSettings = GetWorldPartitionPerWorldSettings(InWorld))
	{
		return PerWorldSettings->LoadedEditorLocationVolumes;
	}

	return TArray<FName>();
}

TArray<const UDataLayerInstance*> UWorldPartitionEditorPerProjectUserSettings::GetWorldDataLayersNotLoadedInEditor(UWorld* InWorld) const
{
	TArray<const UDataLayerInstance*> NotLoadedDataLayers;

	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(InWorld))
	{
		if (const FWorldPartitionPerWorldSettings* PerWorldSettings = GetWorldPartitionPerWorldSettings(InWorld))
		{
			for (const TSoftObjectPtr<const UDataLayerAsset>& DataLayerAsset : PerWorldSettings->NotLoadedDataLayers)
			{
				if (DataLayerAsset != nullptr)
				{
					const UDataLayerInstance* DataLayerInstance = DataLayerManager->GetDataLayerInstance(DataLayerAsset.Get());
					if (DataLayerInstance != nullptr)
					{
						NotLoadedDataLayers.Emplace(DataLayerInstance);
					}
				}
			}
		}
	}

	return NotLoadedDataLayers;
}

TArray<UDataLayerInstance*> UWorldPartitionEditorPerProjectUserSettings::GetWorldDataLayersNotLoadedInEditor(UWorld* InWorld)
{
	TArray<const UDataLayerInstance*> NotLoadedDataLayers = AsConst(*this).GetWorldDataLayersNotLoadedInEditor(InWorld);
	TArray<UDataLayerInstance*> NotLoadedDataLayersMutable;
	Algo::Transform(NotLoadedDataLayers, NotLoadedDataLayersMutable, [](const UDataLayerInstance* DataLayerInstance) { return const_cast<UDataLayerInstance*>(DataLayerInstance); });
	return NotLoadedDataLayersMutable;
}

TArray<const UDataLayerInstance*> UWorldPartitionEditorPerProjectUserSettings::GetWorldDataLayersLoadedInEditor(UWorld* InWorld) const
{
	TArray<const UDataLayerInstance*> LoadedDataLayers;

	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(InWorld))
	{
		if (const FWorldPartitionPerWorldSettings* PerWorldSettings = GetWorldPartitionPerWorldSettings(InWorld))
		{
			for (const TSoftObjectPtr<const UDataLayerAsset>& DataLayerAsset : PerWorldSettings->LoadedDataLayers)
			{
				if (DataLayerAsset != nullptr)
				{
					const UDataLayerInstance* DataLayerInstance = DataLayerManager->GetDataLayerInstance(DataLayerAsset.Get());
					if (DataLayerInstance != nullptr)
					{
						LoadedDataLayers.Emplace(DataLayerInstance);
					}
				}
			}
		}
	}

	return LoadedDataLayers;
}

TArray<UDataLayerInstance*> UWorldPartitionEditorPerProjectUserSettings::GetWorldDataLayersLoadedInEditor(UWorld* InWorld)
{
	TArray<const UDataLayerInstance*> LoadedDataLayers = AsConst(*this).GetWorldDataLayersLoadedInEditor(InWorld);
	TArray<UDataLayerInstance*> LoadedDataLayersMutable;
	Algo::Transform(LoadedDataLayers, LoadedDataLayersMutable, [](const UDataLayerInstance* DataLayerInstance) { return const_cast<UDataLayerInstance*>(DataLayerInstance); });
	return LoadedDataLayersMutable;
}

const FWorldPartitionPerWorldSettings* UWorldPartitionEditorPerProjectUserSettings::GetWorldPartitionPerWorldSettings(UWorld* InWorld) const
{
	if (!ShouldLoadSettings(InWorld))
	{
		return nullptr;
	}

	if (const FWorldPartitionPerWorldSettings* ExistingPerWorldSettings = PerWorldEditorSettings.Find(TSoftObjectPtr<UWorld>(InWorld)))
	{
		return ExistingPerWorldSettings;
	}
	else if (const FWorldPartitionPerWorldSettings* DefaultPerWorldSettings = InWorld->GetWorldSettings()->GetDefaultWorldPartitionSettings())
	{
		return DefaultPerWorldSettings;
	}

	return nullptr;
}

bool UWorldPartitionEditorPerProjectUserSettings::ShouldSaveSettings(const UWorld* InWorld) const
{
	return InWorld && !InWorld->IsGameWorld() && (InWorld->WorldType != EWorldType::Inactive) && FPackageName::DoesPackageExist(InWorld->GetPackage()->GetName());
}

bool UWorldPartitionEditorPerProjectUserSettings::ShouldLoadSettings(const UWorld* InWorld) const
{
	return InWorld && (InWorld->WorldType != EWorldType::Inactive);
}

#endif
