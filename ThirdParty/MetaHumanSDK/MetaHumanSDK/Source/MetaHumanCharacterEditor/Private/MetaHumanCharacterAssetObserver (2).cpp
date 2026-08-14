// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterAssetObserver.h"

#include "Algo/Transform.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#include "MetaHumanWardrobeItem.h"
#include "Misc/PackageName.h"

namespace UE::MetaHuman::Private
{
	IDirectoryWatcher* GetDirectoryWatcherIfLoaded()
	{
		static const FName DirectoryWatcherModuleName = FName("DirectoryWatcher");

		if (FModuleManager::Get().IsModuleLoaded(DirectoryWatcherModuleName))
		{
			return FModuleManager::GetModuleChecked<FDirectoryWatcherModule>(DirectoryWatcherModuleName).Get();
		}

		return nullptr;
	}

	FARCompiledFilter GetAssetFilter(FName InPackagePath, const TSet<TSubclassOf<UObject>>& InClassesToFilter)
	{
		FARCompiledFilter Filter;
		Filter.PackagePaths.Add(InPackagePath);
		for (const TSubclassOf<UObject>& Class : InClassesToFilter)
		{
			Filter.ClassPaths.Add(FTopLevelAssetPath(Class));
		}

		return Filter;
	}

	TArray<FAssetData> GetAssetsInDirectory(const FName& InDirectory, const TSet<TSubclassOf<UObject>>& InClassesToFilter)
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(GetAssetFilter(InDirectory, InClassesToFilter), Assets);

		return Assets;
	}

	TArray<FAssetData> GetWardrobeItemsInDirectory(const FName& InDirectory, const TSet<TSubclassOf<UObject>>& InClassesToFilter)
	{
		TSet<FTopLevelAssetPath> ClassAssetPathsToFilter;
		Algo::Transform(InClassesToFilter, ClassAssetPathsToFilter, [](TSubclassOf<UObject> Class) { return FTopLevelAssetPath(Class); });

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(GetAssetFilter(InDirectory, { UMetaHumanWardrobeItem::StaticClass() }), Assets);

		for (int32 AssetIndex = Assets.Num() - 1; AssetIndex >= 0; AssetIndex--)
		{
			const FAssetData& Asset = Assets[AssetIndex];
			const UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(Asset.GetAsset());

			if (WardrobeItem)
			{
				FAssetData PrincipalAsset;
				if (AssetRegistry.TryGetAssetByObjectPath(WardrobeItem->PrincipalAsset.ToSoftObjectPath(), PrincipalAsset) == UE::AssetRegistry::EExists::Exists
					&& ClassAssetPathsToFilter.Contains(PrincipalAsset.AssetClassPath))
				{
					// This asset matches; keep it
					continue;
				}
			}

			// This asset doesn't match; remove it
			Assets.RemoveAtSwap(AssetIndex, 1, EAllowShrinking::No);
		}

		return Assets;
	}
}

FMetaHumanCharacterAssetObserver& FMetaHumanCharacterAssetObserver::Get()
{
	static FMetaHumanCharacterAssetObserver Instance;

	return Instance;
}

FMetaHumanCharacterAssetObserver::~FMetaHumanCharacterAssetObserver()
{
	StopObserving();
}

bool FMetaHumanCharacterAssetObserver::IsDirectoryObserved(const FName& InDir) const
{
	return ObserverData.Contains(InDir);
}

bool FMetaHumanCharacterAssetObserver::StartObserving(const FName& InDir)
{
	if (ObserverData.Contains(InDir))
	{
		return false;
	}

	FString LongPackageName;

	if (!FPackageName::TryConvertLongPackageNameToFilename(InDir.ToString(), LongPackageName))
	{
		return false;
	}

	if (IDirectoryWatcher* DirectoryWatcher = UE::MetaHuman::Private::GetDirectoryWatcherIfLoaded())
	{
		FDelegateHandle DirWatcherHandle;

		const FString AbsDir = FPaths::ConvertRelativePathToFull(LongPackageName);

		DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
			AbsDir,
			IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FMetaHumanCharacterAssetObserver::OnDirectoryChanged, InDir),
			DirWatcherHandle);

		
		FObserverData& Data = ObserverData.Add(InDir);
		Data.DirWatcherHandle = MoveTemp(DirWatcherHandle);
		Data.AbsDir = AbsDir;

		return true;
	}
	
	return false;
}

