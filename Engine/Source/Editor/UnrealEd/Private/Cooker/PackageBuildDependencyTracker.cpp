// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageBuildDependencyTracker.h"
#include "Misc/ConfigAccessTracking.h"

#if UE_WITH_PACKAGE_ACCESS_TRACKING
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Async/UniqueLock.h"
#include "Cooker/CookConfigAccessTracker.h"
#include "Cooker/CookDependency.h"
#include "CookOnTheSide/CookLog.h"
#include "HAL/Platform.h"
#include "Logging/LogMacros.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#endif

#if UE_WITH_PACKAGE_ACCESS_TRACKING

DEFINE_LOG_CATEGORY_STATIC(LogPackageBuildDependencyTracker, Log, All);

FPackageBuildDependencyTracker FPackageBuildDependencyTracker::Singleton;

void FPackageBuildDependencyTracker::Disable()
{
	if (bEnabled)
	{
		UE::CoreUObject::RemoveObjectHandleReadCallback(ObjectHandleReadHandle);
		ObjectHandleReadHandle = UE::CoreUObject::FObjectHandleTrackingCallbackId {};
		bEnabled = false;
	}
}

bool FPackageBuildDependencyTracker::IsEnabled() const
{
	return bEnabled;
}

void FPackageBuildDependencyTracker::DumpStats() const
{
	if (!IsEnabled())
	{
		return;
	}

	UE::TUniqueLock RecordsScopeLock(RecordsLock);
	uint64 ReferencingPackageCount = 0;
	uint64 ReferenceCount = 0;
	for (const TPair<FName, TMap<FBuildDependencyAccessData, FResultProjectionList>>& PackageAccessRecord : Records)
	{
		++ReferencingPackageCount;
		for (const TPair<FBuildDependencyAccessData, FResultProjectionList>& AccessedData : PackageAccessRecord.Value)
		{
			++ReferenceCount;
		}
	}
	UE_LOG(LogPackageBuildDependencyTracker, Display, TEXT("Package Accesses (%u referencing packages with a total of %u unique accesses)"), ReferencingPackageCount, ReferenceCount);

	constexpr bool bDetailedDump = false;
	if (bDetailedDump)
	{
		UE_LOG(LogPackageBuildDependencyTracker, Display, TEXT("========================================================================="));
		for (const TPair<FName, TMap<FBuildDependencyAccessData, FResultProjectionList>>& PackageAccessRecord : Records)
		{
			UE_LOG(LogPackageBuildDependencyTracker, Display, TEXT("%s:"), *PackageAccessRecord.Key.ToString());
			for (const TPair<FBuildDependencyAccessData, FResultProjectionList>& AccessedData : PackageAccessRecord.Value)
			{
				UE_LOG(LogPackageBuildDependencyTracker, Display, TEXT("    %s"), *AccessedData.Key.ReferencedPackage.ToString());
			}
		}
	}
}

TArray<TPair<FBuildDependencyAccessData, FResultProjectionList>> FPackageBuildDependencyTracker::GetAccessDatas(FName ReferencerPackage) const
{
	UE::TUniqueLock RecordsScopeLock(Singleton.RecordsLock);
	const TMap<FBuildDependencyAccessData, FResultProjectionList>* ReferencerMap = Records.Find(ReferencerPackage);
	if (!ReferencerMap)
	{
		return TArray<TPair<FBuildDependencyAccessData, FResultProjectionList>>();
	}
	return ReferencerMap->Array();
}

FPackageBuildDependencyTracker::FPackageBuildDependencyTracker()
{
	ObjectHandleReadHandle = UE::CoreUObject::AddObjectHandleReadCallback(StaticOnObjectHandleRead);
	bEnabled = true;
}

FPackageBuildDependencyTracker::~FPackageBuildDependencyTracker()
{
	Disable();
}

static bool ShouldSkipDependency(const UObject* Object)
{
	return !Object ||
		!GUObjectArray.IsValidIndex(Object) ||
		!Object->HasAnyFlags(RF_Public) ||
		(Object->GetClass() == UClass::StaticClass());
}

static FName NAME_EngineTransient(TEXT("/Engine/Transient"));

