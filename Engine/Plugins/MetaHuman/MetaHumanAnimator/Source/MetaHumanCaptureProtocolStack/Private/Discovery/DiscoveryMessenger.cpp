// Copyright Epic Games, Inc. All Rights Reserved.

#include "Discovery/DiscoveryMessenger.h"

DEFINE_LOG_CATEGORY(LogCPSDiscoveryMessenger)

FDiscoveryMessenger::FDiscoveryMessenger()
{
}

FDiscoveryMessenger::~FDiscoveryMessenger()
{
    Stop();
}

TProtocolResult<void> FDiscoveryMessenger::Start()
{
    CPS_CHECK_VOID_RESULT(Communication.Start());

    Communication.SetReceiveHandler(
        FDiscoveryCommunication::FOnPacketReceived::CreateRaw(this, &FDiscoveryMessenger::OnPacketArrived));

    return ResultOk;
}

TProtocolResult<void> FDiscoveryMessenger::Stop()
{
    return Communication.Stop();
}

TProtocolResult<void> FDiscoveryMessenger::SendMulticastRequest()
{
    constexpr int32 NumberOfRequests = 3;

    FDiscoveryRequest Request;
    TProtocolResult<FDiscoveryPacket> SerializeResult = FDiscoveryRequest::Serialize(Request);

    if (SerializeResult.IsError())
    {
        return FCaptureProtocolError(TEXT("Failed to serialize request"));
    }

    for (int32 Counter = 0; Counter < NumberOfRequests; ++Counter)
    {
		FDiscoveryPacket Packet = SerializeResult.GetResult();
        Communication.SendMessage(MoveTemp(Packet), FDiscoveryCommunication::MulticastAddress, FDiscoveryCommunication::MulticastPort);
    }

    return ResultOk;
}

void FDiscoveryMessenger::SetResponseHandler(FOnResponseArrived InOnResponse)
{
    OnResponse = MoveTemp(InOnResponse);
}

void FDiscoveryMessenger::SetNotifyHandler(FOnNotifyArrived InOnNotify)
{
    OnNotify = MoveTemp(InOnNotify);
}

void FDiscoveryMessenger::OnPacketArrived(FDiscoveryPacket InPacket)
{
    const TArray<uint8>& Payload = InPacket.GetPayload();

    if (InPacket.GetMessageType() == FDiscoveryPacket::EMessageType::Request)
    {
        UE_LOG(LogCPSDiscoveryMessenger, Error, TEXT("Client currently doesn't support requests."));
        return;
    }
    else if (InPacket.GetMessageType() == FDiscoveryPacket::EMessageType::Response)
    {
        TProtocolResult<FDiscoveryResponse> DeserializeResult = FDiscoveryResponse::Deserialize(InPacket);

        if (DeserializeResult.IsError())
        {
            UE_LOG(LogCPSDiscoveryMessenger, Error, TEXT("Failed to parse the response message."));
            return;
        }

        FDiscoveryResponse Response = DeserializeResult.ClaimResult();

        OnResponse.ExecuteIfBound(MoveTemp(Response));
    }
    else if (InPacket.GetMessageType() == FDiscoveryPacket::EMessageType::Notify)
    {
        TProtocolResult<FDiscoveryNotify> DeserializeResult = FDiscoveryNotify::Deserialize(InPacket);

        if (DeserializeResult.IsError())
        {
            UE_LOG(LogCPSDiscoveryMessenger, Error, TEXT("Failed to parse the notify message."));
            return;
        }

        FDiscoveryNotify Notify = DeserializeResult.ClaimResult();

        OnNotify.ExecuteIfBound(MoveTemp(Notify));
    }
    else
    {
        UE_LOG(LogCPSDiscoveryMessenger, Error, TEXT("Invalid message arrived."));
        return;
    }
}

