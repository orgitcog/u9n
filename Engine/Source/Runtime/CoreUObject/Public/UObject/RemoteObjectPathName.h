// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteObjectTypes.h"

/**
* Structure that holds unique Names and RemoteIds associated with remote object data or pathnames (noexport type)
*/
struct FRemoteObjectTables
{
	TArray<FName> Names;
	TArray<FRemoteObjectId> RemoteIds;

	FORCEINLINE int32 Num() const
	{
		return Names.Num();
	}

	COREUOBJECT_API void Serialize(FArchive& Ar);
};

inline FArchive& operator<<(FArchive& Ar, FRemoteObjectTables& Tables)
{
	Tables.Serialize(Ar);
	return Ar;
}


/**
* Structure that represents remote object pathname (noexport type)
* Stores pathnames as an array of indices (into a sidecar FRemoteObjectTables object) of FNames and associated FRemoteObjectIds
* Names and Ids are stored from the innermost object first to the outermost object last
*/
struct FPackedRemoteObjectPathName
{
	using FNameIndexType = uint16;
	TArray<FNameIndexType> RemoteIds;
	TArray<FNameIndexType> Names;

	FORCEINLINE int32 Num() const
	{
		return Names.Num();
	}

	FORCEINLINE FName GetSegmentName(int32 InSegmentIndex, const FRemoteObjectTables& InTables) const
	{
		return InTables.Names[Names[InSegmentIndex]];
	}

	FORCEINLINE FRemoteObjectId GetSegmentId(int32 InSegmentIndex, const FRemoteObjectTables& InTables) const
	{
		return InTables.RemoteIds[RemoteIds[InSegmentIndex]];
	}

	COREUOBJECT_API UObject* Resolve(const FRemoteObjectTables& InTables) const;
	COREUOBJECT_API FString ToString(const FRemoteObjectTables& InTables, int32 InMinPathSegmentIndex = 0) const;
	COREUOBJECT_API void Serialize(FArchive& Ar);
};

inline FArchive& operator<<(FArchive& Ar, FPackedRemoteObjectPathName& PathName)
{
	PathName.Serialize(Ar);
	return Ar;
}

/**
* Structure that represents remote object pathname (noexport type)
* Stores pathnames as an array of FNames and associated FRemoteObjectIds
* Names and Ids are stored from the innermost object first to the outermost object last
*/
struct FRemoteObjectPathName : public FRemoteObjectTables
{
	FRemoteObjectPathName() = default;
	FRemoteObjectPathName(FRemoteObjectPathName&&) = default;
	COREUOBJECT_API explicit FRemoteObjectPathName(UObject* InObject);
	FRemoteObjectPathName& operator=(FRemoteObjectPathName&&) = default;

	FRemoteObjectPathName(const FRemoteObjectPathName&) = default;
	FRemoteObjectPathName& operator=(const FRemoteObjectPathName&) = default;

	FORCEINLINE FName GetSegmentName(int32 InSegmentIndex, const FRemoteObjectTables&) const
	{
		return Names[InSegmentIndex];
	}

	FORCEINLINE FRemoteObjectId GetSegmentId(int32 InSegmentIndex, const FRemoteObjectTables&) const
	{
		return RemoteIds[InSegmentIndex];
	}

	COREUOBJECT_API UObject* Resolve() const;
	COREUOBJECT_API FString ToString(int32 InMinPathSegmentIndex = 0) const;
};