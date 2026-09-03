// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/PCGGraphCache.h"

#include "PCGComponent.h"
#include "PCGModule.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeRWLock.h"

static TAutoConsoleVariable<bool> CVarCacheEnabled(
	TEXT("pcg.Cache.Enabled"),
	true,
	TEXT("Enables the cache system."));

static TAutoConsoleVariable<bool> CVarCacheDebugging(
	TEXT("pcg.Cache.EnableDebugging"),
	false,
	TEXT("Enable various features for debugging the graph cache system."));

static TAutoConsoleVariable<int32> CVarCacheMemoryBudgetMB(
	TEXT("pcg.Cache.MemoryBudgetMB"),
	6144,
	TEXT("Memory budget for data in cache (MB)."));

static TAutoConsoleVariable<float> CVarCacheMemoryCleanupRatio(
	TEXT("pcg.Cache.MemoryCleanupRatio"),
	0.5f,
	TEXT("Target cache size ratio after triggering a cleanup (between 0 and 1.)."));

static TAutoConsoleVariable<bool> CVarCacheMemoryBudgetEnabled(
	TEXT("pcg.Cache.EnableMemoryBudget"),
	true,
	TEXT("Whether memory budget is enforced (items purged from cache to respect pcg.Cache.MemoryBudgetMB."));

static TAutoConsoleVariable<bool> CVarValidateElementToCacheEntryKeys(
	TEXT("pcg.Cache.Debug.ValidateElementToCacheEntryKeys"),
	false,
	TEXT("Validate ElementToCacheEntryKeys acceleration table (debug)."));

// Initial max number of entries graph cache
static const int32 GPCGGraphCacheInitialCapacity = 65536;

FPCGGraphCache::FPCGGraphCache()
	: CacheData(GPCGGraphCacheInitialCapacity)
{
}

FPCGGraphCache::~FPCGGraphCache()
{
	ClearCache();
}

bool FPCGGraphCache::GetFromCache(const FPCGGetFromCacheParams& Params, FPCGDataCollection& OutOutput) const
{
	if (!CVarCacheEnabled.GetValueOnAnyThread())
	{
		return false;
	}

	const UPCGNode* InNode = Params.Node;
	const IPCGElement* InElement = Params.Element;
	const IPCGGraphExecutionSource* InExecutionSource = Params.ExecutionSource;
	const FPCGCrc& InDependenciesCrc = Params.Crc;

	if(!InDependenciesCrc.IsValid())
	{
		UE_LOG(LogPCG, Warning, TEXT("Invalid dependencies passed to FPCGGraphCache::GetFromCache(), lookup aborted."));
		return false;
	}

	const bool bDebuggingEnabled = IsDebuggingEnabled() && InExecutionSource && InNode;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::GetFromCache);
		UE::TScopeLock ScopedLock(CacheLock);

		FPCGCacheEntryKey CacheKey(InElement, InDependenciesCrc);
		if (const FPCGDataCollection* Value = const_cast<FPCGGraphCache*>(this)->CacheData.FindAndTouch(CacheKey))
		{
			if (bDebuggingEnabled)
			{
				// Leading spaces to align log content with warnings below - helps readability a lot.
				UE_LOG(LogPCG, Log, TEXT("         [%s] %s\t\tCACHE HIT %u"), *InExecutionSource->GetExecutionState().GetDebugName(), *InNode->GetNodeTitle(EPCGNodeTitleType::ListView).ToString(), InDependenciesCrc.GetValue());
			}

			OutOutput = *Value;

			return true;
		}
		else
		{
			if (bDebuggingEnabled)
			{
				UE_LOG(LogPCG, Warning, TEXT("[%s] %s\t\tCACHE MISS %u"), *InExecutionSource->GetExecutionState().GetDebugName(), *InNode->GetNodeTitle(EPCGNodeTitleType::ListView).ToString(), InDependenciesCrc.GetValue());
			}

			return false;
		}
	}
}

void FPCGGraphCache::StoreInCache(const FPCGStoreInCacheParams& Params, const FPCGDataCollection& InOutput)
{
	if (!CVarCacheEnabled.GetValueOnAnyThread())
	{
		return;
	}

	const IPCGElement* InElement = Params.Element;
	const FPCGCrc& InDependenciesCrc = Params.Crc;

	if (!ensure(InDependenciesCrc.IsValid()))
	{
		return;
	}

	// Proxies should never go into the graph cache. These can hold onto large chunks of video memory.
	for (const FPCGTaggedData& Data : InOutput.TaggedData)
	{
		ensure(!Data.Data || Data.Data->IsCacheable());
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::StoreInCache);
		UE::TScopeLock ScopedLock(CacheLock);

		if (CacheData.Num() == CacheData.Max())
		{
			GrowCache_Unsafe();
		}

		const FPCGCacheEntryKey CacheKey(InElement, InDependenciesCrc);
		AddToCacheInternal(CacheKey, InOutput, /*bAddToMemory=*/true);
	}
}

