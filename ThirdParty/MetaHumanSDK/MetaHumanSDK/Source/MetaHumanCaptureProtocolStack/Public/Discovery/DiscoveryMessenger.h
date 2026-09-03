// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Discovery/Communication/DiscoveryCommunication.h"
#include "Discovery/Communication/DiscoveryPacket.h"

#include "Discovery/Messages/DiscoveryRequest.h"
#include "Discovery/Messages/DiscoveryResponse.h"
#include "Discovery/Messages/DiscoveryNotify.h"

#include "Utility/Error.h"

class METAHUMANCAPTUREPROTOCOLSTACK_API FDiscoveryMessenger final
{
public:

    DECLARE_DELEGATE_OneParam(FOnResponseArrived, FDiscoveryResponse InResponse);
    DECLARE_DELEGATE_OneParam(FOnNotifyArrived, FDiscoveryNotify InResponse);

    FDiscoveryMessenger();
    ~FDiscoveryMessenger();

    TProtocolResult<void> Start();
    TProtocolResult<void> Stop();

    TProtocolResult<void> SendMulticastRequest();

    void SetResponseHandler(FOnResponseArrived InOnResponse);
    void SetNotifyHandler(FOnNotifyArrived InOnNotify);

private:

    void OnPacketArrived(FDiscoveryPacket InPacket);

    FDiscoveryCommunication Communication;

    FOnResponseArrived OnResponse;
    FOnNotifyArrived OnNotify;
};
