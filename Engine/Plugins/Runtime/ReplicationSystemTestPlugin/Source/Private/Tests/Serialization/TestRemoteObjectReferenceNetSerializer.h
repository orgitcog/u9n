// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Tests/ReplicationSystem/RPC/ReplicatedTestObjectWithRPC.h"
#include "UObject/RemoteObjectTransfer.h"
#include "TestRemoteObjectReferenceNetSerializer.generated.h"

/** Just need an empty object the test can spawn with a stable name. Can't use UObject directly since it' abstract. */
UCLASS()
class UTestNamedObject : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class UTestReplicatedObjectWithRemoteReference : public UTestReplicatedObjectWithRPC
{
	GENERATED_BODY()

public:
	void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const;

	UPROPERTY(Replicated)
	FRemoteObjectReference RemoteReferenceProperty;

	UFUNCTION(Client, Reliable)
	void RemoteRPCWithRemoteReferenceParam(FRemoteObjectReference RemoteReference);

	int32 RemoteRPCWithRemoteReferenceParamCallCount = 0;

	FRemoteObjectReference LastReceivedRemoteReference;
};
