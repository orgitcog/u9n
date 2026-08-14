// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Discovery/Communication/DiscoveryPacket.h"

#include "Utility/Error.h"

#include "Containers/Array.h"
#include "Containers/StaticArray.h"

namespace UE::CaptureManager
{

class CAPTUREPROTOCOLSTACK_API FDiscoveryNotify
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

	UE_DEPRECATED(5.6, "This constructor is no longer supported")
	FDiscoveryNotify(FServerId InServerId, uint16 InControlPort, EConnectionState InConnectionState, TArray<uint16> InSupportedVersions);
	
	FDiscoveryNotify(FServerId InServerId, FString InServerName, uint16 InControlPort, EConnectionState InConnectionState);

	static TProtocolResult<FDiscoveryNotify> Deserialize(const FDiscoveryPacket& InPacket);
	static TProtocolResult<FDiscoveryPacket> Serialize(const FDiscoveryNotify& InNotify);

	const FServerId& GetServerId() const;
	const FString& GetServerName() const;
	uint16 GetControlPort() const;
	EConnectionState GetConnectionState() const;
	
	UE_DEPRECATED(5.6, "GetSupportedVersions is no longer supported")
	const TArray<uint16>& GetSupportedVersions() const;

private:

	static EConnectionState ToConnectionState(uint8 InConnectionState);
	static uint8 FromConnectionState(EConnectionState InConnectionState);

	FServerId ServerId;
	FString ServerName;
	uint16 ControlPort;
	EConnectionState ConnectionState;

	// Deprecated in 5.6.
	TArray<uint16> SupportedVersions;
};

}