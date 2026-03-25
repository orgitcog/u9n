// Copyright Epic Games, Inc. All Rights Reserved.

#include "Discovery/Communication/DiscoveryCommunication.h"

DEFINE_LOG_CATEGORY(LogCPSDiscoveryCommunication)

const uint16 FDiscoveryCommunication::MulticastPort = 27838; // According to the specification
const FString FDiscoveryCommunication::MulticastAddress = TEXT("239.255.137.139"); // According to the specification
FDiscoveryCommunication::FDiscoveryCommunication()
    : SynchronizedReceiver(FRunnerType::FOnProcess::CreateRaw(this, &FDiscoveryCommunication::OnProcessReceivedPacket))
{
}

TProtocolResult<void> FDiscoveryCommunication::Start()
{
    CPS_CHECK_VOID_RESULT(Client.Init({ MulticastPort, MulticastAddress }, FOnSocketDataReceived::CreateRaw(this, &FDiscoveryCommunication::OnPacketArrived)));
    CPS_CHECK_VOID_RESULT(Client.Start());

    return ResultOk;
}

TProtocolResult<void> FDiscoveryCommunication::Stop()
{
    return Client.Stop();
}

void FDiscoveryCommunication::SendMessage(FDiscoveryPacket InMessage, const FString& InEndpoint)
{
    TProtocolResult<TArray<uint8>> SerializeResult = FDiscoveryPacket::Serialize(InMessage);

    if (SerializeResult.IsError())
    {
        UE_LOG(LogCPSDiscoveryCommunication, Error, TEXT("Invalid message: %s"), *(SerializeResult.ClaimError().GetMessage()));
        return;
    }

    Client.SendMessage(SerializeResult.ClaimResult(), InEndpoint);
}

void FDiscoveryCommunication::SendMessage(FDiscoveryPacket InMessage, const FString& InEndpointIp, const uint16 InEndpointPort)
{
    SendMessage(MoveTemp(InMessage), InEndpointIp + TEXT(":") + FString::FromInt(InEndpointPort));
}

void FDiscoveryCommunication::SetReceiveHandler(FOnPacketReceived InReceiveHandler)
{
    OnPacketReceived = MoveTemp(InReceiveHandler);
}

void FDiscoveryCommunication::OnPacketArrived(const FArrayReaderPtr& InPayload, const FIPv4Endpoint& InEndpoint)
{
    const TArray<uint8>& Payload = *InPayload;

    TProtocolResult<FDiscoveryPacket> DeserializeResult = FDiscoveryPacket::Deserialize(Payload);
    if (DeserializeResult.IsError())
    {
        UE_LOG(LogCPSDiscoveryCommunication, Error, TEXT("Invalid message: %s"), *(DeserializeResult.ClaimError().GetMessage()));
        return;
    }

    FDiscoveryPacket Packet = DeserializeResult.ClaimResult();
    SynchronizedReceiver.Add(MoveTemp(Packet));
}

void FDiscoveryCommunication::OnProcessReceivedPacket(FDiscoveryPacket InPacket)
{
    OnPacketReceived.ExecuteIfBound(MoveTemp(InPacket));
}