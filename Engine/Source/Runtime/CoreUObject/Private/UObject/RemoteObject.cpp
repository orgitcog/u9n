// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/RemoteObject.h"
#include "UObject/RemoteExecutor.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Class.h"
#include "UObject/GCObject.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/GarbageCollection.h"
#include "UObject/RemoteObjectTransfer.h"
#include "UObject/RemoteObjectSerialization.h"
#include "UObject/RemoteObjectPrivate.h"
#include "UObject/ObjectHandlePrivate.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectMigrationContext.h"
#include "UObject/UObjectThreadContext.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "String/LexFromString.h"
#include <atomic>

DEFINE_LOG_CATEGORY(LogRemoteObject);

namespace UE::RemoteObject::Private
{

FRemoteServerId RemoteServerId;
std::atomic<uint64> RemoteObjectSerialNumber(1);
std::atomic<uint64> AssetObjectSerialNumber(1);
int32 UnsafeToMigrateObjects = 0; // This should go into TLS

class FRemoteObjectMaps
{
	mutable FTransactionallySafeCriticalSection ObjectMapCritical;
	TMap<FRemoteObjectId, UE::RemoteObject::Handle::FRemoteObjectStub*> RemoteObjects;
	TMap<FRemoteObjectId, FRemoteObjectPathName> AssetPaths;

public:

	UE::RemoteObject::Handle::FRemoteObjectStub* FindRemoteObjectStub(FRemoteObjectId Id)
	{
		using namespace UE::RemoteObject::Handle;

		UE::TScopeLock MapLock(ObjectMapCritical);
		if (FRemoteObjectStub** ExistingStub = RemoteObjects.Find(Id))
		{
			return *ExistingStub;
		}
		return nullptr;
	}

	UE::RemoteObject::Handle::FRemoteObjectStub* FindOrAddRemoteObjectStub(FRemoteObjectId Id, FRemoteServerId ResidentServerId = FRemoteServerId())
	{
		using namespace UE::RemoteObject::Handle;

		UE::TScopeLock MapLock(ObjectMapCritical);
		FRemoteObjectStub*& Stub = RemoteObjects.FindOrAdd(Id);
		if (!Stub)
		{
			Stub = new FRemoteObjectStub();
			Stub->Id = Id;
			Stub->ResidentServerId = ResidentServerId.IsValid() ? ResidentServerId : Id.GetServerId();

			// if we are creating the stub, then this object's owner is deduced from its ID
			// if the server ID is invalid then it's a local native object that was created before the local server had its ID assigned
			FRemoteServerId ObjectServerId = Id.GetServerId();
			Stub->OwningServerId = ObjectServerId.IsValid() ? ObjectServerId : GetGlobalServerId();
		}		
		return Stub;
	}

	UE::RemoteObject::Handle::FRemoteObjectStub* FindOrAddRemoteObjectStub(UObject* Object, FRemoteServerId DestinationServerId)
	{
		using namespace UE::RemoteObject::Handle;

		UE::TScopeLock MapLock(ObjectMapCritical);
		FRemoteObjectStub*& Stub = RemoteObjects.FindOrAdd(UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Object));
		if (!Stub)
		{
			Stub = new FRemoteObjectStub(Object);

			// if we are creating the stub, then this object's owner is deduced from its ID
			// if the server ID is invalid then it's a local native object that was created before the local server had its ID assigned
			FRemoteServerId ObjectServerId = Stub->Id.GetServerId();
			Stub->OwningServerId = ObjectServerId.IsValid() ? ObjectServerId : GetGlobalServerId();
		}

		FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);
		checkf(ObjectItem, TEXT("Attempting to get a serial number for an object that does not exist in the global UObject array (it's possible GUObjectArray is not initialized yet, ObjectIndex=%d)"), GUObjectArray.ObjectToIndex(Object));
		Stub->SerialNumber = ObjectItem->GetSerialNumber();
		Stub->Name = Object->GetFName();
		Stub->ResidentServerId = DestinationServerId;

