// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageTracker.h"

#include "CookOnTheFlyServerInterface.h"
#include "CookPackageData.h"
#include "CookPlatformManager.h"
#include "CookProfiling.h"
#include "Misc/ScopeRWLock.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

namespace UE::Cook
{

void FThreadSafeUnsolicitedPackagesList::AddCookedPackage(const FFilePlatformRequest& PlatformRequest)
{
	FScopeLock S(&SyncObject);
	CookedPackages.Add(PlatformRequest);
}

void FThreadSafeUnsolicitedPackagesList::GetPackagesForPlatformAndRemove(const ITargetPlatform* Platform, TArray<FName>& PackageNames)
{
	FScopeLock _(&SyncObject);

	for (int I = CookedPackages.Num() - 1; I >= 0; --I)
	{
		FFilePlatformRequest& Request = CookedPackages[I];

		if (Request.GetPlatforms().Contains(Platform))
		{
			// remove the platform
			Request.RemovePlatform(Platform);
			PackageNames.Emplace(Request.GetFilename());

			if (Request.GetPlatforms().Num() == 0)
			{
				CookedPackages.RemoveAt(I);
			}
		}
	}
}

void FThreadSafeUnsolicitedPackagesList::Empty()
{
	FScopeLock _(&SyncObject);
	CookedPackages.Empty();
}

FPackageTracker::FPackageTracker(UCookOnTheFlyServer& InCOTFS)
	:COTFS(InCOTFS)
{
	ActiveInstances = new FPackageStreamInstancedPackageContainer(); // RefCounted
}

FPackageTracker::~FPackageTracker()
{
	Unsubscribe();
}

void FPackageTracker::Unsubscribe()
{
	if (!bSubscribed)
	{
		return;
	}

	bSubscribed = false;
	GUObjectArray.RemoveUObjectDeleteListener(this);
	GUObjectArray.RemoveUObjectCreateListener(this);
	FCoreUObjectDelegates::OnEndLoadPackage.RemoveAll(this);
}

void FPackageTracker::InitializeTracking(TSet<FName>& OutStartupPackages)
{
	check(!bTrackingInitialized);

	LLM_SCOPE_BYTAG(Cooker);

	FWriteScopeLock ScopeLock(Lock);
	OutStartupPackages.Empty();
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Package = *It;

		if (Package->GetOuter() == nullptr && Package != GetTransientPackage())
		{
			LoadedPackages.Add(Package);
			OutStartupPackages.Add(Package->GetFName());
		}
	}

	TMap<FName, FInstigator> MapOfNewPackages;
	MapOfNewPackages.Reserve(LoadedPackages.Num());
	for (UPackage* Package : LoadedPackages)
	{
		MapOfNewPackages.Add(Package->GetFName(), FInstigator(EInstigator::StartupPackage));
	}

	GUObjectArray.AddUObjectDeleteListener(this);
	GUObjectArray.AddUObjectCreateListener(this);
	FCoreUObjectDelegates::OnEndLoadPackage.AddRaw(this, &FPackageTracker::OnEndLoadPackage);

	bSubscribed = true;

	TArray<TPair<FName, ECookLoadType>> StartupPackageLoadTypes;
	FCookLoadScope::SetCookerStartupComplete(StartupPackageLoadTypes);

	for (TPair<FName, ECookLoadType>& Pair : StartupPackageLoadTypes)
	{
		switch (Pair.Value)
		{
		case ECookLoadType::EditorOnly:
		{
			OutStartupPackages.Remove(Pair.Key);
			FInstigator* Instigator = MapOfNewPackages.Find(Pair.Key);
			if (Instigator)
			{
				*Instigator = FInstigator(EInstigator::EditorOnlyLoad);
			}
			break;
		}
		case ECookLoadType::UsedInGame:
		{
			FInstigator* Instigator = MapOfNewPackages.Find(Pair.Key);
			if (Instigator)
			{
				*Instigator = FInstigator(EInstigator::StartupPackageCookLoadScope);
			}
			break;
		}
		default:
			break;
		}
	}
	bTrackingInitialized = true;

