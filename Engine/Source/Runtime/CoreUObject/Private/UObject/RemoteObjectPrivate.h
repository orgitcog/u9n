// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRemoteObject, Display, All);

struct FRemoteObjectPathName;

namespace UE::RemoteObject::Private
{
	/**
	* Initializes remote objects subsystems
	*/
	void InitRemoteObjects();

	/**
	* Marks object memory as remote and creates its stub
	*/
	void MarkAsRemote(UObject* Object, FRemoteServerId DestinationServerId);

	/**
	* Marks object memory as local
	*/
	void MarkAsLocal(UObject* Object);

	/**
	* Registers a stub for a remote object that is known to be resident on a specific server
	*/
	void RegisterRemoteObjectId(FRemoteObjectId ObjectId, FRemoteServerId ResidentServerId);

	/**
	* Registers object for sharing, marking it as owned by the current server
	*/
	void RegisterSharedObject(UObject* Object);

	/**
	* Finds remote object stub
	*/
	UE::RemoteObject::Handle::FRemoteObjectStub* FindRemoteObjectStub(FRemoteObjectId ObjectId);

	/**
	* Gets a base FName that will be used to generate a unique object name (see MakeUniqueObjectName)
	*/
	FName GetServerBaseNameForUniqueName(const UClass* Class);

	/**
	* Stores FRemoteObjectPath for a remotely referenced asset that's about to be destroyed so that the engine knows it should load the asset when something requests it
	*/
	void StoreAssetPath(UObject* Object);

	/**
	* Attempts to find an FRemoteObjectPath for an object id representing an asset
	*/
	FRemoteObjectPathName* FindAssetPath(FRemoteObjectId RemoteId);

	struct FUnsafeToMigrateScope
	{
		FUnsafeToMigrateScope();
		~FUnsafeToMigrateScope();
	};
}

namespace UE::RemoteObject::Transfer::Private
{


	/**
	* Stores unreachable objects to database
	*/
	void StoreObjectToDatabase(UObject* Object);
}