void FPCGGraphCache::ClearCache()
{
	UE::TScopeLock ScopedLock(CacheLock);

	// Remove all entries
	ClearCacheInternal(CacheData.Max(), /*bClearMemory=*/true);
}

bool FPCGGraphCache::EnforceMemoryBudget()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::FPCGGraphCache::EnforceMemoryBudget);
	if (!CVarCacheEnabled.GetValueOnAnyThread())
	{
		return false;
	}

	if (!CVarCacheMemoryBudgetEnabled.GetValueOnAnyThread())
	{
		return false;
	}

	const uint64 MemoryBudget = static_cast<uint64>(CVarCacheMemoryBudgetMB.GetValueOnAnyThread()) * 1024 * 1024;
	if (TotalMemoryUsed <= MemoryBudget)
	{
		return false;
	}

	{
		UE::TScopeLock ScopedLock(CacheLock);
		const float MemoryCleanupRatio = FMath::Clamp(CVarCacheMemoryCleanupRatio.GetValueOnAnyThread(), 0.0f, 1.0f);
		const uint64 TargetCacheMemoryUsage = static_cast<uint64>(MemoryCleanupRatio * MemoryBudget);

		while (TotalMemoryUsed > TargetCacheMemoryUsage && CacheData.Num() > 0)
		{
			RemoveFromCacheInternal(CacheData.GetLeastRecentKey());
		}
		ValidateElementToCacheEntryKeys();
	}

	return true;
}

#if WITH_EDITOR
void FPCGGraphCache::CleanFromCache(const IPCGElement* InElement, const UPCGSettings* InSettings/*= nullptr*/)
{
	if (!InElement)
	{
		return;
	}

	if (IsDebuggingEnabled())
	{
		UE_LOG(LogPCG, Warning, TEXT("[] \t\tCACHE: PURGED [%s]"), InSettings ? *InSettings->GetDefaultNodeTitle().ToString() : TEXT("AnonymousElement"));
	}

	{
		UE::TScopeLock ScopedLock(CacheLock);

		ValidateElementToCacheEntryKeys();

		TSet<FPCGCacheEntryKey> ElementCacheEntryKeys;
		ElementToCacheEntryKeys.RemoveAndCopyValue(InElement, ElementCacheEntryKeys);

		for (const FPCGCacheEntryKey& Key : ElementCacheEntryKeys)
		{
			RemoveFromCacheInternal(Key);
		}

		ValidateElementToCacheEntryKeys();
	}
}

uint32 FPCGGraphCache::GetGraphCacheEntryCount(IPCGElement* InElement) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::GetFromCache);
	UE::TScopeLock ScopedLock(CacheLock);

	if (const TSet<FPCGCacheEntryKey>* ElementCacheEntryKeys = ElementToCacheEntryKeys.Find(InElement))
	{
		return ElementCacheEntryKeys->Num();
	}

	return 0;
}
#endif // WITH_EDITOR

void FPCGGraphCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::AddReferencedObjects);
	UE::TScopeLock ScopedLock(CacheLock);

	for (FPCGDataCollection& CacheEntry : CacheData)
	{
		CacheEntry.AddReferences(Collector);
	}
}

void FPCGGraphCache::ValidateElementToCacheEntryKeys() const
{
	if (CVarValidateElementToCacheEntryKeys.GetValueOnAnyThread())
	{
		int32 CacheKeyCount = 0;
		for (const auto& Kvp : ElementToCacheEntryKeys)
		{
			CacheKeyCount += Kvp.Value.Num();
		}

		check(CacheKeyCount == CacheData.Num());
	}
}

void FPCGGraphCache::ClearCacheInternal(int32 InMaxEntries, bool bClearMemory)
{
	if (bClearMemory)
	{
		MemoryRecords.Empty();
		TotalMemoryUsed = 0;
	}

	CacheData.Empty(InMaxEntries);
	ElementToCacheEntryKeys.Empty();
}