	PackageStream.Reserve(MapOfNewPackages.Num());
	for (TPair<FName, FInstigator>& Pair : MapOfNewPackages)
	{
		PackageStream.Add({ Pair.Key, MoveTemp(Pair.Value), EPackageStreamEvent::PackageLoad });
	}
}

TArray<FPackageStreamEvent> FPackageTracker::GetPackageStream()
{
	check(bTrackingInitialized);

	FWriteScopeLock ScopeLock(Lock);
	TArray<FPackageStreamEvent> Result = MoveTemp(PackageStream);
	PackageStream.Reset();
	return Result;
}

void FPackageTracker::NotifyUObjectCreated(const class UObjectBase* Object, int32 Index)
{
	if (Object->GetClass() == UPackage::StaticClass())
	{
		UPackage* Package = const_cast<UPackage*>(static_cast<const UPackage*>(Object));
		if (Package->GetOuter() == nullptr) // Nested packages are no longer created, but can still exist in old data.
		{
			OnCreatePackage(Package->GetFName());
			LoadedPackages.Add(Package);
		}
	}
}

FInstigator FPackageTracker::GetPackageCreationInstigator() const
{
#if UE_WITH_PACKAGE_ACCESS_TRACKING
	PackageAccessTracking_Private::FTrackedData* AccumulatedScopeData =
		PackageAccessTracking_Private::FPackageAccessRefScope::GetCurrentThreadAccumulatedData();
	FName ReferencerName(AccumulatedScopeData ? AccumulatedScopeData->PackageName : NAME_None);
#else
	FName ReferencerName(NAME_None);
#endif
	EInstigator InstigatorType;
	switch (FCookLoadScope::GetCurrentValue())
	{
	case ECookLoadType::EditorOnly:
		InstigatorType = EInstigator::EditorOnlyLoad;
		break;
	case ECookLoadType::UsedInGame:
		InstigatorType = EInstigator::SaveTimeSoftDependency;
		break;
	default:
		InstigatorType = EInstigator::Unsolicited;
		break;
	}
	return FInstigator(InstigatorType, ReferencerName);
}

void FPackageTracker::OnCreatePackage(FName PackageName)
{
	LLM_SCOPE_BYTAG(Cooker);
#if ENABLE_COOK_STATS
	++DetailedCookStats::NumDetectedLoads;
#endif
	FInstigator Instigator = GetPackageCreationInstigator();
	if (Instigator.Category == EInstigator::Unsolicited && COTFS.bHiddenDependenciesDebug)
	{
		COTFS.OnDiscoveredPackageDebug(PackageName, Instigator);
	}

	FWriteScopeLock ScopeLock(Lock);
	if (ExpectedNeverLoadPackages.Contains(PackageName))
	{
		UE_LOG(LogCook, Verbose, TEXT("SoftGC PoorPerformance: Reloaded package %s."),
			*WriteToString<256>(PackageName));
	}

	// We store packages by name rather than by pointer, because they might have their name changed. When
	// external actors are moved out of their external package, we rename the package to <PackageName>_Trash.
	// We want to report a load dependency on the package as it was originally loaded; we don't want to report
	// the renamed packagename if it gets renamed after load.
	ActivePackageInstigators.FindOrAdd(PackageName, Instigator);
	PackageStream.Add({ PackageName, MoveTemp(Instigator), EPackageStreamEvent::PackageLoad });
}

void FPackageTracker::NotifyUObjectDeleted(const class UObjectBase* ObjectBase, int32 Index)
{
	if (ObjectBase->GetClass() == UPackage::StaticClass())
	{
		UPackage* Package = const_cast<UPackage*>(static_cast<const UPackage*>(ObjectBase));

		FWriteScopeLock ScopeLock(Lock);
		LoadedPackages.Remove(Package);
	}
	if (!bCollectingGarbage)
	{
		const UObject* Object = static_cast<const UObject*>(ObjectBase);
		COTFS.PackageDatas->CachedCookedPlatformDataObjectsOnDestroyedOutsideOfGC(Object);
	}
}

