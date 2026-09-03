// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformCrt.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/SecureHash.h"
#include "PackageId.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FArchive;
class FCbObjectView;
class FCbWriter;
class FStructuredArchiveSlot;

/**
 * Package store entry status.
 */
enum class EPackageStoreEntryStatus
{
	None,
	Missing,
	NotInstalled,
	Pending,
	Ok,
};

/**
 * Package store entry.
 */ 
struct FPackageStoreEntry
{
	TArrayView<const FPackageId> ImportedPackageIds;
	TArrayView<const FSHAHash> ShaderMapHashes;
#if WITH_EDITOR
	TArrayView<const FPackageId> OptionalSegmentImportedPackageIds;
	bool bHasOptionalSegment = false;
	// This field is used to indicate that the package should load the optional chunk instead of the regular chunk. This is
	// needed by the StorageServerPackgeStore (ie ZenStore) when loading "AutoOptional" assets, but the FilePackageStore 
	// (ie .ucas files) does _not_ set this. This is because:
	//    * AutoOptional is used to "silently" replace the normal asset with an asset that still has editor-only data in it
	//    * When .ucas files are made, they create a .o.ucas file, which is mounted with a higher priority then the .ucas file
	//    * When FilePackageStore loads, it will automatically read the editor-only version of the asset, _as if it was the normal asset_
	//    * However, ZenStore has no ".o.ucas" file to silently read from instead of the regular file, so it must, at runtime,
	//      request the editor version of the asset (the .o.uasset chunk in the store).
	// This field is how we communicate out to the AsyncLoadingThread code that it should request the optional chunk instead of the
	// regular chunk for a given asset. It is basically making a runtime decision that is handled offline by ucas files.
	// Note that bHasOptionalSegment is always false for AutoOptional because of the "silently read optional as regular".
	// So to sum up for the two main PackageStores:
	//   ZenStore:
	//     Manual Optional
	//	     bHasOptionalSegment = true
	//	     bReplaceChunkWithOptional = false
	//	   AutoOptional
	//	     bHasOptionalSegment = false
	//	     bReplaceChunkWithOptional = true
	//	 FileStore
	//	   Manual Optional(same as Zen)
	//	     bHasOptionalSegment = true
	//	     bReplaceChunkWithOptional = false
	//	   AutoOptional (_not_ same)
	//	     bHasOptionalSegment = false
	//	     bReplaceChunkWithOptional = false
	bool bReplaceChunkWithOptional = false;
#endif
};

/**
 * Package store entry flags. These flags are persisted in the oplog as integers so
 * do not change their values.
 */
enum class EPackageStoreEntryFlags : uint32
{
	None				= 0,
	HasPackageData		= 0x00000001,
	AutoOptional		= 0x00000002,
	OptionalSegment		= 0x00000004,
	HasCookError		= 0x00000008,
	LoadUncooked		= 0x00000010, // This package must be loaded uncooked, when possibe, when loading from IoStore / ZenStore (i.e. HybridCookedEditor)
};
ENUM_CLASS_FLAGS(EPackageStoreEntryFlags);

/**
 * Package store entry resource.
 *
 * This is a non-optimized serializable version
 * of a package store entry. Used when cooking
 * and when running cook-on-the-fly.
 */
struct FPackageStoreEntryResource
{
	/** The package store entry flags. */
	EPackageStoreEntryFlags Flags = EPackageStoreEntryFlags::None;
	/** The package name. */
	FName PackageName;
	FPackageId PackageId;
	/** Imported package IDs. */
	TArray<FPackageId> ImportedPackageIds;
	/** Referenced shader map hashes. */
	TArray<FSHAHash> ShaderMapHashes;
	/** Editor data imported package IDs. */
	TArray<FPackageId> OptionalSegmentImportedPackageIds;
	/** Soft package references. */
	TArray<FPackageId> SoftPackageReferences;

	/** Returns the package ID. */
	FPackageId GetPackageId() const
	{
		return PackageId;
	}

	/** Returns whether this package was saved as auto optional */
	bool IsAutoOptional() const
	{
		return EnumHasAnyFlags(Flags, EPackageStoreEntryFlags::AutoOptional);
	}

	bool HasOptionalSegment() const
	{
		return EnumHasAnyFlags(Flags, EPackageStoreEntryFlags::OptionalSegment);
	}