		return Stub;
	}

	void ClearAllPhysicsIds()
	{
#if UE_WITH_REMOTE_OBJECT_HANDLE
		for (TPair<FRemoteObjectId, UE::RemoteObject::Handle::FRemoteObjectStub*>& Elem : RemoteObjects)
		{
			if (Elem.Value)
			{
				Elem.Value->PhysicsOverrideServerId = FRemoteServerId();
			}
		}
#endif
	}

	void StoreAssetPath(UObject* InObject)
	{
		FRemoteObjectId ObjectId(InObject);
		FRemoteObjectPathName& Path = AssetPaths.FindOrAdd(ObjectId);
		Path = FRemoteObjectPathName(InObject);
	}

	FRemoteObjectPathName* FindAssetPath(FRemoteObjectId ObjectId)
	{
		return AssetPaths.Find(ObjectId);
	}
	void UpdateAllPhysicsServerId(const TMap<uint32, uint32>& PhysicsServerMergingMap)
	{
#if UE_WITH_REMOTE_OBJECT_HANDLE
		for (TPair<FRemoteObjectId, UE::RemoteObject::Handle::FRemoteObjectStub*>& Elem : RemoteObjects)
		{
			if (Elem.Value)
			{
				const uint32 OriginalPhysicsId = Elem.Value->PhysicsOverrideServerId.GetIdNumber();
				if (const uint32* MergedPhysicsId = PhysicsServerMergingMap.Find(OriginalPhysicsId))
				{
					Elem.Value->PhysicsOverrideServerId = FRemoteServerId(*MergedPhysicsId);
				}
			}
		}
#endif
	}
};
FRemoteObjectMaps* ObjectMaps = nullptr;

void InitServerId()
{
	FString ServerId;
	if (!FParse::Value(FCommandLine::Get(), TEXT("MultiServerLocalId="), ServerId))
	{
		if (!FParse::Value(FCommandLine::Get(), TEXT("LocalPeerId="), ServerId))
		{
			RemoteServerId = FRemoteServerId(ERemoteServerIdConstants::Invalid);
		}
	}
	if (!ServerId.IsEmpty())
	{
		RemoteServerId = FRemoteServerId(ServerId);
		checkf(RemoteServerId.IsValid(), TEXT("Remote ServerId is not valid"));
	}

	UE_LOG(LogRemoteObject, Display, TEXT("Remote ServerId: %s"), *RemoteServerId.ToString());
}

void InitRemoteObjects()
{
	ObjectMaps = new FRemoteObjectMaps();

	InitServerId();

	UE::RemoteObject::Transfer::InitRemoteObjectTransfer();

	if (!UE::RemoteObject::Transfer::RemoteObjectTransferDelegate.IsBound())
	{
		UE::RemoteObject::Transfer::RemoteObjectTransferDelegate.BindStatic(&UE::RemoteObject::Serialization::Disk::SaveObjectToDisk);
	}
	if (!UE::RemoteObject::Transfer::RequestRemoteObjectDelegate.IsBound())
	{
		UE::RemoteObject::Transfer::RequestRemoteObjectDelegate.BindLambda(
			[](FRemoteWorkPriority RequestPriority, FRemoteObjectId ObjectId, FRemoteServerId LastKnownResidentServerId, FRemoteServerId DestinationServerId)
			{
				// Turns a request into an immediate load
				FUObjectMigrationContext MigrationContext {
					.ObjectId = ObjectId, .RemoteServerId = DestinationServerId, .OwnerServerId = LastKnownResidentServerId,
					.PhysicsServerId = LastKnownResidentServerId, .MigrationSide = EObjectMigrationSide::Receive
				};
				UE::RemoteObject::Serialization::Disk::LoadObjectFromDisk(MigrationContext);
			}
		);
	}
	if (!UE::RemoteObject::Transfer::StoreRemoteObjectDataDelegate.IsBound())
	{
		UE::RemoteObject::Transfer::StoreRemoteObjectDataDelegate.BindStatic(&UE::RemoteObject::Serialization::Disk::SaveObjectToDisk);
	}
	if (!UE::RemoteObject::Transfer::RestoreRemoteObjectDataDelegate.IsBound())
	{
		UE::RemoteObject::Transfer::RestoreRemoteObjectDataDelegate.BindStatic(&UE::RemoteObject::Serialization::Disk::LoadObjectFromDisk);
	}

	if (!UE::RemoteExecutor::FetchNextDeferredRPCDelegate.IsBound())
	{
		UE::RemoteExecutor::FetchNextDeferredRPCDelegate.BindStatic([]() { return TOptional<TTuple<FName, FRemoteWorkPriority, bool, TFunction<void(void)>>>(); });
	}
}

