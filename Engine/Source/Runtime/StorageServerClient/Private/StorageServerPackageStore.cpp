// Copyright Epic Games, Inc. All Rights Reserved.

#include "StorageServerPackageStore.h"
#include "IO/IoDispatcher.h"
#include "StorageServerConnection.h"
#include "Serialization/MemoryReader.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"
#include "CoreGlobalsInternal.h"

#if !UE_BUILD_SHIPPING

FStorageServerPackageStoreBackend::FStorageServerPackageStoreBackend(FStorageServerConnection& Connection)
{
	AsyncInit = MakeShared<FAsyncInitRunnable>(*this, Connection);
}

EPackageStoreEntryStatus FStorageServerPackageStoreBackend::GetPackageStoreEntry(FPackageId PackageId, FName PackageName,
	FPackageStoreEntry& OutPackageStoreEntry)
{
	// nb. the async init is highly likely to have finished by the time the first entry is requested
	if (AsyncInit.IsValid())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerPackageStoreWaitForInit);
		AsyncInit->WaitForCompletion();
		AsyncInit.Reset();
	}

	const FStoreEntry* FindEntry = StoreEntriesMap.Find(PackageId);

#if WITH_EDITOR
	// for now wrapping this in a HCE check, until we determine there are no side effects with, say, CookedCookers or UEFN
	if (IsRunningHybridCookedEditor())
	{
		// if we marked the package to be uncooked at runtime, or we marked the package at cook-time to always load uncooked,
		// return Missing for the cooked version, even if it exists in the Store
		if ((FindEntry && !!(FindEntry->Flags & EPackageStoreEntryFlags::LoadUncooked)))
		{
			return EPackageStoreEntryStatus::Missing;
		}
	}
#endif

	if (FindEntry)
	{
		OutPackageStoreEntry.ShaderMapHashes = FindEntry->ShaderMapHashes;
#if WITH_EDITOR
		// auto optional needs to request the optional chunk instead of regular, and because of that
		// we use the optional imports as if they are regular imports, and we leave bHasOptionalSegment
		// as false (this matches the FilePackageStore)
		if (EnumHasAnyFlags(FindEntry->Flags, EPackageStoreEntryFlags::AutoOptional))
		{
			OutPackageStoreEntry.ImportedPackageIds = FindEntry->OptionalSegmentImportedPackageIds;
			OutPackageStoreEntry.bReplaceChunkWithOptional = true;
		}
		// for manual optional, we report imported and optional imports as expected
		else
		{
			OutPackageStoreEntry.ImportedPackageIds = FindEntry->ImportedPackages;
			if (FindEntry->OptionalSegmentImportedPackageIds.Num() > 0)
			{
				OutPackageStoreEntry.OptionalSegmentImportedPackageIds = FindEntry->OptionalSegmentImportedPackageIds;
				OutPackageStoreEntry.bHasOptionalSegment = true;
			}
		}
#else
		OutPackageStoreEntry.ImportedPackageIds = FindEntry->ImportedPackages;
#endif

		return EPackageStoreEntryStatus::Ok;
	}
	else
	{
		return EPackageStoreEntryStatus::Missing;
	}
}




FStorageServerPackageStoreBackend::FAsyncInitRunnable::FAsyncInitRunnable(FStorageServerPackageStoreBackend& InOwner, FStorageServerConnection& InConnection)
	: Owner(InOwner)
	, Connection(InConnection)
	, IsCompleted(FPlatformProcess::GetSynchEventFromPool(true))
{
	if (FRunnableThread::Create( this, TEXT("StorageServerPackageStoreInit"), 0, EThreadPriority::TPri_Normal) == nullptr)
	{
		IsCompleted->Trigger();
	}
}
	
FStorageServerPackageStoreBackend::FAsyncInitRunnable::~FAsyncInitRunnable()
{
	IsCompleted->Wait();
	FPlatformProcess::ReturnSynchEventToPool(IsCompleted);
}

void FStorageServerPackageStoreBackend::FAsyncInitRunnable::WaitForCompletion() const
{
	IsCompleted->Wait();
}

uint32 FStorageServerPackageStoreBackend::FAsyncInitRunnable::Run()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerPackageStoreRequest);
	Connection.PackageStoreRequest([this](FPackageStoreEntryResource&& PackageStoreEntryResource)
		{
			FStoreEntry& LocalEntry = Owner.StoreEntriesMap.FindOrAdd(PackageStoreEntryResource.GetPackageId());
			LocalEntry.ImportedPackages = MoveTemp(PackageStoreEntryResource.ImportedPackageIds);
#if WITH_EDITOR
			LocalEntry.OptionalSegmentImportedPackageIds = MoveTemp(PackageStoreEntryResource.OptionalSegmentImportedPackageIds);
			LocalEntry.Flags = PackageStoreEntryResource.Flags;
#endif
			LocalEntry.ShaderMapHashes = MoveTemp(PackageStoreEntryResource.ShaderMapHashes);
		});

	IsCompleted->Trigger();
	return 0;
}

#endif
