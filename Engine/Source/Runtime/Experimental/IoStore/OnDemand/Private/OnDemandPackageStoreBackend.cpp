// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandPackageStoreBackend.h"

#include "Algo/Accumulate.h"
#include "Algo/Find.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "IO/IoContainerHeader.h"
#include "IO/IoStoreOnDemand.h"
#include "IO/PackageId.h"
#include "Internationalization/PackageLocalizationManager.h"
#include "OnDemandIoStore.h"

namespace UE::IoStore
{

///////////////////////////////////////////////////////////////////////////////
class FOnDemandPackageStoreBackend final
	: public IOnDemandPackageStoreBackend
{
	using FSoftPackageReferenceMap = TMap<FPackageId, const FFilePackageStoreEntrySoftReferences*>;

	struct FContainer
	{
		FContainer(FString&& ContainerName, FSharedContainerHeader ContainerHeader)
			: Name(MoveTemp(ContainerName))
			, Header(MoveTemp(ContainerHeader))
		{ }

		FString						Name;
		FSharedContainerHeader		Header;
		FSoftPackageReferenceMap	SoftRefs;
	};

	struct FOnDemandPackageStoreEntry
	{
		const FFilePackageStoreEntry* FilePackageStoreEntry = nullptr;
		bool bIsInstalled = false; // uninstalled packages are not 'missing' and not 'pending'
	};

	using FSharedBackendContext = TSharedPtr<const FPackageStoreBackendContext>;
	using FEntryMap				= TMap<FPackageId, FOnDemandPackageStoreEntry>;
	using FRedirect				= TTuple<FName, FPackageId>;
	using FLocalizedMap			= TMap<FPackageId, FName>;
	using FRedirectMap			= TMap<FPackageId, FRedirect>;

public:
						FOnDemandPackageStoreBackend(TWeakPtr<FOnDemandIoStore> OnDemandIoStore);
						virtual ~FOnDemandPackageStoreBackend ();

	virtual void		NeedsUpdate(EOnDemandPackageStoreUpdateMode Mode) override;

	virtual void		OnMounted(TSharedRef<const FPackageStoreBackendContext> Context) override;
	virtual void		BeginRead() override;
	virtual void		EndRead() override;

	virtual bool		GetPackageRedirectInfo(
							FPackageId PackageId,
							FName& OutSourcePackageName,
							FPackageId& OutRedirectedToPackageId) override;

	virtual EPackageStoreEntryStatus GetPackageStoreEntry(
							FPackageId PackageId,
							FName PackageName,
							FPackageStoreEntry& OutPackageStoreEntry) override;

	virtual TConstArrayView<uint32> GetSoftReferences(FPackageId PackageId, TConstArrayView<FPackageId>& OutPackageIds) override;

private:
	void				UpdateLookupTables(const TArray<FSharedOnDemandContainer>& AllContainers, const TArray<TBitArray<>>& ReferencedChunkEntryIndices);
	void				UpdateReferencedPackages(const TArray<FSharedOnDemandContainer>& AllContainers, const TArray<TBitArray<>>& ReferencedChunkEntryIndices);

	TWeakPtr<FOnDemandIoStore>			OnDemandIoStore;
	TArray<FContainer>					Containers;
	FEntryMap							EntryMap;
	FLocalizedMap						LocalizedMap;
	FRedirectMap						RedirectMap;
	UE::FMutex							Mutex;
	std::atomic<EOnDemandPackageStoreUpdateMode> NeedsUpdateMode{ EOnDemandPackageStoreUpdateMode::Full };
};

///////////////////////////////////////////////////////////////////////////////
FOnDemandPackageStoreBackend::FOnDemandPackageStoreBackend(TWeakPtr<FOnDemandIoStore> OnDemandIoStore)
	: OnDemandIoStore(MoveTemp(OnDemandIoStore))
{
}

FOnDemandPackageStoreBackend::~FOnDemandPackageStoreBackend()
{
}

void FOnDemandPackageStoreBackend::NeedsUpdate(EOnDemandPackageStoreUpdateMode Mode)
{
	if (Mode == EOnDemandPackageStoreUpdateMode::Full)
	{
		NeedsUpdateMode.store(EOnDemandPackageStoreUpdateMode::Full);
	}
	else if (Mode == EOnDemandPackageStoreUpdateMode::ReferencedPackages)
	{
		EOnDemandPackageStoreUpdateMode Expected = EOnDemandPackageStoreUpdateMode::None;
		NeedsUpdateMode.compare_exchange_strong(Expected, EOnDemandPackageStoreUpdateMode::ReferencedPackages);
	}
}

void FOnDemandPackageStoreBackend::OnMounted(TSharedRef<const FPackageStoreBackendContext> Context)
{
}

void FOnDemandPackageStoreBackend::BeginRead()
{
	EOnDemandPackageStoreUpdateMode LocalNeedsUpdate = NeedsUpdateMode.exchange(EOnDemandPackageStoreUpdateMode::None);

	if (LocalNeedsUpdate == EOnDemandPackageStoreUpdateMode::None)
	{
		Mutex.Lock();
		return;
	}

	TArray<FSharedOnDemandContainer> AllContainers;
	TArray<TBitArray<>> ReferencedChunkEntryIndices;

	if (TSharedPtr<FOnDemandIoStore> PinOnDemandIoStore = OnDemandIoStore.Pin())
	{
		const bool bPackageStore = true;
		PinOnDemandIoStore->GetReferencedContent(AllContainers, ReferencedChunkEntryIndices, bPackageStore);
	}

	Mutex.Lock();

	if (LocalNeedsUpdate == EOnDemandPackageStoreUpdateMode::Full)
	{
		UpdateLookupTables(AllContainers, ReferencedChunkEntryIndices);
	}
	else if (LocalNeedsUpdate == EOnDemandPackageStoreUpdateMode::ReferencedPackages)
	{
		UpdateReferencedPackages(AllContainers, ReferencedChunkEntryIndices);
	}
}

void FOnDemandPackageStoreBackend::EndRead()
{
	Mutex.Unlock();
}

EPackageStoreEntryStatus FOnDemandPackageStoreBackend::GetPackageStoreEntry(
	FPackageId PackageId,
	FName PackageName,
	FPackageStoreEntry& OutPackageStoreEntry)
{
	if (const FOnDemandPackageStoreEntry* Entry = EntryMap.Find(PackageId))
	{
		const FFilePackageStoreEntry* FilePackageStoreEntry = Entry->FilePackageStoreEntry;
		OutPackageStoreEntry.ImportedPackageIds =
			MakeArrayView(FilePackageStoreEntry->ImportedPackages.Data(), FilePackageStoreEntry->ImportedPackages.Num());
		OutPackageStoreEntry.ShaderMapHashes =
			MakeArrayView(FilePackageStoreEntry->ShaderMapHashes.Data(), FilePackageStoreEntry->ShaderMapHashes.Num());

		if (Entry->bIsInstalled)
		{
			return EPackageStoreEntryStatus::Ok;
		}

		return EPackageStoreEntryStatus::NotInstalled;
	}

	return EPackageStoreEntryStatus::Missing;
}

bool FOnDemandPackageStoreBackend::GetPackageRedirectInfo(
	FPackageId PackageId,
	FName& OutSourcePackageName,
	FPackageId& OutRedirectedToPackageId)
{
	if (const FRedirect* Redirect = RedirectMap.Find(PackageId))
	{
		OutSourcePackageName		= Redirect->Key;
		OutRedirectedToPackageId	= Redirect->Value;
		return true;
	}
	
	if (const FName* SourcePkgName = LocalizedMap.Find(PackageId))
	{
		const FName LocalizedPkgName = FPackageLocalizationManager::Get().FindLocalizedPackageName(*SourcePkgName);
		if (LocalizedPkgName.IsNone() == false)
		{
			const FPackageId LocalizedPkgId = FPackageId::FromName(LocalizedPkgName);
			if (EntryMap.Find(LocalizedPkgId))
			{
				OutSourcePackageName		= *SourcePkgName;
				OutRedirectedToPackageId	= LocalizedPkgId;
				return true;
			}
		}
	}

	return false;
}

TConstArrayView<uint32> FOnDemandPackageStoreBackend::GetSoftReferences(FPackageId PackageId, TConstArrayView<FPackageId>& OutPackageIds) 
{
	for (const FContainer& Container : Containers)
	{
		if (const FFilePackageStoreEntrySoftReferences* SoftRefs = Container.SoftRefs.FindRef(PackageId))
		{
			OutPackageIds = Container.Header->SoftPackageReferences.PackageIds;
			return TConstArrayView<uint32>(SoftRefs->Indices.Data(), SoftRefs->Indices.Num());
		}
	}
	return TConstArrayView<uint32>();
}

void FOnDemandPackageStoreBackend::UpdateLookupTables(const TArray<FSharedOnDemandContainer>& AllContainers, const TArray<TBitArray<>>& ReferencedChunkEntryIndices)
{
	check(Mutex.IsLocked());

	int32 PackageCount = 0;
	for (const FSharedOnDemandContainer& C : AllContainers)
	{
		PackageCount += C->Header->PackageIds.Num();
	}

	LocalizedMap.Empty();
	RedirectMap.Empty();
	Containers.Empty(AllContainers.Num());
	EntryMap.Empty(PackageCount);

	for (int32 ContainerIdx = 0; const FSharedOnDemandContainer& SharedContainer : AllContainers)
	{
		FContainer& Container = Containers.Emplace_GetRef(CopyTemp(SharedContainer->UniqueName()), SharedContainer->Header);

		const FIoContainerHeader& Hdr = *Container.Header;
		TConstArrayView<FFilePackageStoreEntry> Entries(
			reinterpret_cast<const FFilePackageStoreEntry*>(Hdr.StoreEntries.GetData()),
			Hdr.PackageIds.Num());

		TConstArrayView<FFilePackageStoreEntrySoftReferences> AllSoftReferences;
		if (Hdr.SoftPackageReferences.bContainsSoftPackageReferences)
		{
			AllSoftReferences = MakeArrayView<const FFilePackageStoreEntrySoftReferences>(
				reinterpret_cast<const FFilePackageStoreEntrySoftReferences*>(Hdr.SoftPackageReferences.PackageIndices.GetData()),
				Hdr.PackageIds.Num());
			Container.SoftRefs.Reserve(Hdr.PackageIds.Num());
		}

		for (int32 EntryIdx = 0; const FFilePackageStoreEntry& Entry : Entries)
		{
			const FPackageId PkgId = Hdr.PackageIds[EntryIdx];
			const FIoChunkId PkgChunkId = CreatePackageDataChunkId(PkgId);
			const int32 PkgChunkIdx = SharedContainer->FindChunkEntryIndex(PkgChunkId);
			check(PkgChunkIdx != INDEX_NONE);
			const bool IsReferenced = ReferencedChunkEntryIndices[ContainerIdx][PkgChunkIdx];

			EntryMap.Add(PkgId, FOnDemandPackageStoreEntry{ .FilePackageStoreEntry = &Entry, .bIsInstalled = IsReferenced });
			if (AllSoftReferences.IsEmpty() == false)
			{
				const FFilePackageStoreEntrySoftReferences& SoftRefs = AllSoftReferences[EntryIdx];
				if (SoftRefs.Indices.Num() > 0)
				{
					Container.SoftRefs.Add(PkgId, &SoftRefs);
				}
			}
			++EntryIdx;
		}
		Container.SoftRefs.Shrink();

		for (const FIoContainerHeaderLocalizedPackage& Localized : Hdr.LocalizedPackages)
		{
			FName& SourcePackageName = LocalizedMap.FindOrAdd(Localized.SourcePackageId);
			if (SourcePackageName.IsNone())
			{
				FDisplayNameEntryId NameEntry = Hdr.RedirectsNameMap[Localized.SourcePackageName.GetIndex()];
				SourcePackageName = NameEntry.ToName(Localized.SourcePackageName.GetNumber());
			}
		}

		for (const FIoContainerHeaderPackageRedirect& Redirect : Hdr.PackageRedirects)
		{
			FRedirect& RedirectEntry = RedirectMap.FindOrAdd(Redirect.SourcePackageId);
			FName& SourcePackageName = RedirectEntry.Key;
			if (SourcePackageName.IsNone())
			{
				FDisplayNameEntryId NameEntry = Hdr.RedirectsNameMap[Redirect.SourcePackageName.GetIndex()];
				SourcePackageName = NameEntry.ToName(Redirect.SourcePackageName.GetNumber());
				RedirectEntry.Value = Redirect.TargetPackageId;
			}
		}

		++ContainerIdx;
	}

	EntryMap.Shrink();
	LocalizedMap.Shrink();
	RedirectMap.Shrink();
}

void FOnDemandPackageStoreBackend::UpdateReferencedPackages(const TArray<FSharedOnDemandContainer>& AllContainers, const TArray<TBitArray<>>& ReferencedChunkEntryIndices)
{
	check(Mutex.IsLocked());

	for (int32 ContainerIdx = 0; const FSharedOnDemandContainer& SharedContainer : AllContainers)
	{
		const FIoContainerHeader& Hdr = *SharedContainer->Header;

		for (const FPackageId PkgId : Hdr.PackageIds)
		{
			// Entry may not be present if this function is racing with a container unmount
			// It will be removed on the next full update after the unmount completes
			if (FOnDemandPackageStoreEntry* OnDemandEntry = EntryMap.Find(PkgId))
			{
				const FIoChunkId PkgChunkId = CreatePackageDataChunkId(PkgId);
				const int32 PkgChunkIdx = SharedContainer->FindChunkEntryIndex(PkgChunkId);
				check(PkgChunkIdx != INDEX_NONE);
				const bool IsReferenced = ReferencedChunkEntryIndices[ContainerIdx][PkgChunkIdx];

				OnDemandEntry->bIsInstalled = IsReferenced;
			}
		}

		++ContainerIdx;
	}
}

///////////////////////////////////////////////////////////////////////////////
TSharedPtr<IOnDemandPackageStoreBackend> MakeOnDemandPackageStoreBackend(TWeakPtr<FOnDemandIoStore> OnDemandIoStore)
{
	return MakeShared<FOnDemandPackageStoreBackend>(MoveTemp(OnDemandIoStore));
}

} // namespace UE
