// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IStormSyncTransportCoreModule.h"

class FStormSyncDiscoveryManager;

/**
 * Implements the StormSyncTransportCore module.
 */
class FStormSyncTransportCoreModule : public IStormSyncTransportCoreModule
{
public:
	//~ Begin IStormSyncTransportCoreModule
	virtual FOnGetEndpointConfig& OnGetCurrentTcpServerEndpointAddress() override { return OnGetCurrentTcpServerAddressDelegate; }
	virtual FOnGetEndpointConfig& OnGetServerEndpointMessageAddress() override { return OnGetServerEndpointMessageAddressDelegate; }
	virtual FOnGetEndpointConfig& OnGetClientEndpointMessageAddress() override { return OnGetClientEndpointMessageAddressDelegate; }
	//~ End IStormSyncTransportCoreModule

private:
	FOnGetEndpointConfig OnGetCurrentTcpServerAddressDelegate;
	FOnGetEndpointConfig OnGetServerEndpointMessageAddressDelegate;
	FOnGetEndpointConfig OnGetClientEndpointMessageAddressDelegate;
};
