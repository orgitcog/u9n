// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DiscoveryPacket.h"

#include "Communication/UdpClient.h"

#include "Utility/Error.h"
#include "Utility/QueueRunner.h"

class FDiscoveryCommunication final
{
public:

	static const uint16 MulticastPort;
    static const FString MulticastAddress;

    DECLARE_DELEGATE_OneParam(FOnPacketReceived, FDiscoveryPacket InPacket)

    using FRunnerType = TQueueRunner<FDiscoveryPacket>;

    FDiscoveryCommunication();

    TProtocolResult<void> Start();
    TProtocolResult<void> Stop();

    void SendMessage(FDiscoveryPacket InMessage, const FString& Endpoint);
    void SendMessage(FDiscoveryPacket InMessage, const FString& EndpointIp, const uint16 EndpointPort);

    void SetReceiveHandler(FOnPacketReceived InReceiveHandler);

private:

    void OnProcessReceivedPacket(FDiscoveryPacket InPacket);
    void OnPacketArrived(const FArrayReaderPtr& InPayload, const FIPv4Endpoint& InEndpoint);

    FUdpClient Client;

    FRunnerType SynchronizedReceiver;

    FOnPacketReceived OnPacketReceived;
};