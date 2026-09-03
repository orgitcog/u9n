// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Discovery/Communication/DiscoveryCommunication.h"
#include "Discovery/Communication/DiscoveryPacket.h"

#include "Discovery/Messages/DiscoveryRequest.h"
#include "Discovery/Messages/DiscoveryResponse.h"
#include "Discovery/Messages/DiscoveryNotify.h"

#include "Utility/Error.h"

namespace UE::CaptureManager
{

class CAPTUREPROTOCOLSTACK_API FDiscoveryMessenger final
{
public:
	using FOnResponseArrived = TDelegate<void(FString, FDiscoveryResponse), FDefaultTSDelegateUserPolicy>;
	using FOnNotifyArrived = TDelegate<void(FString, FDiscoveryNotify), FDefaultTSDelegateUserPolicy>;

	FDiscoveryMessenger();
	~FDiscoveryMessenger();

	TProtocolResult<void> Start();
	TProtocolResult<void> Stop();

	TProtocolResult<void> SendRequest();

	void SetResponseHandler(FOnResponseArrived InOnResponse);
	void SetNotifyHandler(FOnNotifyArrived InOnNotify);

private:

	void OnPacketArrived(FString InServerIp, FDiscoveryPacket InPacket);

	FDiscoveryCommunication Communication;

	FOnResponseArrived OnResponse;
	FOnNotifyArrived OnNotify;
};

}