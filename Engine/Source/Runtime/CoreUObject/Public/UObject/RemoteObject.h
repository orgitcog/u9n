// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/RemoteObjectTypes.h"
#include "UObject/ObjectMacros.h"

class UObject;

/**
 * Remote objects are unique UObjects that are referenced by a local server instance but their memory is actually owned by (exists on) another server.
 * 
 * It's possible that an object is remote but its memory hasn't been freed yet (UObject with EInternalObjectFlags::Remote flag that hasn't been GC'd yet). 
 * In such case any attempt to access that object through TObjectPtr will result in its memory being migrated from a remote server to a local server. 
 * Remote object memory is freed in the next GC pass after the object has been migrated and any existing references to that object (must be referenced by TObjectPtr) 
 * will be updated by GC to point to the remote object's FRemoteObjectStub. 
 */
namespace UE::RemoteObject
{
	/**
	* Returns a unique id associated with this (server) process
	*/
	COREUOBJECT_API FRemoteServerId GetGlobalServerId();

}

namespace UE::RemoteObject::Handle
{
	// Structure that holds basic information about a remote object
	// This is what FObjectPtr that references a remote object actually points to after the remote object's memory has been claimed by GC
	struct FRemoteObjectStub
	{
		FRemoteObjectId Id;
		FRemoteObjectId OuterId;
		FName Name;

		/** SerialNumber this object had on this server */
		int32 SerialNumber = 0;

		/** Server id that where the object currently resides */
		FRemoteServerId ResidentServerId;

		/** Server id of the server that has ownership of the object
			(note: only valid if the object is Local) */
		FRemoteServerId OwningServerId;

		/** Server id of the which server the object should be on for physics
			Default (invalid) means physics ID should not be considered when migrating the object.   */
		FRemoteServerId PhysicsOverrideServerId;

		FRemoteObjectStub() = default;
		COREUOBJECT_API explicit FRemoteObjectStub(UObject* Object);
	};

	enum class ERemoteReferenceType
	{
		Strong = 0,
		Weak = 1
	};

	/**
	* Resolves a remote object given its stub, aborting the active transaction if the object is unavailable
	* @param Stub Basic data required to migrate a remote object
	* @param RefType Reference type that wants to resolve an object
	* @return Resolved object
	*/
	COREUOBJECT_API UObject* ResolveObject(const FRemoteObjectStub* Stub, ERemoteReferenceType RefType = ERemoteReferenceType::Strong);

	/**
	* Resolves a remote object, aborting the active transaction if the object is unavailable
	* @param Object Object to resolve (remote object memory that has not yet been GC'd)
	* @param RefType Reference type that wants to resolve an object
	* @return Resolved object
	*/
	COREUOBJECT_API UObject* ResolveObject(UObject* Object, ERemoteReferenceType RefType = ERemoteReferenceType::Strong);
	
	COREUOBJECT_API void TouchResidentObject(UObject* Object);


	/**
	* Resolves a remote object
	* @param Object Object id to resolve
	* @param RefType Reference type that wants to resolve an object
	* @return True if a remote object can be resolved
	*/
	COREUOBJECT_API bool CanResolveObject(FRemoteObjectId ObjectId);

	/**
	* Checks if an object associated with the specified unique id is remote
	* @return True if an object associated with the specified unique id is remote
	*/
	COREUOBJECT_API bool IsRemote(FRemoteObjectId ObjectId);

	/**
	* Checks if an object (memory that has not yet been GC'd) is remote
	* @return True if the object is remote
	*/
	COREUOBJECT_API bool IsRemote(const UObject* Object);

	/**
	* Checks if a locally resident object is owned by this server
	*/
	COREUOBJECT_API bool IsOwned(const UObject* Object);

	/**
	* Checks if an object id is owned by this server
	* We are only able to check if we own the object. If we don't
	* own the object then we don't have a reliable way of knowing
	* who the owner is, which is why the below GetOwnerServerId
	* function requires the object be locally resident
	*/
	COREUOBJECT_API bool IsOwned(FRemoteObjectId ObjectId);

	/**
	* Get the owner server id for a locally resident object
	*/
	COREUOBJECT_API FRemoteServerId GetOwnerServerId(const UObject* Object);

	/**
	* Sets the owner server id for a locally resident object
	*/
	COREUOBJECT_API void ChangeOwnerServerId(const UObject* Object, FRemoteServerId NewOwnerServerId);

	/**
	* Get the physics server id where the object should be simulated on
	*/
	COREUOBJECT_API FRemoteServerId GetPhysicsServerId(const UObject* Object);

	/**
	* Sets the physics id where the object should go to
	*/
	COREUOBJECT_API void ChangePhysicsServerId(const UObject* Object, FRemoteServerId NewPhysicsServerId);

	COREUOBJECT_API void ClearAllPhysicsServerId();

	COREUOBJECT_API void UpdateAllPhysicsServerId(const TMap<uint32, uint32>& PhysicsServerMergingMap);
}