void RegisterRemoteObjectId(FRemoteObjectId ObjectId, FRemoteServerId ResidentServerId)
{
	ObjectMaps->FindOrAddRemoteObjectStub(ObjectId, ResidentServerId);
}

void RegisterSharedObject(UObject* Object)
{
	FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);
	ObjectItem->SetFlags(EInternalObjectFlags::RemoteReference);
}

void MarkAsRemote(UObject* Object, FRemoteServerId DestinationServerId)
{
	static_assert(sizeof(FObjectHandle) == sizeof(UObject*));
	checkf(!Object->IsTemplate(), TEXT("Attempted to Migrate Template Object '%s' which is considered an asset and never allowed to migrate"), *GetNameSafe(Object));

	FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);
	ObjectItem->SetFlags(EInternalObjectFlags::Remote);
	ObjectItem->ClearFlags(EInternalObjectFlags_RootFlags | EInternalObjectFlags::RemoteReference);
	ObjectMaps->FindOrAddRemoteObjectStub(Object, DestinationServerId);
}

void MarkAsLocal(UObject* Object)
{
	ensureMsgf(!Object->HasAnyFlags(EObjectFlags::RF_ClassDefaultObject | EObjectFlags::RF_ArchetypeObject), TEXT("We're about to set an ArchetypeObject %s as remote reference"), *GetNameSafe(Object));

	FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);
	ObjectItem->ClearFlags(EInternalObjectFlags::Remote);
	ObjectItem->SetFlags(EInternalObjectFlags::RemoteReference);
	ObjectMaps->FindOrAddRemoteObjectStub(Object, GetGlobalServerId());
}

void StoreAssetPath(UObject* Object)
{
	// Make sure the asset has a stub and that the stub knows the owner if this asset is the asset server (disk / content)
	ObjectMaps->FindOrAddRemoteObjectStub(Object, FRemoteServerId(ERemoteServerIdConstants::Asset));
	ObjectMaps->StoreAssetPath(Object);
}

FRemoteObjectPathName* FindAssetPath(FRemoteObjectId RemoteId)
{
	return ObjectMaps->FindAssetPath(RemoteId);
}

UE::RemoteObject::Handle::FRemoteObjectStub* FindRemoteObjectStub(FRemoteObjectId ObjectId)
{
	return ObjectMaps->FindRemoteObjectStub(ObjectId);
}

FName GetServerBaseNameForUniqueName(const UClass* Class)
{
	using namespace UE::RemoteObject;

	checkf(Class, TEXT("Unable to generate base name for a unique object name without the object's Class"));

	// Packages follow different naming rules than other UObjects and ATM we're not migrating packages so fall back to Class->GetFName()
	if (GetGlobalServerId().IsValid() && Class->GetFName() != NAME_Package)
	{
		return *FString::Printf(TEXT("%s_S%s"), *Class->GetFName().GetPlainNameString(), *GetGlobalServerId().ToString());
	}
	return Class->GetFName();
}

FUnsafeToMigrateScope::FUnsafeToMigrateScope()
{
	UnsafeToMigrateObjects++;
}
FUnsafeToMigrateScope::~FUnsafeToMigrateScope()
{
	UnsafeToMigrateObjects--;
	check(UnsafeToMigrateObjects >= 0);
}

bool IsSafeToMigrateObjects()
{
	// Not a thread safe test but atm we assume we're running single-threaded
	return !(GIsGarbageCollecting || UnsafeToMigrateObjects);
}

} // namespace UE::RemoteObject::Private

