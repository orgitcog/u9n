// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Discovery/Communication/DiscoveryPacket.h"

#include "Utility/Error.h"

#include "Containers/Array.h"
#include "Containers/StaticArray.h"

namespace UE::CaptureManager
{

class CAPTUREPROTOCOLSTACK_API FDiscoveryResponse
{
public:

	static const uint32 MinPayloadSize;

	using FServerId = TStaticArray<uint8, 16>;

	UE_DEPRECATED(5.6, "This constructor is no longer supported")
	FDiscoveryResponse(FServerId InServerId, uint16 InControlPort, TArray<uint16> InSupportedVersions);
	
	FDiscoveryResponse(FServerId InServerId, FString InServerName, uint16 InControlPort);

	static TProtocolResult<FDiscoveryResponse> Deserialize(const FDiscoveryPacket& InPacket);
	static TProtocolResult<FDiscoveryPacket> Serialize(const FDiscoveryResponse& InResponse);

	const FServerId& GetServerId() const;
	const FString& GetServerName() const;
	uint16 GetControlPort() const;
	
	UE_DEPRECATED(5.6, "GetSupportedVersions is no longer supported")
	const TArray<uint16>& GetSupportedVersions() const;

private:

	FServerId ServerId;
	FString ServerName;
	uint16 ControlPort;

	// Deprecated in 5.6
	TArray<uint16> SupportedVersions;
};

}