void FPCGGraphCache::AddToCacheInternal(const FPCGCacheEntryKey& InKey, const FPCGDataCollection& InCollection, bool bAddToMemory)
{
	// We currently grow the cache before calling add so this shouldn't be needed but if 
	// the rules change we need to make sure we keep ElementToCacheEntryKeys in sync
	if (CacheData.Num() == CacheData.Max())
	{
		RemoveFromCacheInternal(CacheData.GetLeastRecentKey());
	}

	CacheData.Add(InKey, InCollection);
	ElementToCacheEntryKeys.FindOrAdd(InKey.GetElement()).Add(InKey);

	if (bAddToMemory)
	{
		AddDataToAccountedMemory(InCollection);
	}

	ValidateElementToCacheEntryKeys();
}

void FPCGGraphCache::RemoveFromCacheInternal(const FPCGCacheEntryKey& InKey)
{
	if (TSet<FPCGCacheEntryKey>* ElementCacheEntryKeys = ElementToCacheEntryKeys.Find(InKey.GetElement()))
	{
		ElementCacheEntryKeys->Remove(InKey);
		if (ElementCacheEntryKeys->IsEmpty())
		{
			ElementToCacheEntryKeys.Remove(InKey.GetElement());
		}
	}

	if (const FPCGDataCollection* RemovedData = CacheData.Find(InKey))
	{
		RemoveFromMemoryTotal(*RemovedData);
		CacheData.Remove(InKey);
	}
}

void FPCGGraphCache::GrowCache_Unsafe()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCache::GrowCache_Unsafe);

	TLruCache<FPCGCacheEntryKey, FPCGDataCollection> CacheDataCopy(CacheData.Max());

	// Iteration begins from most recent, so this reverses the temporal order.
	for (TLruCache<FPCGCacheEntryKey, FPCGDataCollection>::TIterator It(CacheData); It; ++It)
	{
		CacheDataCopy.Add(It.Key(), It.Value());
	}

	// Resize and flush
	ClearCacheInternal(CacheData.Num() * 2, /*bClearMemory=*/false);

	UE_LOG(LogPCG, Log, TEXT("Graph cache doubled in capacity to %d entries."), CacheData.Max());

	// Copy back. Restore temporal order.
	for (TLruCache<FPCGCacheEntryKey, FPCGDataCollection>::TIterator It(CacheDataCopy); It; ++It)
	{
		AddToCacheInternal(It.Key(), It.Value(), /*bAddToMemory=*/false);
	}
}

void FPCGGraphCache::AddDataToAccountedMemory(const FPCGDataCollection& InCollection)
{
	for (const FPCGTaggedData& Data : InCollection.TaggedData)
	{
		if (Data.Data)
		{
			Data.Data->VisitDataNetwork([this](const UPCGData* Data)
			{
				if (Data)
				{
					// Find or add record
					if (FCachedMemoryRecord* ExistingRecord = MemoryRecords.Find(Data->UID))
					{
						ExistingRecord->InstanceCount++;
					}
					else
					{
						FResourceSizeEx ResSize = FResourceSizeEx(EResourceSizeMode::Exclusive);
						// Calculate data size. Function is non-const but is const-like, especially when
						// resource mode is Exclusive. The other mode calls a function to find all outer'd
						// objects which is non-const.
						const_cast<UPCGData*>(Data)->GetResourceSizeEx(ResSize);
						const SIZE_T DataSize = ResSize.GetDedicatedSystemMemoryBytes();

						FCachedMemoryRecord& NewRecord = MemoryRecords.Add(Data->UID);
						NewRecord.MemoryPerInstance = DataSize;
						NewRecord.InstanceCount = 1;
						TotalMemoryUsed += DataSize;
					}
				}
			});
		}
	}
}

void FPCGGraphCache::RemoveFromMemoryTotal(const FPCGDataCollection& InCollection)
{
	for (const FPCGTaggedData& Data : InCollection.TaggedData)
	{
		if (Data.Data)
		{
			Data.Data->VisitDataNetwork([this](const UPCGData* Data)
			{
				FCachedMemoryRecord* Record = Data ? MemoryRecords.Find(Data->UID) : nullptr;
				if (ensure(Record))
				{
					// Update instance count
					if (ensure(Record->InstanceCount > 0))
					{
						--Record->InstanceCount;
					}

					if (Record->InstanceCount == 0)
					{
						// Last instance removed, update accordingly
						if (TotalMemoryUsed >= Record->MemoryPerInstance)
						{
							TotalMemoryUsed -= Record->MemoryPerInstance;
						}
						else
						{
							// Should not normally reach here but it seems to happen in rare cases. Clamp to 0.
							TotalMemoryUsed = 0;
						}

						MemoryRecords.Remove(Data->UID);
					}
				}
			});
		}
	}
}

bool FPCGGraphCache::IsDebuggingEnabled() const
{
	return CVarCacheDebugging.GetValueOnAnyThread();
}