namespace UE::RemoteObject
{
	FRemoteServerId GetGlobalServerId()
	{
		return UE::RemoteObject::Private::RemoteServerId;
	}
}

namespace UE::RemoteObject::Handle
{

FRemoteObjectStub::FRemoteObjectStub(UObject* Object)
{
	using namespace UE::CoreUObject::Private;

	Id = FObjectHandleUtils::GetRemoteId(Object);
	if (UObject* Outer = FObjectHandleUtils::GetNonAccessTrackedOuterNoResolve(Object))
	{
		OuterId = FObjectHandleUtils::GetRemoteId(Outer);
	}
}

bool IsRemote(FRemoteObjectId ObjectId)
{
	using namespace UE::RemoteObject::Private;

	if (!ObjectId.IsValid())
	{
		return false;
	}

	if (UObject* Object = StaticFindObjectFastInternal(ObjectId))
	{
		return IsRemote(Object);
	}

	if (FindRemoteObjectStub(ObjectId))
	{
		return true;
	}

	const FRemoteServerId ServerId = ObjectId.GetServerId();
	// Invalid server Id means local native classes which are created before a server has a chance to have an id assigned
	return ServerId.IsValid() && ServerId != UE::RemoteObject::GetGlobalServerId();
}

bool IsRemote(const UObject* Object)
{
	const int32 InternalIndex = GUObjectArray.ObjectToIndex(Object);
	const bool bIsRemote = InternalIndex >= 0 && GUObjectArray.IndexToObject(InternalIndex)->HasAnyFlags(EInternalObjectFlags::Remote);
	return bIsRemote;
}

bool IsOwned(const UObject* Object)
{
	using namespace UE::CoreUObject::Private;
	return IsOwned(FObjectHandleUtils::GetRemoteId(Object));
}

bool IsOwned(FRemoteObjectId ObjectId)
{
	using namespace UE::CoreUObject::Private;
	using namespace UE::RemoteObject::Private;

	bool bResult = true;
#if UE_WITH_REMOTE_OBJECT_HANDLE
	UE::RemoteObject::Handle::FRemoteObjectStub* RemoteStub = FindRemoteObjectStub(ObjectId);
	if (RemoteStub)
	{
		bResult = (RemoteStub->OwningServerId == GetGlobalServerId() || RemoteStub->OwningServerId.IsAsset());
	}
	else
	{
		const FRemoteServerId ServerId = ObjectId.GetServerId();
		// Invalid server Id means local native objects which were created before the local server had a chance to have an id assigned
		bResult = (!ServerId.IsValid() || ServerId.IsAsset() || ServerId == UE::RemoteObject::GetGlobalServerId());
	}
#endif
	return bResult;
}

FRemoteServerId GetOwnerServerId(const UObject* Object)
{
	using namespace UE::CoreUObject::Private;
	using namespace UE::RemoteObject::Private;

	FRemoteServerId Result;
#if UE_WITH_REMOTE_OBJECT_HANDLE
	UE::RemoteObject::Handle::FRemoteObjectStub* RemoteStub = FindRemoteObjectStub(FObjectHandleUtils::GetRemoteId(Object));
	if (RemoteStub)
	{
		Result = RemoteStub->OwningServerId;
	}
	else
	{
		// if the object wasn't received or ever migrated, we own it locally
		Result = GetGlobalServerId();
	}
#endif
	return Result;
}

void ChangeOwnerServerId(const UObject* Object, FRemoteServerId NewOwnerServerId)
{
	using namespace UE::CoreUObject::Private;
	using namespace UE::RemoteObject::Private;
	
#if UE_WITH_REMOTE_OBJECT_HANDLE
	UE::RemoteObject::Handle::FRemoteObjectStub* RemoteStub = FindRemoteObjectStub(FObjectHandleUtils::GetRemoteId(Object));

	// The remote stub is always expected to be found for this object.
	if (ensureMsgf(RemoteStub, TEXT("Missing stub for %s (%s / 0x%016llx)"), *GetPathNameSafe(Object), *FRemoteObjectId(Object).ToString(), (int64)(PTRINT)Object))
	{
		RemoteStub->OwningServerId = NewOwnerServerId;
	}
#endif
}

FRemoteServerId GetPhysicsServerId(const UObject* Object)
{
	using namespace UE::CoreUObject::Private;
	using namespace UE::RemoteObject::Private;

	FRemoteServerId Result;
#if UE_WITH_REMOTE_OBJECT_HANDLE
	UE::RemoteObject::Handle::FRemoteObjectStub* RemoteStub = FindRemoteObjectStub(FObjectHandleUtils::GetRemoteId(Object));
	if (RemoteStub)
	{
		Result = RemoteStub->PhysicsOverrideServerId;
	}
	else
	{
		//If the object doesn't have stub, then the physics ID is invalid. It should not participate in push migration.
		Result = FRemoteServerId();
	}
#endif
	return Result;
}

void ChangePhysicsServerId(const UObject* Object, FRemoteServerId NewPhysicsServerId)
{
	using namespace UE::CoreUObject::Private;
	using namespace UE::RemoteObject::Private;

#if UE_WITH_REMOTE_OBJECT_HANDLE
	FRemoteObjectId ThisObject = FObjectHandleUtils::GetRemoteId(Object);
	UE::RemoteObject::Handle::FRemoteObjectStub* RemoteStub = ObjectMaps->FindOrAddRemoteObjectStub(FObjectHandleUtils::GetRemoteId(Object));
	if (RemoteStub)
	{
		RemoteStub->PhysicsOverrideServerId = NewPhysicsServerId;
	}
#endif
}

void ClearAllPhysicsServerId()
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	if (Private::ObjectMaps)
	{
		Private::ObjectMaps->ClearAllPhysicsIds();
	}
#endif
}

