// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Components/ActorComponent.h"
#include "NetworkPredictionProxy.h"
#include "NetworkPredictionReplicationProxy.h"

#include "PhysicsInterfaceDeclaresCore.h"
#include "NetworkPredictionPhysicsComponent.generated.h"

#define UE_API NETWORKPREDICTION_API

enum ENetRole : int;
namespace EEndPlayReason { enum Type : int; }

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
//	UNetworkPredictionPhysicsComponent
//
//	This is a component that will register a FGenericPhysicsModelDef and bind to the first UPrimitiveComponent found in the parent actor.
//	That is - there is NO backing NetworkPrediction simulation/gameplay code. This will always be SimulatedProxy and does not support any
//	of the AP<-->Server communication that UNetworkPredictionComponent.
//
//	To fully emphasize: this is for STAND ALONE physics objects that want to use NP's fixed tick services. 
//	It is NOT for "any NP object that uses physics". A vehicle that can be controlled by a client needs to use (or model after) UNetworkPredictionComponent.
//
//	Even then, it is just sort of an example. An extra component may not be the best way to use this. It may make more sense to just
//	put the important bits on your actor class and set the exact UpdatedPrimitive you want (really PhysicsActorHandle is all that matters!)
//
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
UCLASS(MinimalAPI, BlueprintType, meta=(BlueprintSpawnableComponent))
class UNetworkPredictionPhysicsComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UE_API UNetworkPredictionPhysicsComponent();

	UE_API virtual void InitializeComponent() override;
	UE_API virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	UE_API virtual void PreNetReceive() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type Reason);

protected:
	
	// Classes must initialize the NetworkPredictionProxy (register with the NetworkPredictionSystem) here. EndPlay will unregister.
	UE_API virtual void InitializeNetworkPredictionProxy();

	// Finalizes initialization when NetworkRole changes. Does not need to be overridden.
	UE_API virtual void InitializeForNetworkRole(ENetRole Role, const bool bHasNetConnection);

	// Helper: Checks if the owner's role has changed and calls InitializeForNetworkRole if necessary.
	UE_API bool CheckOwnerRoleChange();
	
	UE_API virtual void SetPrimitiveComponent(UPrimitiveComponent* NewUpdatedComponent);

	// Proxy to interface with the NetworkPrediction system
	UPROPERTY(Replicated, transient)
	FNetworkPredictionProxy NetworkPredictionProxy;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> UpdatedPrimitive = nullptr;

	UPROPERTY(Replicated, transient)
	FReplicationProxy ReplicationProxy;

	FPhysicsActorHandle PhysicsActorHandle = nullptr;

	FReplicationProxySet GetReplicationProxies()
	{
		return FReplicationProxySet{ nullptr, nullptr, &ReplicationProxy, nullptr };
	}
	
};

#undef UE_API
