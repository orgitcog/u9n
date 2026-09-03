// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "UObject/RemoteObjectPathName.h"
#include "Async/Async.h"
#include "UObject/RemoteExecutor.h"

namespace UE::Net
{
	struct FRemoteObjectReferenceNetSerializer;
}

struct FUObjectMigrationContext;

/**
* Structure that holds serialized remote object data chunk (< 64KB of data) (noexport type)
*/
struct FRemoteObjectBytes
{
	TArray<uint8> Bytes;
};

/**
* Structure that holds remote object memory (noexport type)
*/
struct FRemoteObjectData
{
	FRemoteObjectTables Tables;
	TArray<FPackedRemoteObjectPathName> PathNames;
	TArray<FRemoteObjectBytes> Bytes;

	inline int32 GetNumBytes() const
	{
		int32 Num = 0;
		for (const FRemoteObjectBytes& Chunk : Bytes)
		{
			Num += Chunk.Bytes.Num();
		}
		return Num;
	}
};

namespace UE::RemoteObject::Transfer
{
	/** Information for performing a migration (send) an object to a remote server */
	struct FMigrateSendParams
	{
		/** The migration context (meta data) of the send request */
		FUObjectMigrationContext& MigrationContext;

		/** The serialized data of the object being sent */
		FRemoteObjectData ObjectData;
	};

	/**
	* (Advanced API) Adds the remote object to the current transaction's requested object list, causing
	* the object to be migrated to this server. Does not abort the transaction if the object isn't yet local
	*/
	COREUOBJECT_API void PrefetchRemoteObject(FRemoteObjectId ObjectId, UObject* DestinationOuter);

	/**
	* Called when remote object data has been received from a remote server
	* @param ObjectId Remote object id
	* @param RemoteServerId Server that sent object data
	* @param Data Remote object data. Data ownership is transferred to the remote object queue
	*/
	COREUOBJECT_API void OnObjectDataReceived(FRemoteServerId OwnerServerId, FRemoteServerId PhysicsId, FRemoteObjectId ObjectId, FRemoteServerId RemoteServerId, FRemoteObjectData& Data);

	/**
	* Called when a remote object request was denied by a remote server
	* @param ObjectId Remote object id
	* @param RemoteServerId Server that owns the object data
	*/
	COREUOBJECT_API void OnObjectDataDenied(FRemoteObjectId ObjectId, FRemoteServerId RemoteServerId);

	/**
	* Migrates and transfers ownership of an object to a remote server
	* @param Object Local object to migrate
	* @param DestinationServerId Id of a remote server to receive the object and assume ownership
	*/
	COREUOBJECT_API void TransferObjectOwnershipToRemoteServer(UObject* Object, FRemoteServerId DestinationServerId);

	/**
	* Migrates an object to a remote server without changing ownership
	* @param ObjectId Remote object id
	* @param DestinationServerId Id of a remote server to receive the object
	*/
	COREUOBJECT_API void MigrateObjectToRemoteServer(FRemoteObjectId ObjectId, FRemoteServerId DestinationServerId);
	COREUOBJECT_API void MigrateObjectToRemoteServerWithExplicitPriority(FRemoteWorkPriority RequestPriority, FRemoteObjectId Id, FRemoteServerId DestinationServerId);
	
	/**
	* Migrates an object from a remote server (temp function)
	* @param ObjectId Remote object id
	* @param CurrentOwnerServerId Id of a remote server that currently owns the object
	* @param DestinationOuter Optional outer the migrated object should be reparented to
	* @return Migrated object
	*/
	COREUOBJECT_API void MigrateObjectFromRemoteServer(FRemoteObjectId ObjectId, FRemoteServerId CurrentOwnerServerId, UObject* DestinationOuter = nullptr);

	/**
	* Reports code that touches a resident object
	*/
	COREUOBJECT_API void TouchResidentObject(UObject* Object);

	/**
	* Utility that converts FRemoteObjectId to TObjectPtr of a given type. Will not resolve the object if it's not local.
	*/
	UE_DEPRECATED(5.6, "RemoteObjectIdToObjectPtr is deprecated use FObjectPtr(FRemoteObjectId) instead.")
	FORCEINLINE FObjectPtr RemoteObjectIdToObjectPtr(FRemoteObjectId RemoteId)
	{
#if UE_WITH_REMOTE_OBJECT_HANDLE
		return FObjectPtr(UE::CoreUObject::Private::FRemoteObjectHandlePrivate::FromIdNoResolve(RemoteId));
#else
		return FObjectPtr();
#endif
	}

	/**
	* Registers object ID as known to be owned by another server, without migrating it
	* @param ObjectId Remote object id
	* @param OwnerServerId Id of a remote server that currently owns the object
	*/
	COREUOBJECT_API void RegisterRemoteObjectId(FRemoteObjectId Id, FRemoteServerId ResidentServerId);

	/**
	* Returns the list of all object IDs currently borrowed from another server.
	*/
	COREUOBJECT_API void GetAllBorrowedObjects(TArray<FRemoteObjectId>& OutBorrowedObjectIds);

	/**
	* Registers object for sharing, marking it as owned by the current server
	* @param Object to make shareable
	*/
	COREUOBJECT_API void RegisterSharedObject(UObject* Object);

	extern COREUOBJECT_API const FRemoteServerId DatabaseId;

