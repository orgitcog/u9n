// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/PhysicsObject.h"

class UPrimitiveComponent;
struct FRigidBodyState;
struct FNetworkPhysicsSettingsAsync;

class IPhysicsReplication // Game Thread API
{
public:
	virtual ~IPhysicsReplication() { }

	virtual void Tick(float DeltaSeconds) { }

	virtual void SetReplicatedTarget(UPrimitiveComponent* Component, FName BoneName, const FRigidBodyState& ReplicatedTarget, int32 ServerFrame) = 0;

	virtual void RemoveReplicatedTarget(UPrimitiveComponent* Component) = 0;
};

class IPhysicsReplicationAsync // Physics Thread API
{
public:
	virtual ~IPhysicsReplicationAsync() { }

	virtual void RegisterSettings(Chaos::FConstPhysicsObjectHandle PhysicsObject, FNetworkPhysicsSettingsAsync InSettings) = 0;
};
