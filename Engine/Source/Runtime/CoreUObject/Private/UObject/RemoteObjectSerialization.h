// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/RemoteObjectTransfer.h"

namespace UE::RemoteObject::Handle { struct FRemoteObjectStub; }

#ifndef UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING
#define UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING 0
#endif

namespace UE::RemoteObject::Serialization
{
	enum class ERemoteObjectSerializationFlags : uint8
	{
		None = 0,
		UseExistingObjects = (1 << 0),			// If possible re-use existing UObjects and don't reconstruct them when deserializing object data
		PreserveRemoteReferences = (1 << 1),		// Don't overwrite references to objects that are remote
		Resetting = (1 << 2)						// Indicates that the serialization process is resetting an object to its archetype state
	};
	ENUM_CLASS_FLAGS(ERemoteObjectSerializationFlags);

	/**
	* Basic information needed to construct deserialized remote object (this information does not get serialized in UObject::Serialize())
	*/
	struct FRemoteObjectHeader
	{
		FName Name;
		FRemoteObjectId RemoteId;
		TObjectPtr<UClass> Class;
		TObjectPtr<UObject> Outer;
		TObjectPtr<UObject> Archetype;
		int32 InternalFlags = 0;
		int64 NextOffset = 0;

		/** Transient (SerialNumber is local-only but it's stored here for convenience when deserializing object data) */
		int32 SerialNumber = 0;
	};

	/**
	* Basic information needed to construct remote (sub)object
	*/
	struct FRemoteObjectConstructionParams
	{
		FName Name;
		FRemoteObjectId OuterId;
		FRemoteObjectId RemoteId;
		int32 SerialNumber = 0;
	};

	/**
	* Stores basic information for constructing remote (sub)objects.
	* Prevents unnecessary calls to FRemoteObjectId::Generate() when constructing default subobjects (or in general objects constructed in remote objects' constructors)
	* Sets the SerialNumber during subobject construction so that any weak pointers also constructed in constructors that may point to a subobject, has the correct SerialNumber
	*/
	class FRemoteObjectConstructionOverrides
	{
		TArray<FRemoteObjectConstructionParams> Overrides;

	public:

		explicit FRemoteObjectConstructionOverrides(const TArray<FRemoteObjectHeader>& InObjectHeaders);

		/**
		* Finds object construction overrides for an object that will be constricted with the specified Name and Outer
		*/
		const FRemoteObjectConstructionParams* Find(FName InName, UObject* InOuter) const;
	};

	/**
	* Serializes an object and its subobject (or if the object is a default subobject, its parent and the parent's subobjects)
	* @param InObject Object to be serialized
	* @param OutObjects All objects that have been serialized (Object and its subobjects and/or parent)
	* @param OutReferencedObjects Keeps track of all objects that need to be tagged with RemoteReference
	* @param MigrationContext Contains the meta data of the current migration request
	* @return Remote object data representing the serialized objects
	*/
	FRemoteObjectData SerializeObjectData(UObject* InObject, TSet<UObject*>& OutObjects, TSet<UObject*>& OutReferencedObjects, const FUObjectMigrationContext* MigrationContext);

	/**
	* Deserializes remote object data
	* @param ObjectData the data to deserailize 
	* @param MigrationContext the Context (meta data) of the current migration that's causing the deserialization
	* @param OutObjectRemoteIds Remote IDs of the deserialized objects
	* @param OutReceivedObjects All deserialized objects
	* @param DeserializeFlags Flags modifying the behavior of the deserialization process
	* @return Index of an object in OutReceivedObjects that was the main object the migration request was triggered for 
	* (usually 0 but if a migration requests a default subobject then its parent is also migrated and the return value will be > 0)
	*/
	int32 DeserializeObjectData(FRemoteObjectData& ObjectData, const FUObjectMigrationContext* MigrationContext, TArray<FRemoteObjectId>& OutObjectRemoteIds, TArray<UObject*>& OutReceivedObjects, ERemoteObjectSerializationFlags DeserializeFlags = ERemoteObjectSerializationFlags::None);

	/**
	* Finds the canonical 'root' object that is used for remote object serialization
	* - we trace up the chain of Outer pointers until we reach the first non default subobject
	*/
	UObject* FindCanonicalRootObjectForSerialization(UObject* Object);

} // namespace UE::RemoveObject::Serialization


namespace UE::RemoteObject::Serialization::Disk
{
	void LoadObjectFromDisk(const FUObjectMigrationContext& MigrationContext);
	void SaveObjectToDisk(const UE::RemoteObject::Transfer::FMigrateSendParams& Params);
}