	COREUOBJECT_API void InitRemoteObjectTransfer();

	/** Delegate that transfers object data to another server */
	extern COREUOBJECT_API TDelegate<void(const FMigrateSendParams& /*Params*/)> RemoteObjectTransferDelegate;

	/** Delegate that handles an object request being denied */
	extern COREUOBJECT_API TDelegate<void(FRemoteObjectId /*ObjectId*/, FRemoteServerId /*DestinationServerId*/)> RemoteObjectDeniedTransferDelegate;

	/** Delegate that requests remote object data from LastKnownResidentServerId to be transferred to DestinationServerId. Allows requests to be forwarded if ObjectId does not reside on LastKnownResidentServerId. */
	extern COREUOBJECT_API TDelegate<void(FRemoteWorkPriority /*RequestPriority*/, FRemoteObjectId /*ObjectId*/, FRemoteServerId /*LastKnownResidentServerId*/, FRemoteServerId /*DestinationServerId*/)> RequestRemoteObjectDelegate;

	/** Delegate executed when objects have been migrated from another server. */
	extern COREUOBJECT_API TMulticastDelegate<void(const FRemoteObjectData& /*ObjectData*/, const FUObjectMigrationContext& /*MigrationContext*/)> OnObjectDataReceivedDelegate;

	/** Delegate executed when objects have been migrated from another server. */
	extern COREUOBJECT_API TMulticastDelegate<void(const TArray<UObject*>& /*MigratedObjects*/, const FUObjectMigrationContext& /*MigrationContext*/)> OnObjectsReceivedDelegate;

	/** Delegate executed when object data has been migrated to another server */
	extern COREUOBJECT_API TMulticastDelegate<void(const FRemoteObjectData& /*ObjectData*/, const FUObjectMigrationContext& /*MigrationContext*/)> OnObjectDataSentDelegate;

	/** 
	 * Delegate executed when objects have been migrated to another server. Migrated objects can still be resolved with weak pointers but at this point 
	 * any changes to their internal state won't be migrated across to the remote server 
	 */
	extern COREUOBJECT_API TMulticastDelegate<void(const TSet<UObject*>& /*MigratedObjects*/, const FUObjectMigrationContext& /*MigrationContext*/)> OnObjectsSentDelegate;

	/** Delegate executed when an object has been accessed by a transaction */
	extern COREUOBJECT_API TMulticastDelegate<void(FRemoteTransactionId /*RequestId*/, FRemoteObjectId /*ObjectId*/)> OnObjectTouchedDelegate;

	/** Delegate that stores locally unrachable object data into a database. */
	extern COREUOBJECT_API TDelegate<void(const FMigrateSendParams&)> StoreRemoteObjectDataDelegate;

	/** Delegate that restores object data from a database. */
	extern COREUOBJECT_API TDelegate<void(const FUObjectMigrationContext&)> RestoreRemoteObjectDataDelegate;

} // namespace UE::RemoveObject::Transfer

struct FRemoteObjectReference
{
private:
	/** Object id being shared with another server */
	FRemoteObjectId ObjectId;
	/** Id of a server that shared the object (last owner of the object) */
	FRemoteServerId ServerId;

public:
	FRemoteObjectReference() = default;

	FRemoteObjectReference(const FRemoteObjectReference&) = default;
	FRemoteObjectReference& operator=(const FRemoteObjectReference&) = default;

	COREUOBJECT_API explicit FRemoteObjectReference(const FObjectPtr& Ptr);

	template <typename T>
	explicit FRemoteObjectReference(const TObjectPtr<T>& Ptr)
		: FRemoteObjectReference(FObjectPtr(Ptr.GetHandle()))
	{
	}

	COREUOBJECT_API explicit FRemoteObjectReference(const FWeakObjectPtr& WeakPtr);

	template <typename T>
	explicit FRemoteObjectReference(const TWeakObjectPtr<T>& WeakPtr)
		: FRemoteObjectReference((FWeakObjectPtr)WeakPtr)
	{
	}

	FORCEINLINE bool operator==(const FRemoteObjectReference& Other) const
	{
		return ObjectId == Other.ObjectId;
	}

	COREUOBJECT_API FObjectPtr ToObjectPtr() const;
	COREUOBJECT_API FWeakObjectPtr ToWeakPtr() const;
	COREUOBJECT_API UObject* Resolve() const;

	FORCEINLINE FRemoteObjectId GetRemoteId() const
	{
		return ObjectId;
	}
	FORCEINLINE FRemoteServerId GetSharingServerId() const
	{
		return ServerId;
	}
	FORCEINLINE bool IsRemote() const
	{
		return UE::RemoteObject::Handle::IsRemote(ObjectId);
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}

	COREUOBJECT_API bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FRemoteObjectReference& Ref);

private:
	friend struct UE::Net::FRemoteObjectReferenceNetSerializer;
	COREUOBJECT_API void NetDequantize(FRemoteObjectId InObjectId, FRemoteServerId InServerId, const FRemoteObjectPathName& InPath);
};

template<>
struct TStructOpsTypeTraits<FRemoteObjectReference> : public TStructOpsTypeTraitsBase2<FRemoteObjectReference>
{
	enum
	{
		WithNetSerializer = true,
		WithSerializer = true,
	};
};
