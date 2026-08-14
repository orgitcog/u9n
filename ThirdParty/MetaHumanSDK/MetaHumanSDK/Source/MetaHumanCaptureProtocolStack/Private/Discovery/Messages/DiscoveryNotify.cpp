// Copyright Epic Games, Inc. All Rights Reserved.

#include "Discovery/Messages/DiscoveryNotify.h"
#include "Containers/ArrayView.h"

const uint32 FDiscoveryNotify::MinPayloadSize = 16 + 2 + 1 + 2; // ServerId + ControlPort + ConnectionState + Supported Versions (at least 1)
FDiscoveryNotify::FDiscoveryNotify(FServerId InServerId, uint16 InControlPort, EConnectionState InConnectionState, TArray<uint16> InSupportedVersions)
    : ServerId(MoveTemp(InServerId))
    , ControlPort(InControlPort)
    , ConnectionState(InConnectionState)
	, SupportedVersions(MoveTemp(InSupportedVersions))
{
}

TProtocolResult<FDiscoveryNotify> FDiscoveryNotify::Deserialize(const FDiscoveryPacket& InPacket)
{
	if (InPacket.GetMessageType() != FDiscoveryPacket::EMessageType::Notify)	
	{ 
		return FCaptureProtocolError(TEXT("Invalid request arrived"));
	}

	TConstArrayView<uint8> Payload = InPacket.GetPayload();

	if (Payload.Num() < MinPayloadSize)
    {
        return FCaptureProtocolError(TEXT("Invalid number of bytes"));
    }

    FServerId ServerId;
    FMemory::Memcpy(ServerId.GetData(), Payload.GetData(), ServerId.Num());

    uint16 ControlPort = *reinterpret_cast<const uint16*>(Payload.GetData() + ServerId.Num());
    uint8 ConnectionState = Payload[ServerId.Num() + sizeof(ControlPort)];

    EConnectionState ConnectionStateEnum = ToConnectionState(ConnectionState);

    if (ConnectionStateEnum == EConnectionState::Invalid)
    {
        return FCaptureProtocolError(TEXT("Invalid connection state field"));
    }

	TConstArrayView<uint8> SupportedVersionsBytes = Payload.RightChop(ServerId.Num() + sizeof(ControlPort) + sizeof(ConnectionStateEnum));
	TArray<uint16> SupportedVersions;
	SupportedVersions.Append((uint16*)SupportedVersionsBytes.GetData(), SupportedVersionsBytes.Num() / 2);

    return FDiscoveryNotify(MoveTemp(ServerId), ControlPort, ConnectionStateEnum, MoveTemp(SupportedVersions));
}

TProtocolResult<FDiscoveryPacket> FDiscoveryNotify::Serialize(const FDiscoveryNotify& InNotify)
{
    TArray<uint8> Payload;
    Payload.Reserve(InNotify.ServerId.Num() + sizeof(InNotify.ControlPort) + sizeof(InNotify.ConnectionState) + InNotify.SupportedVersions.Num() * sizeof(uint16));

    Payload.Append(InNotify.ServerId.GetData(), InNotify.ServerId.Num());
    Payload.Append((uint8*) &InNotify.ControlPort, sizeof(InNotify.ControlPort));

    uint8 ConnectionState = FromConnectionState(InNotify.ConnectionState);

    if (ConnectionState == MAX_uint8)
    {
        return FCaptureProtocolError(TEXT("Invalid connection state field"));
    }

    Payload.Add(ConnectionState);
	Payload.Append((uint8*) InNotify.SupportedVersions.GetData(), InNotify.SupportedVersions.Num() * sizeof(uint16));

    return FDiscoveryPacket(FDiscoveryPacket::EMessageType::Notify, MoveTemp(Payload));
}

const FDiscoveryNotify::FServerId& FDiscoveryNotify::GetServerId() const
{
    return ServerId;
}

uint16 FDiscoveryNotify::GetControlPort() const
{
    return ControlPort;
}

FDiscoveryNotify::EConnectionState FDiscoveryNotify::GetConnectionState() const
{
    return ConnectionState;
}

const TArray<uint16>& FDiscoveryNotify::GetSupportedVersions() const
{
	return SupportedVersions;
}

FDiscoveryNotify::EConnectionState FDiscoveryNotify::ToConnectionState(uint8 InConnectionState)
{
    if (InConnectionState == 0)
    {
        return EConnectionState::Offline;
    }
    else if (InConnectionState == 1)
    {
        return EConnectionState::Online;
    }
    else
    {
        return EConnectionState::Invalid;
    }
}

uint8 FDiscoveryNotify::FromConnectionState(EConnectionState InConnectionState)
{
    switch (InConnectionState)
    {
        case EConnectionState::Offline:
            return 0;
        case EConnectionState::Online:
            return 1;
        case EConnectionState::Invalid:
        default:
            return MAX_uint8;
    }
}