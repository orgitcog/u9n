// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Misc/Timeout.h"

// Interface used by FPhysScene_AsyncPhysicsStateJobQueue

class UObject;
class UBodySetup;

class IAsyncPhysicsStateProcessor
{
public:
	/** Returns whether this component allows having its physics state being created asynchronously (outside of the GameThread). */
	virtual bool AllowsAsyncPhysicsStateCreation() const { return false; }
	
	/** Returns whether this component allows having its physics state being destroyed asynchronously (outside of the GameThread). */
	virtual bool AllowsAsyncPhysicsStateDestruction() const { return false; }

	/** Returns whether the physics state is created. */
	virtual bool IsAsyncPhysicsStateCreated() const { return false; }

	/** Returns the associated UObject for this processor. */
	virtual UObject* GetAsyncPhysicsStateObject() const { return nullptr; }

	/** Returns body setups that needs to create their physics meshes before physics state asynchronous creation */
	virtual void CollectBodySetupsWithPhysicsMeshesToCreate(TSet<UBodySetup*>& OutBodySetups) const {}

	/** Used to create any physics engine information for this component outside of the GameThread. */
	virtual bool OnAsyncCreatePhysicsState(const UE::FTimeout& TimeOut) { return true; }

	/** Called on the GameThread once the component's physic engine information is created. */
	virtual void OnAsyncCreatePhysicsStateEnd_GameThread() {}

	/** Called on the GameThread before the component's physic engine information is destroyed. */
	virtual void OnAsyncDestroyPhysicsStateBegin_GameThread() {}

	/** Used to destroy any physics engine information for this component outside of the GameThread. */
	virtual bool OnAsyncDestroyPhysicsState(const UE::FTimeout& TimeOut) { return true; }

	/** Called on the GameThread once the component's physic engine information is destroyed. */
	virtual void OnAsyncDestroyPhysicsStateEnd_GameThread() {}
};