void UpdateAllPhysicsServerId(const TMap<uint32, uint32>& PhysicsServerMergingMap)
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	if (Private::ObjectMaps)
	{
		Private::ObjectMaps->UpdateAllPhysicsServerId(PhysicsServerMergingMap);
	}
#endif
}

UObject* ResolveObject(const FRemoteObjectStub* Stub, ERemoteReferenceType RefType /*= ERemoteReferenceType::Strong*/)
{
	using namespace UE::RemoteObject::Transfer;
	using namespace UE::RemoteObject::Private;

	// This is a slightly faster version of IsRemote(FRemoteObjectId) because we already know a Stub exists and we are going to re-use the Object pointer
	UObject* Object = StaticFindObjectFastInternal(Stub->Id);

	if (!Object && Stub->OwningServerId == FRemoteServerId(ERemoteServerIdConstants::Asset))
	{
		if (FRemoteObjectPathName* AssetPath = FindAssetPath(Stub->Id))
		{
			Object = AssetPath->Resolve();
		}
	}

	if (!IsSafeToMigrateObjects() && (Object || RefType == ERemoteReferenceType::Weak)) // Not a thread-safe test
	{
		// Begin/FinishDestroy overrides may attempt to access subobjects of objects that have been migrated in which case we
		// don't want to accidentally migrate them back mid-purge and if the Object memory is still valid (but has EInternalObjectFlags::Remote flag)
		// we can just return it and let the owner finish its cleanup.
		// In case of weak object ptrs it's relatively safe to just return null if the object doesn't exist on this server (see CanResolveObject)
		TouchResidentObject(Object);
		return Object;
	}

	bool bRemoteObject = !Object || IsRemote(Object);
	if (bRemoteObject)
	{
		checkf(!GIsGarbageCollecting, TEXT("Resolving remote objects while collecting garbage is not allowed (trying to resolve object %s (%s)"), *Stub->Id.ToString(), *Stub->Name.ToString());

		UObject* Outer = Object ? Object->GetOuter() : StaticFindObjectFastInternal(Stub->OuterId);

		MigrateObjectFromRemoteServer(Stub->Id, Stub->ResidentServerId, Outer);

		// if running transactionally, we will have aborted and not reached here

		// if running non-transactionally, object migrated immediately, so we just re-resolve
		Object = StaticFindObjectFastInternal(Stub->Id);
		bRemoteObject = !Object || IsRemote(Object);
		checkf(!bRemoteObject, TEXT("Failed to resolve remote object %s, either this code is not running in a transaction and should be, or the transaction failed to abort"), *Stub->Id.ToString());
	}

	return Object;
}