void FPackageTracker::OnUObjectArrayShutdown()
{
	Unsubscribe();
}

void FPackageTracker::RemapTargetPlatforms(const TMap<ITargetPlatform*, ITargetPlatform*>& Remap)
{
	RemapMapKeys(PlatformSpecificNeverCookPackages, Remap);
}

void FPackageTracker::OnEndLoadPackage(const FEndLoadPackageContext& Context)
{
	using namespace UE::AssetRegistry;

	// OnEndLoadPackage is the hook we use to test for whether a load is instanced; the CreatePackage hook is too early
	// (LoadPath is unknown in the case of instanced loads), and the OnSyncLoadPackage and OnAsyncLoadPackage hooks are
	// called too frequently - they are called even for packages that have already loaded, before LoadPackage checks
	// for whether they can early exit. OnEndLoadPackage has the information we need and is only called for packages
	// when they transition from unloaded to loaded.
	// 
	// Use this hook to record instanced loads in our lookup map; ProcessUnsolicitedPackages will respond to the
	// creation event for the packages by looking them up in the map and using the information we provide about their
	// AssetRegistry dependencies non-instanced referencers.
	TArray<TRefCountPtr<FPackageStreamInstancedPackage>, TInlineAllocator<10>> LoadedInstances;
	for (UPackage* Package : Context.LoadedPackages)
	{
		FName PackageName = Package->GetFName();
		FName LoadedName = Package->GetLoadedPath().GetPackageFName();
		if (PackageName == LoadedName || LoadedName.IsNone())
		{
			continue;
		}

		FInstigator Instigator(EInstigator::Unsolicited);
		{
			FWriteScopeLock ScopeLock(Lock);
			FInstigator* ActiveInstigator = ActivePackageInstigators.Find(PackageName);
			if (ActiveInstigator)
			{
				Instigator = *ActiveInstigator;
			}
		}

		TArray<FAssetDependency> PackageDependencies;
		COTFS.AssetRegistry->GetDependencies(FAssetIdentifier(LoadedName), PackageDependencies,
			EDependencyCategory::Package, EDependencyQuery::Hard);

		FWriteScopeLock ActiveInstancesScopeLock(ActiveInstances->Lock);
		FPackageStreamInstancedPackage*& Existing = ActiveInstances->Map.FindOrAdd(PackageName, nullptr);
		if (Existing)
		{
			if (Existing->LoadedName != LoadedName)
			{
				UE_LOG(LogCook, Error,
					TEXT("OnBeginLoadPackage was called twice for the same package with two different LoadedPaths. Ignoring the second call. PackageName: %s. LoadedPath1: %s. LoadedPath2: %s."),
					*PackageName.ToString(), *Existing->LoadedName.ToString(), *LoadedName.ToString());
				continue;
			}
			LoadedInstances.Emplace(Existing);
			continue;
		}

		TRefCountPtr<FPackageStreamInstancedPackage> InstancedPackage = new FPackageStreamInstancedPackage(*ActiveInstances);
		Existing = InstancedPackage.GetReference();

		InstancedPackage->PackageName = PackageName;
		InstancedPackage->LoadedName = LoadedName;
		InstancedPackage->Instigator = MoveTemp(Instigator);
		InstancedPackage->Dependencies.Reserve(PackageDependencies.Num());
		for (FAssetDependency& PackageDependency : PackageDependencies)
		{
			InstancedPackage->Dependencies.Add(PackageDependency.AssetId.PackageName, PackageDependency.Properties);
		}

		LoadedInstances.Emplace(MoveTemp(InstancedPackage));
	}

	if (LoadedInstances.IsEmpty())
	{
		// The usual path through this function is that there were no loaded instances. Clear the
		// ActivePackageInstigators to fulfill our design of removing that memory when we no longer need it, and then
		// return without further work.
		FWriteScopeLock ScopeLock(Lock);
		ActivePackageInstigators.Reset();
	}
	else
	{
		// Now that FPackageStreamInstancedPackage have been registered for all of the instanced loads that occurred
		// during the top-most LoadPackage call, calculate the ancestor non-instanced package referencer for each of
		// the instanced package loads.
		{
			FReadScopeLock ActiveInstancesScopeLock(ActiveInstances->Lock);
			for (TRefCountPtr<FPackageStreamInstancedPackage>& InstancedPackage : LoadedInstances)
			{
				TSet<FPackageStreamInstancedPackage*> Visited;
				InstancedPackage->FlattenReferencer(ActiveInstancesScopeLock, Visited);
			}
		}

		// The FPackageStreamInstancedPackage we created need to remain available until all of the package creation
		// records created during their load and that we have added into the PackageStream have been processed by
		// ProcessUnsolicitedPackages. Add end-of-data-lifetime markers for the FPackageStreamInstancedPackages
		// into the PackageStream, after all of those creation records.
		FWriteScopeLock ScopeLock(Lock);
		for (TRefCountPtr<FPackageStreamInstancedPackage>& Instance : LoadedInstances)
		{
			PackageStream.Add({ Instance->PackageName, FInstigator(), EPackageStreamEvent::InstancedPackageEndLoad,
				TRefCountPtr<const FPackageStreamInstancedPackage>(Instance.GetReference()) });
		}

		// Clear ActivePackageInstigators to fulfill our design of removing that memory when we no longer need it.
		ActivePackageInstigators.Reset();
	}
}