bool FMetaHumanCharacterAssetObserver::StopObserving(const FName& InDir)
{
	if (const FObserverData* FoundData = ObserverData.Find(InDir))
	{
		if (IDirectoryWatcher* DirectoryWatcher = UE::MetaHuman::Private::GetDirectoryWatcherIfLoaded())
		{
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(FoundData->AbsDir, FoundData->DirWatcherHandle);
			ObserverData.Remove(InDir);

			return true;
		}
	}

	return false;
}

void FMetaHumanCharacterAssetObserver::StopObserving()
{
	if (IDirectoryWatcher* DirectoryWatcher = UE::MetaHuman::Private::GetDirectoryWatcherIfLoaded())
	{
		TArray<FName> Directories;
		ObserverData.GenerateKeyArray(Directories);

		for (const FName& Dir : Directories)
		{
			StopObserving(Dir);
		}
	}
}

FDelegateHandle FMetaHumanCharacterAssetObserver::SubscribeToObserver(const FName& InDir, const FOnObservedDirectoryChanged& InCallback)
{
	if (FObserverData* FoundData = ObserverData.Find(InDir))
	{
		return FoundData->Callback.Add(InCallback);
	}

	return FDelegateHandle();
}

bool FMetaHumanCharacterAssetObserver::UnsubscribeFromObserver(const FName& InDir, const FOnObservedDirectoryChanged& InCallback)
{
	if (FObserverData* FoundData = ObserverData.Find(InDir))
	{
		if (FoundData->Callback.Remove(InCallback.GetHandle()))
		{
			return true;
		}
	}

	return false;
}

bool FMetaHumanCharacterAssetObserver::UnsubscribeFromObserver(const FName& InDir, const FDelegateHandle& InHandle)
{
	if (FObserverData* FoundData = ObserverData.Find(InDir))
	{
		return FoundData->Callback.Remove(InHandle);
	}

	return false;
}

bool FMetaHumanCharacterAssetObserver::GetAssets(
	const FName& InDir,
	const TSet<TSubclassOf<UObject>>& InClassesToFilter,
	TArray<FAssetData>& OutAssets)
{
	if (const FObserverData* FoundData = ObserverData.Find(InDir))
	{
		OutAssets = UE::MetaHuman::Private::GetAssetsInDirectory(InDir, InClassesToFilter);

		return true;
	}

	return false;
}

bool FMetaHumanCharacterAssetObserver::GetWardrobeAssets(
	const FName& InDir,
	const TSet<TSubclassOf<UObject>>& InClassesToFilter,
	TArray<FAssetData>& OutAssets)
{
	if (const FObserverData* FoundData = ObserverData.Find(InDir))
	{
		OutAssets = UE::MetaHuman::Private::GetWardrobeItemsInDirectory(InDir, InClassesToFilter);

		return true;
	}

	return false;
}

void FMetaHumanCharacterAssetObserver::OnDirectoryChanged(const TArray<FFileChangeData>& InChanges, const FName InDir)
{
	if (const FObserverData* FoundObserverData = ObserverData.Find(InDir))
	{
		FMetaHumanObserverChanges Result;
		Result.Dir = InDir;

		for (FFileChangeData FileChange : InChanges)
		{
			FString LongPackageName;

			if (FPackageName::TryConvertFilenameToLongPackageName(FileChange.Filename, LongPackageName))
			{
				LongPackageName += FString::Printf(TEXT(".%s"), *FPaths::GetBaseFilename(FileChange.Filename));
				TSoftObjectPtr<UObject> Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(FTopLevelAssetPath(LongPackageName)));

				if (FileChange.Action == FFileChangeData::FCA_Added)
				{
					Result.Changes.FindOrAdd(FMetaHumanObserverChanges::EChangeType::Added).Add(Asset);
				}
				else if (FileChange.Action == FFileChangeData::FCA_Removed)
				{
					Result.Changes.FindOrAdd(FMetaHumanObserverChanges::EChangeType::Removed).Add(Asset);
				}
				else if (FileChange.Action == FFileChangeData::FCA_Modified)
				{
					Result.Changes.FindOrAdd(FMetaHumanObserverChanges::EChangeType::Modified).Add(Asset);
				}
			}
		}

		if (FoundObserverData->Callback.IsBound())
		{
			FoundObserverData->Callback.Broadcast(Result);
		}
	}
}