UObject* ResolveObject(UObject* Object, ERemoteReferenceType RefType /*= ERemoteReferenceType::Strong*/)
{
	using namespace UE::RemoteObject::Transfer;
	using namespace UE::RemoteObject::Private;

	// Begin/FinishDestroy overrides may attempt to access subobjects of objects that have been migrated in which case we
	// don't want to accidentally migrate them back mid-purge and if the Object memory is still valid (but has EInternalObjectFlags::Remote flag)
	// we can just return it and let the owner finish its cleanup.
	if (IsSafeToMigrateObjects()) // Note: this is not a thread-safe check
	{
		FRemoteObjectStub* Stub = ObjectMaps->FindRemoteObjectStub(UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Object));
		checkf(Stub, TEXT("Failed to find remote object stub for %s"), *GetPathNameSafe(Object));
		return ResolveObject(Stub, RefType);
	}

	TouchResidentObject(Object);
	return Object;
}

void TouchResidentObject(UObject* Object)
{
	UE::RemoteObject::Transfer::TouchResidentObject(Object);
}

bool CanResolveObject(FRemoteObjectId ObjectId)
{
	using namespace UE::RemoteObject::Private;

	// Note: this function needs to reflect the logic of ResolveObject(...) functions
	
	if (UObject* Object = StaticFindObjectFastInternal(ObjectId))
	{
		// Object memory is local and even if it's already been migrated we can resolve it
		return true;
	}

	if (FRemoteObjectStub* Stub = ObjectMaps->FindRemoteObjectStub(ObjectId))
	{
		// A stub exists so the object memory is not local but we can (attempt to) migrate it back if we're not garbage collecting
		// Note: GIsGarbageCollecting checks are not thread safe
		return IsSafeToMigrateObjects();
	}

	// ObjectId is local or represents an object that has never been migrated
	return false;
}

} // namespace UE::RemoteObject::Handle

namespace UE::CoreUObject::Private
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	void FObjectHandleUtils::ChangeRemoteId(UObjectBase* Object, FRemoteObjectId Id)
	{
		using namespace UE::RemoteObject::Private;
		UnhashObject(Object);
		FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);
		// ObjectItem may not exist when the UObject system hasn't been initialized yet but theoretically this function should only get called when
		// something attempts to re-construct a default subobject that already exists so ObjectItem should always be valid
		checkf(ObjectItem, TEXT("Attempting to change remote ID for an object that does not exist in the global UObject array (it's possible GUObjectArray is not initialized yet, ObjectIndex=%d)"), GUObjectArray.ObjectToIndex(Object));
		ObjectItem->RemoteId = Id;
		HashObject(Object);
	}

	FRemoteObjectId FRemoteObjectHandlePrivate::GetRemoteId() const
	{
		if ((PointerOrHandle & UPTRINT(1)))
		{
			return ToStub()->Id;
		}
		return FRemoteObjectId(reinterpret_cast<const UObjectBase*>(PointerOrHandle));
	}

	FRemoteObjectHandlePrivate FRemoteObjectHandlePrivate::ConvertToRemoteHandle(UObject* Object)
	{
		using namespace UE::RemoteObject::Handle;
		using namespace UE::RemoteObject::Private;		

		FRemoteObjectStub* Stub = ObjectMaps->FindRemoteObjectStub(UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Object));
		checkf(Stub, TEXT("Failed to find remote object stub for %s"), *GetPathNameSafe(Object));
		return FRemoteObjectHandlePrivate(Stub);
	}

	FRemoteObjectHandlePrivate FRemoteObjectHandlePrivate::FromIdNoResolve(FRemoteObjectId ObjectId)
	{
		using namespace UE::RemoteObject::Handle;
		using namespace UE::RemoteObject::Private;

		UObject* Obj = nullptr;
		if (ObjectId.IsValid())
		{
			Obj = StaticFindObjectFastInternal(ObjectId);
			if (Obj && !Obj->HasAnyInternalFlags(EInternalObjectFlags::Remote))
			{
				return FRemoteObjectHandlePrivate(Obj);
			}
			else if (FRemoteObjectStub* Stub = ObjectMaps->FindOrAddRemoteObjectStub(ObjectId))
			{
				return FRemoteObjectHandlePrivate(Stub);
			}
			check(false);
		}
		return FRemoteObjectHandlePrivate(Obj);
	}