TRefCountPtr<const FPackageStreamInstancedPackage> FPackageTracker::FindInstancedPackage(FName PackageName)
{
	FWriteScopeLock ActiveInstancesScopeLock(ActiveInstances->Lock);
	FPackageStreamInstancedPackage** Existing = ActiveInstances->Map.Find(PackageName);
	return TRefCountPtr<const FPackageStreamInstancedPackage>(Existing ? *Existing : nullptr);
}

EInstigator FPackageTracker::MergeReferenceCategories(EInstigator Parent, EInstigator Child)
{
	// EditorOnly -> 0, Unsolicited -> 1, UsedInGame -> 2. Return Min(Child, Parent)
	switch (Parent)
	{
	case EInstigator::EditorOnlyLoad: [[fallthrough]];
	case EInstigator::HardEditorOnlyDependency:
		return Parent;
	case EInstigator::Unsolicited:
		switch (Child)
		{
		case EInstigator::EditorOnlyLoad: [[fallthrough]];
		case EInstigator::HardEditorOnlyDependency:
			return Child;
		default:
			// Child is Unsolicited or UsedInGame, 
			return Parent;
		}
	default:
		return Child;
	}
}

FPackageStreamInstancedPackage::FPackageStreamInstancedPackage(FPackageStreamInstancedPackageContainer& InContainer)
	: Container(&InContainer)
{
}

FPackageStreamInstancedPackage::~FPackageStreamInstancedPackage()
{
	{
		FWriteScopeLock ActiveInstancesScopeLock(Container->Lock);
		Container->Map.Remove(PackageName);
	}
	Container.SafeRelease();
}

void FPackageStreamInstancedPackage::FlattenReferencer(FReadScopeLock& CalledInsideActiveInstancesLock,
	TSet<FPackageStreamInstancedPackage*>& Visited)
{
	bool bAlreadyVisited;
	Visited.Add(this, &bAlreadyVisited);
	if (bAlreadyVisited)
	{
		UE_LOG(LogCook, Error, TEXT("Cycle detected in InstancedPackage referencers. PackageName == %s"),
			*PackageName.ToString());
		return;
	}
	if (Instigator.Referencer.IsNone())
	{
		return;
	}
	FPackageStreamInstancedPackage** ReferencerPtr = Container->Map.Find(Instigator.Referencer);
	if (!ReferencerPtr)
	{
		return;
	}

	FPackageStreamInstancedPackage* Referencer = *ReferencerPtr;
	Referencer->FlattenReferencer(CalledInsideActiveInstancesLock, Visited);

	Instigator.Referencer = Referencer->Instigator.Referencer;
	Instigator.Category = FPackageTracker::MergeReferenceCategories(
		Referencer->Instigator.Category, Instigator.Category);
}

} // namespace UE::Cook