void FPackageBuildDependencyTracker::StaticOnObjectHandleRead(const TArrayView<const UObject* const>& Objects)
{
	int Count = Objects.Num();
	if(Count == 0 || (Count == 1 && ShouldSkipDependency(Objects[0])))
	{
		return;
	}

	PackageAccessTracking_Private::FTrackedData* AccumulatedScopeData = PackageAccessTracking_Private::FPackageAccessRefScope::GetCurrentThreadAccumulatedData();
	if (!AccumulatedScopeData)
	{
		return;
	}

	FName Referencer = AccumulatedScopeData->PackageName;
	FName CookResultProjection = AccumulatedScopeData->CookResultProjection;
	if (AccumulatedScopeData->BuildOpName.IsNone()
		| CookResultProjection == UE::Cook::ResultProjection::None // -V792
		| Referencer.IsNone() // -V792
		| Referencer == NAME_EngineTransient) // -V792
	{
		return;
	}

	for (const UObject* ReadObject : Objects)
	{
		if (ShouldSkipDependency(ReadObject))
		{
			continue;
		}

		UPackage* ReferencedPackage = ReadObject->GetOutermost();
		FName Referenced = ReferencedPackage->GetFName();
		if ((Referencer == Referenced) || ReferencedPackage->HasAnyPackageFlags(PKG_CompiledIn)
			|| Referenced == NAME_EngineTransient)
		{
			continue;
		}

		if (AccumulatedScopeData->OpName == PackageAccessTrackingOps::NAME_NoAccessExpected)
		{
			UE_LOG(LogPackageBuildDependencyTracker, Warning, TEXT("Object %s is referencing object %s inside of a NAME_NoAccessExpected scope. Programmer should narrow the scope or debug the reference."),
				*Referencer.ToString(), *Referenced.ToString());
		}

		LLM_SCOPE_BYNAME(TEXTVIEW("PackageBuildDependencyTracker"));

		FBuildDependencyAccessData AccessData{ Referenced, AccumulatedScopeData->TargetPlatform };
		UE::TUniqueLock RecordsScopeLock(Singleton.RecordsLock);

		bool bNeedsAdd = true;
		if (Referencer == Singleton.LastReferencer)
		{
			if (AccessData == Singleton.LastAccessData)
			{
				if (CookResultProjection == UE::Cook::ResultProjection::All
					&& Singleton.LastCookResultProjection == UE::Cook::ResultProjection::All)
				{
					bNeedsAdd = false;
				}
				else
				{
					Singleton.LastCookResultProjection = CookResultProjection;
				}
			}
			else
			{
				Singleton.LastCookResultProjection = CookResultProjection;
				Singleton.LastAccessData = AccessData;
			}
		}
		else
		{
			Singleton.LastCookResultProjection = CookResultProjection;
			Singleton.LastAccessData = AccessData;
			Singleton.LastReferencer = Referencer;
			Singleton.LastReferencerMap = &Singleton.Records.FindOrAdd(Referencer);
		}
		if (bNeedsAdd)
		{
			if (CookResultProjection == UE::Cook::ResultProjection::All)
			{
				Singleton.LastReferencerMap->FindOrAdd(AccessData).AddProjectionAll();
			}
			else
			{
				Singleton.LastReferencerMap->FindOrAdd(AccessData).AddProjection(CookResultProjection, ReadObject->GetClass()->GetClassPathName());
			}

		}
	}
}

void FResultProjectionList::AddProjectionAll(bool* bOutExists)
{
	if (bHasAll)
	{
		if (bOutExists)
		{
			*bOutExists = true;
		}
		return;
	}
	bHasAll = true;
	Classes.Empty();
	ResultProjections.Empty();
	if (bOutExists)
	{
		*bOutExists = false;
	}
}

void FResultProjectionList::AddProjection(FName CookResultProjection, FTopLevelAssetPath ClassPath, bool* bOutExists)
{
	if (bHasAll)
	{
		if (bOutExists)
		{
			*bOutExists = true;
		}
		return;
	}

	if (CookResultProjection == UE::Cook::ResultProjection::PackageAndClass)
	{
		Classes.Add(ClassPath, bOutExists);
		ResultProjections.Add(CookResultProjection);
	}
	else
	{
		// ResultProjection::None and ResultProjection::All should have been handled by caller and this function not called
		check(CookResultProjection != UE::Cook::ResultProjection::None);
		check(CookResultProjection != UE::Cook::ResultProjection::All);
		ResultProjections.Add(CookResultProjection, bOutExists);
	}
}

#endif // UE_WITH_PACKAGE_ACCESS_TRACKING

void DumpBuildDependencyTrackerStats()
{
#if UE_WITH_PACKAGE_ACCESS_TRACKING
	FPackageBuildDependencyTracker::Get().DumpStats();
#endif
#if UE_WITH_CONFIG_TRACKING && UE_WITH_OBJECT_HANDLE_TRACKING
	UE::ConfigAccessTracking::FCookConfigAccessTracker::Get().DumpStats();
#endif
}