#endif // UE_WITH_REMOTE_OBJECT_HANDLE

} // namespace UE::CoreUObject::Private

#ifndef UE_WITH_REMOTE_ASSET_ID
#define UE_WITH_REMOTE_ASSET_ID 1 // set this to 0 to disable remote asset IDs
#endif

FRemoteObjectId FRemoteObjectId::Generate(UObjectBase* InObject, EInternalObjectFlags InInitialFlags /*= EInternalObjectFlags::None*/)
{
	using namespace UE::RemoteObject::Private;
	using namespace UE::CoreUObject::Private;

	bool bIsAsset = false;
#if UE_WITH_REMOTE_ASSET_ID
	if (GIsInitialLoad || !UE::RemoteObject::GetGlobalServerId().IsValid() || !!(InInitialFlags & EInternalObjectFlags::Native) || !!(InObject->GetFlags() & RF_ArchetypeObject))
	{
		// Native objects (classes, CDOs etc) or objects loaded during initial load are always in memory and are considered assets any server can find locally
		// Note that this first condition can not touch too much of UObject API because we might literarlly be constructing the first StaticClass() etc.
		// Hopefull GIsInitialLoad and the Native flag will filter most of the initially created objects and most of the native objects will be constructed before we ever hit the 'else' block
		bIsAsset = true;
	}
	else
	{
		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		if (ThreadContext.AsyncPackageLoader || (ThreadContext.GetSerializeContext() && ThreadContext.GetSerializeContext()->GetBeginLoadCount() > 0) || !!(InObject->GetFlags() & RF_WasLoaded))
		{
			// If we're constructing this object when loading content then this object is an asset if:
			// its class or any of its outers' class is NOT marked as MigratingAsset
			// OR it's an archetype or subobject of an archetype 
			// OR it's a subobject of a UStruct (class)
			bool bIsMigratingAsset = false;
			bool bIsClassOrArchetypeSubobject = false;

			for (UObjectBase* OuterIt = InObject; OuterIt; OuterIt = FObjectHandleUtils::GetNonAccessTrackedOuterNoResolve(OuterIt))
			{
				UClass* Class = OuterIt->GetClass();
				if (!!(OuterIt->GetFlags() & RF_ArchetypeObject) || Class->IsChildOf(UStruct::StaticClass()))
				{
					bIsClassOrArchetypeSubobject = true;
					break;
				}
				if (Class->HasAnyClassFlags(CLASS_MigratingAsset))
				{
					bIsMigratingAsset = true;
				}					
			}

			bIsAsset = bIsClassOrArchetypeSubobject || !bIsMigratingAsset;
		}
	}
#endif // UE_WITH_REMOTE_ASSET_ID

	FRemoteObjectId Result;
	if (!bIsAsset)
	{
		Result = FRemoteObjectId(RemoteServerId, RemoteObjectSerialNumber.fetch_add(1));
	}
	else
	{
		Result = FRemoteObjectId(FRemoteServerId(ERemoteServerIdConstants::Asset), AssetObjectSerialNumber.fetch_add(1));
	}

	return Result;
}

FString FRemoteObjectId::ToString() const
{
	return FString::Printf(TEXT("%s-%llu"), *GetServerId().ToString(), SerialNumber);
}

FRemoteObjectId::FRemoteObjectId(const UObjectBase* Object)
	: FRemoteObjectId(UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Object))
{
}

FArchive& operator<<(FArchive& Ar, FRemoteObjectId& Id)
{
	Ar << Id.Id;
	return Ar;
}

