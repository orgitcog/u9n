// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkHubMessagingModule.h"

#include "LiveLinkHubMessages.h"
#include "HAL/CriticalSection.h"

#ifndef WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
#define WITH_LIVELINK_DISCOVERY_MANAGER_THREAD 1
#endif

class FLiveLinkHubConnectionManager;

class FLiveLinkHubMessagingModule : public ILiveLinkHubMessagingModule
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	//~ Begin ILiveLinkHubMessagingModule interface
	virtual FOnHubConnectionEstablished& OnConnectionEstablished() override
	{
		return ConnectionEstablishedDelegate;
	}

	virtual void SetHostTopologyMode(ELiveLinkTopologyMode InMode) override;
	virtual FLiveLinkHubInstanceId GetInstanceId() const override;
	virtual void SetInstanceId(const FLiveLinkHubInstanceId& Id) override;
	virtual ELiveLinkTopologyMode GetHostTopologyMode() const override;
	//~ End ILiveLinkHubMessagingModule interface

private:
	/**
	 * Note: Invoked on the UI (Game) thread.
	 * Filter invoked by the messagebus source factory to filter out sources in the creation panel.
	 */
	bool OnFilterMessageBusSource(UClass* FactoryClass, TSharedPtr<struct FProviderPollResult, ESPMode::ThreadSafe> PollResult);

	/** Handle a message telling this host to connect to a provider if the topology mode is compatible. */
	void HandleDiscoveryMessage(const FLiveLinkHubDiscoveryMessage& Message, const TSharedRef<class IMessageContext, ESPMode::ThreadSafe>& Context);

private:
	bool bUseConnectionManager;

#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
	/** Manages the connection to the live link hub. */
	TPimplPtr<FLiveLinkHubConnectionManager> ConnectionManager;
#endif

	/** Handle to the delegate used to filter message bus sources. */
	FDelegateHandle SourceFilterDelegate;

	/** Delegate called when the connection between a livelink hub and the editor is established. */
	FOnHubConnectionEstablished ConnectionEstablishedDelegate;

	/** Lock to access the instance info struct. */
	mutable FCriticalSection InstanceInfoLock;

	struct FInstanceInfo
	{
		/** Topology Mode for this host. */
		ELiveLinkTopologyMode TopologyMode = ELiveLinkTopologyMode::Hub;
		/** Instance ID for this host. */
		FLiveLinkHubInstanceId Id = FLiveLinkHubInstanceId(FGuid());
	} InstanceInfo;

	/** Critical section to protect access to the disconnected clients list. */
	FCriticalSection DisconnectedClientsLock;

	/** List of clients that were explicitly disconnected and shouldn't reconnect automatically. */
	TMap<FString, double> RecentlyDisconnectedClients;

	/** Simple endpoint meant to respond to LLH discovery messages. */
	TSharedPtr<class FMessageEndpoint> GameThreadEndpoint;
};

