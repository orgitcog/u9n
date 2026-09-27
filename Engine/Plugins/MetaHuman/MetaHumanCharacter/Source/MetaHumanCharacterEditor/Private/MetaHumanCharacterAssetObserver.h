// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"
#include "Delegates/IDelegateInstance.h"

struct FMetaHumanObserverChanges
{
	enum class EChangeType : uint8
	{
		Added,
		Removed,
		Modified
	};

	TMap<EChangeType, TArray<TSoftObjectPtr<UObject>>> Changes;
	FName Dir;
};

DECLARE_DELEGATE_OneParam(FOnObservedDirectoryChanged, const FMetaHumanObserverChanges& /*Changes*/);

struct FFileChangeData;
struct FAssetData;

/**
 * Can be used to put a directory on a watch - whenever the
 * content of the directory changes, the callback is triggered.
 */
class FMetaHumanCharacterAssetObserver
{
public:
	UE_NONCOPYABLE(FMetaHumanCharacterAssetObserver)
	~FMetaHumanCharacterAssetObserver();

	static FMetaHumanCharacterAssetObserver& Get();

	/**
	 * Checks if we're already watching a directory
	 */
	bool IsDirectoryObserved(const FName& InDir) const;

	/**
	 * Starts watching the given directory.
	 * 
	 * Param InDir must be long package name.
	 */
	bool StartObserving(const FName& InDir);

	/**
	 * Removes a directory from the watchlist.
	 * 
	 * Param InDir must be long package name.
	 */
	bool StopObserving(const FName& InDir);

	/**
	 * Removes all observers.
	 */
	void StopObserving();
	
	/**
	 * Adds subscribed to the observer.
	 */
	FDelegateHandle SubscribeToObserver(const FName& InDir, const FOnObservedDirectoryChanged& InCallback);

	/**
	 * Removes a subscriber for a directory from a given callback.
	 */
	bool UnsubscribeFromObserver(const FName& InDir, const FOnObservedDirectoryChanged& InCallback);

	/**
	 * Removes a subscriber for a directory from a given handle.
	 */
	bool UnsubscribeFromObserver(const FName& InDir, const FDelegateHandle& InHandle);

	/**
	 * Queries assets on the given directory.
	 */
	bool GetAssets(
		const FName& InDir,
		const TSet<TSubclassOf<UObject>>& InClassesToFilter,
		TArray<FAssetData>& OutAssets);

	/**
	 * Queries wardrobe assets on the given directory, filering by principal item classes.
	 */
	bool GetWardrobeAssets(
		const FName& InDir,
		const TSet<TSubclassOf<UObject>>& InClassesToFilter,
		TArray<FAssetData>& OutAssets);

private:
	FMetaHumanCharacterAssetObserver() = default;

	void OnDirectoryChanged(const TArray<FFileChangeData>& InChanges, const FName InDir);

	struct FObserverData
	{
		FDelegateHandle DirWatcherHandle;
		FString AbsDir;

		DECLARE_MULTICAST_DELEGATE_OneParam(FOnObservedDirectoryChangedDelegate, const FMetaHumanObserverChanges& /*Changes*/);
		FOnObservedDirectoryChangedDelegate Callback;
	};

	TMap<FName, FObserverData> ObserverData;
};
