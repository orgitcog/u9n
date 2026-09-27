// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Discovery/Communication/DiscoveryPacket.h"

#include "Utility/Error.h"

#include "Containers/Array.h"
#include "Containers/StaticArray.h"

class METAHUMANCAPTUREPROTOCOLSTACK_API FDiscoveryNotify
{
public:

	static const uint32 MinPayloadSize;

    using FServerId = TStaticArray<uint8, 16>;

    enum class EConnectionState : uint8
    {
        Offline = 0,
        Online = 1,

        Invalid
    };

    FDiscoveryNotify(FServerId InServerId, uint16 InControlPort, EConnectionState InConnectionState, TArray<uint16> InSupportedVersions);

    static TProtocolResult<FDiscoveryNotify> Deserialize(const FDiscoveryPacket& InPacket);
    static TProtocolResult<FDiscoveryPacket> Serialize(const FDiscoveryNotify& InNotify);

    const FServerId& GetServerId() const;
    uint16 GetControlPort() const;
    EConnectionState GetConnectionState() const;
	const TArray<uint16>& GetSupportedVersions() const;

private:

    static EConnectionState ToConnectionState(uint8 InConnectionState);
    static uint8 FromConnectionState(EConnectionState InConnectionState);

    FServerId ServerId;
    uint16 ControlPort;
    EConnectionState ConnectionState;
	TArray<uint16> SupportedVersions;
};