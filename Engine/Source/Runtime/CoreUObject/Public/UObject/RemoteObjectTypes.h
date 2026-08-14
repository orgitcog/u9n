// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/TypeHash.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/ObjectMacros.h"

class UObjectBase;
class UObject;
class FArchive;

enum class ERemoteServerIdConstants : uint32
{
	Invalid = 0,
	FirstValid,
	Max = (1 << 10) - 1,

	Database = Max,
	Asset = Max - 1,

	// Add new reserved server IDs above this line in a descending order (so the next would be Something = Max - 2)

	FirstReservedPlusOne,
	FirstReserved = FirstReservedPlusOne - 1
};

struct FRemoteServerId
{
	friend struct FRemoteObjectId;

	FRemoteServerId() = default;
	explicit FRemoteServerId(ERemoteServerIdConstants InId)
		: Id((uint32)InId)
	{
	}
	explicit FRemoteServerId(uint32 InId)
		: Id(InId)
	{
		checkf(Id < (uint32)ERemoteServerIdConstants::FirstReserved, TEXT("Remote server id can not be greater than %u, got: %u"), (uint32)ERemoteServerIdConstants::FirstReserved - 1, Id);
	}
	explicit COREUOBJECT_API FRemoteServerId(const FString& InText);
	COREUOBJECT_API FString ToString() const;

	uint32 GetIdNumber() const
	{
		return Id;
	}
	bool IsValid() const
	{
		return Id != 0;
	}
	bool operator == (FRemoteServerId Other) const
	{
		return Id == Other.Id;
	}
	bool operator != (FRemoteServerId Other) const
	{
		return Id != Other.Id;
	}
	bool operator < (FRemoteServerId Other) const
	{
		return Id < Other.Id;
	}
	bool operator <= (FRemoteServerId Other) const
	{
		return Id <= Other.Id;
	}
	void operator = (FRemoteServerId Other)
	{
		Id = Other.Id;
	}
	bool IsAsset() const
	{
		return (Id == (uint32)ERemoteServerIdConstants::Asset);
	}
	bool IsDatabase() const
	{
		return (Id == (uint32)ERemoteServerIdConstants::Database);
	}

	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FRemoteServerId& Id);

private:

	/** 
	 * Constructor used exclusively by FRemoteObjectId::GetServerId() to bypass range changes in the other constructors 
	 */
	FRemoteServerId(uint32 InId, ERemoteServerIdConstants /*Unused*/)
		: Id(InId)
	{
	}

	uint32 Id = (uint32)ERemoteServerIdConstants::Invalid;
};

struct FRemoteObjectId
{
private:
	union
	{
		struct
		{
			uint64 SerialNumber : 54;
			uint64 ServerId : 10;
		};
		uint64 Id = 0;
	};

public:

	FRemoteObjectId() = default;

	FRemoteObjectId(FRemoteServerId InServerId, uint64 InSerialNumber)
		: SerialNumber(InSerialNumber)
		, ServerId(InServerId.Id)
	{
	}

	explicit COREUOBJECT_API FRemoteObjectId(const UObjectBase* InObject);

	FORCEINLINE uint32 GetTypeHash() const
	{
		return ::GetTypeHash(Id);
	}

	FORCEINLINE bool operator==(const FRemoteObjectId& Other) const
	{
		return Id == Other.Id;
	}
	FORCEINLINE bool operator!=(const FRemoteObjectId& Other) const
	{
		return Id != Other.Id;
	}
	FORCEINLINE bool operator<(const FRemoteObjectId& Other) const
	{
		return Id < Other.Id;
	}
	FORCEINLINE bool operator<=(const FRemoteObjectId& Other) const
	{
		return Id <= Other.Id;
	}
	FORCEINLINE bool operator>(const FRemoteObjectId& Other) const
	{
		return Id > Other.Id;
	}
	FORCEINLINE bool operator>=(const FRemoteObjectId& Other) const
	{
		return Id >= Other.Id;
	}
	FORCEINLINE bool IsValid() const
	{
		return *this != FRemoteObjectId();
	}

	FORCEINLINE uint64 GetIdNumber() const
	{
		return Id;
	}
	
	FORCEINLINE FRemoteServerId GetServerId() const
	{
		return FRemoteServerId(ServerId, ERemoteServerIdConstants::Invalid);
	}

	FORCEINLINE bool IsAsset() const
	{
		return GetServerId().IsAsset();
	}

	COREUOBJECT_API FString ToString() const;

	COREUOBJECT_API static FRemoteObjectId Generate(UObjectBase* InObject, EInternalObjectFlags InInitialFlags = EInternalObjectFlags::None);

	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FRemoteObjectId& Id);
};

FORCEINLINE uint32 GetTypeHash(const FRemoteObjectId& ObjectId)
{
	return ObjectId.GetTypeHash();
}