FRemoteServerId::FRemoteServerId(const FString& InText)
{
	if (InText == TEXT("Asset"))
	{
		Id = (uint32)ERemoteServerIdConstants::Asset;
	}
	else if (InText == TEXT("Database"))
	{
		Id = (uint32)ERemoteServerIdConstants::Database;
	}
	else
	{
		uint32 ServerIdNumber = (uint32)ERemoteServerIdConstants::Invalid;
		LexFromString(ServerIdNumber, InText);
		if (ensureMsgf(ServerIdNumber <= (uint32)ERemoteServerIdConstants::Max, TEXT("Parsed Remote Server Id value %u that is bigger than allowed max %u"), ServerIdNumber, (uint32)ERemoteServerIdConstants::Max))
		{
			Id = ServerIdNumber;
		}
		else
		{
			UE_LOG(LogRemoteObject, Warning, TEXT("Clamping ServerId number %u to the maximum allowed %u"), ServerIdNumber, (uint32)ERemoteServerIdConstants::Max);
			Id = (uint32)ERemoteServerIdConstants::Max;
		}		
	}
}

FString FRemoteServerId::ToString() const
{
	switch (Id)
	{
		case (uint32)ERemoteServerIdConstants::Asset:
			return TEXT("Asset");

		case (uint32)ERemoteServerIdConstants::Database:
			return TEXT("Database");

		default:
			return FString::FromInt(Id);
	}
}

FArchive& operator<<(FArchive& Ar, FRemoteServerId& Id)
{
	Ar << Id.Id;
	return Ar;
}

// Debugging functionality to help us find these objects in debug builds
#if !UE_BUILD_SHIPPING && UE_WITH_REMOTE_OBJECT_HANDLE
/**
 * Put this in a Debug Watch Window on a specific UObject.  You may have to forcibly cast the UObject to UObjectBase*
 * e.g. DebugFindRemoteObjectStub((UObjectBase*)Header.Class.DebugPtr)
 */
COREUOBJECT_API UE::RemoteObject::Handle::FRemoteObjectStub* DebugFindRemoteObjectStub(const UObjectBase* Object)
{
	if (!Object)
	{
		return nullptr;
	}

	uintptr_t Pointer = reinterpret_cast<uintptr_t>(Object);
	if (Pointer & 0x1)
	{
		return reinterpret_cast<UE::RemoteObject::Handle::FRemoteObjectStub*>(Pointer & ~UPTRINT(1));
	}

	FRemoteObjectId ObjId { Object };
	return UE::RemoteObject::Private::ObjectMaps->FindRemoteObjectStub(ObjId);
}

/**
 * Attempt to find a UObject in the currently debugged process by its FRemoteObjectId constituents.
 * Once you know a FRemoteObjectId, take its ServerId and SerialNumber and pass them into Debug Watch Window as arguments (in that order)
 * e.g. DebugFindObjectLocallyFromRemoteId( 2, 1234 )
 */
COREUOBJECT_API UObject* DebugFindObjectLocallyFromRemoteId(uint16 ServerId, uint64 SerialNumber)
{
	return StaticFindObjectFastInternal(FRemoteObjectId(FRemoteServerId{ static_cast<uint32>(ServerId) }, SerialNumber));
}

/**
 * Attempt to find a UObject in the currently debugged process by its FRemoteObjectId's Full uint64 Id
 * Once you find a FRemoteObjectId, copy its Id and pass them into Debug Watch Window as the argument
 * e.g. DebugFindObjectLocallyFromRemoteId( 1234567890 )
 */
COREUOBJECT_API UObject* DebugFindObjectLocallyFromRemoteId(uint64 FullId)
{
	constexpr uint64 SerialBitMask = (1ull << 54ull) - 1ull;
	FRemoteObjectId RemoteId(FRemoteServerId{ static_cast<uint32>((FullId >> 54ull) & 1023ull) }, FullId & SerialBitMask);
	ensure(RemoteId.GetIdNumber() == FullId);

	return StaticFindObjectFastInternal(RemoteId);
}
#endif // !UE_BUILD_SHIPPING && UE_WITH_REMOTE_OBJECT_HANDLE