	bool HasPackageData() const
	{
		return EnumHasAnyFlags(Flags, EPackageStoreEntryFlags::HasPackageData);
	}

	bool HasCookError() const
	{
		return EnumHasAnyFlags(Flags, EPackageStoreEntryFlags::HasCookError);
	}

	CORE_API friend FArchive& operator<<(FArchive& Ar, FPackageStoreEntryResource& PackageStoreEntry);
	
	CORE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FPackageStoreEntryResource& PackageStoreEntry);
	
	CORE_API static FPackageStoreEntryResource FromCbObject(FCbObjectView Obj);

	/**
	 * Creates a PackageStoreEntryResource that records a packagename and will be in an op with possible metadata
	 * stored in attachments, but for which the op has no packagedata. This is used for tracking build dependencies
	 * on packages even if those packages fail to cook due to error or editoronly.
	 */
	CORE_API static FPackageStoreEntryResource CreateEmptyPackage(FName PackageName, bool bHasCookError);
};

class FPackageStoreBackendContext
{
public:
	/* Event broadcasted when pending entries are completed and added to the package store */
	DECLARE_EVENT(FPackageStoreBackendContext, FPendingEntriesAddedEvent);
	FPendingEntriesAddedEvent PendingEntriesAdded;
};

/**
 * Package store backend interface.
 */
class IPackageStoreBackend
{
public:
	/* Destructor. */
	virtual ~IPackageStoreBackend() { }

	/** Called when the backend is mounted */
	virtual void OnMounted(TSharedRef<const FPackageStoreBackendContext> Context) = 0;

	/** Called when the loader enters a package store read scope. */
	virtual void BeginRead() = 0;

	/** Called when the loader exits a package store read scope. */
	virtual void EndRead() = 0;

	/* Returns the package store entry data with export info and imported packages for the specified package ID. */
	virtual EPackageStoreEntryStatus GetPackageStoreEntry(FPackageId PackageId, FName PackageName,
		FPackageStoreEntry& OutPackageStoreEntry) = 0;

	/* Returns the redirected package ID and source package name for the specified package ID if it's being redirected. */
	virtual bool GetPackageRedirectInfo(FPackageId PackageId, FName& OutSourcePackageName, FPackageId& OutRedirectedToPackageId) = 0;

	/* Returns all soft referenced package IDs for the specified package ID. */
	virtual TConstArrayView<uint32> GetSoftReferences(FPackageId PackageId, TConstArrayView<FPackageId>& OutPackageIds)
	{
		return TConstArrayView<uint32>();
	}
};

/**
 * Stores information about available packages that can be loaded.
 */
class FPackageStore
{
public:
	CORE_API static FPackageStore& Get();

	/* Mount a package store backend. */
	CORE_API void Mount(TSharedRef<IPackageStoreBackend> Backend, int32 Priority = 0);

	/* Returns the package store entry data with export info and imported packages for the specified package ID. */
	CORE_API EPackageStoreEntryStatus GetPackageStoreEntry(FPackageId PackageId, FName PackageName, 
		FPackageStoreEntry& OutPackageStoreEntry);

	/* Returns the redirected package ID and source package name for the specified package ID if it's being redirected. */
	CORE_API bool GetPackageRedirectInfo(FPackageId PackageId, FName& OutSourcePackageName, FPackageId& OutRedirectedToPackageId);

	/* Returns all soft referenced package IDs for the specified package ID. */
	CORE_API TConstArrayView<uint32> GetSoftReferences(FPackageId PackageId, TConstArrayView<FPackageId>& OutPackageIds);

	CORE_API FPackageStoreBackendContext::FPendingEntriesAddedEvent& OnPendingEntriesAdded();

	CORE_API bool HasAnyBackendsMounted() const;

private:
	FPackageStore();

	friend class FPackageStoreReadScope;

	TSharedRef<FPackageStoreBackendContext> BackendContext;
	
	using FBackendAndPriority = TTuple<int32, TSharedRef<IPackageStoreBackend>>;
	TArray<FBackendAndPriority> Backends;

	static thread_local int32 ThreadReadCount;
};

class FPackageStoreReadScope
{
public:
	CORE_API FPackageStoreReadScope(FPackageStore& InPackageStore);
	CORE_API ~FPackageStoreReadScope();

private:
	FPackageStore& PackageStore;
};
