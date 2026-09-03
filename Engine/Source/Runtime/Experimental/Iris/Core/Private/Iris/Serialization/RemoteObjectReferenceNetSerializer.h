// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/Serialization/InternalNetSerializer.h"
#include "UObject/RemoteObjectPathName.h"
#include "RemoteObjectReferenceNetSerializer.generated.h"

USTRUCT()
struct FRemoteObjectReferenceNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

USTRUCT()
struct FRemoteObjectReferenceNetSerializationHelper
{
	GENERATED_BODY()

	UPROPERTY()
	FRemoteObjectId ObjectId;

	UPROPERTY()
	FRemoteServerId ServerId;

	UPROPERTY()
	FRemoteObjectPathName Path;
};

namespace UE::Net
{

UE_NET_DECLARE_SERIALIZER_INTERNAL(FRemoteObjectReferenceNetSerializer, IRISCORE_API);

}
