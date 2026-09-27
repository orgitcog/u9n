// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Discovery/Communication/DiscoveryPacket.h"

#include "Utility/Error.h"

#include "Containers/Array.h"
#include "Containers/StaticArray.h"

class METAHUMANCAPTUREPROTOCOLSTACK_API FDiscoveryResponse
{
public:

	static const uint32 MinPayloadSize;

    using FServerId = TStaticArray<uint8, 16>;

    FDiscoveryResponse(FServerId InServerId, uint16 InControlPort, TArray<uint16> InSupportedVersions);

    static TProtocolResult<FDiscoveryResponse> Deserialize(const FDiscoveryPacket& InPacket);
    static TProtocolResult<FDiscoveryPacket> Serialize(const FDiscoveryResponse& InResponse);

    const FServerId& GetServerId() const;
    uint16 GetControlPort() const;
	const TArray<uint16>& GetSupportedVersions() const;

private:

    FServerId ServerId;
    uint16 ControlPort;
	TArray<uint16> SupportedVersions;
};