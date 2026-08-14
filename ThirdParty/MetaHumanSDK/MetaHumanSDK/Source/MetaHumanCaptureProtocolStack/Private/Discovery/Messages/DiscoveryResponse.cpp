// Copyright Epic Games, Inc. All Rights Reserved.

#include "Discovery/Messages/DiscoveryResponse.h"
#include "Containers/ArrayView.h"

const uint32 FDiscoveryResponse::MinPayloadSize = 16 + 2 + 2; // Server Id (16) + Control Port (2) + Supported Versions (at least 1)
FDiscoveryResponse::FDiscoveryResponse(FServerId InServerId, uint16 InControlPort, TArray<uint16> InSupportedVersions)
    : ServerId(MoveTemp(InServerId))
    , ControlPort(InControlPort)
	, SupportedVersions(MoveTemp(InSupportedVersions))
{
}

TProtocolResult<FDiscoveryResponse> FDiscoveryResponse::Deserialize(const FDiscoveryPacket& InPacket)
{
	if (InPacket.GetMessageType() != FDiscoveryPacket::EMessageType::Response)
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

	TConstArrayView<uint8> SupportedVersionsBytes = Payload.RightChop(ServerId.Num() + sizeof(ControlPort));
	TArray<uint16> SupportedVersions;
	SupportedVersions.Append((uint16*) SupportedVersionsBytes.GetData(), SupportedVersionsBytes.Num() / 2);
	
    return FDiscoveryResponse(MoveTemp(ServerId), ControlPort, MoveTemp(SupportedVersions));
}

TProtocolResult<FDiscoveryPacket> FDiscoveryResponse::Serialize(const FDiscoveryResponse& InResponse)
{
    TArray<uint8> Payload;
    Payload.Reserve(InResponse.ServerId.Num() + sizeof(InResponse.ControlPort) + InResponse.SupportedVersions.Num() * sizeof(uint16));

    Payload.Append(InResponse.ServerId.GetData(), InResponse.ServerId.Num());
    Payload.Append((uint8*) &InResponse.ControlPort, sizeof(InResponse.ControlPort));
	Payload.Append((uint8*) InResponse.SupportedVersions.GetData(), InResponse.SupportedVersions.Num() * sizeof(uint16));

    return FDiscoveryPacket(FDiscoveryPacket::EMessageType::Response, MoveTemp(Payload));
}

const FDiscoveryResponse::FServerId& FDiscoveryResponse::GetServerId() const
{
    return ServerId;
}

uint16 FDiscoveryResponse::GetControlPort() const
{
    return ControlPort;
}

const TArray<uint16>& FDiscoveryResponse::GetSupportedVersions() const
{
	return SupportedVersions;
}