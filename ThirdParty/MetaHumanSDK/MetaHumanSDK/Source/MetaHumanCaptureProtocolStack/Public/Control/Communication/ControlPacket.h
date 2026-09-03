// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Communication/TcpReaderWriter.h"

#include "Utility/Error.h"

#include "Containers/Array.h"

#include "Dom/JsonObject.h"

class FControlPacketHeader final
{
public:

    static const TArray<uint8> Header;

    static TProtocolResult<FControlPacketHeader> Deserialize(ITcpSocketReader& InReader);
    static uint32 GetHeaderSize();

    FControlPacketHeader();
	FControlPacketHeader(uint16 InVersion, uint32 InPayloadSize);

	uint16 GetVersion() const;
    uint32 GetPayloadSize() const;

private:
	
	uint16 Version;
    uint32 PayloadSize;
};

class FControlPacket final
{
public:

    static TProtocolResult<FControlPacket> Deserialize(const FControlPacketHeader& InPacketHeader, ITcpSocketReader& InReader);
    static TProtocolResult<void> Serialize(const FControlPacket& InMessage, ITcpSocketWriter& InWriter);

    FControlPacket();
    FControlPacket(uint16 InVersion, TArray<uint8> InPayload);

    FControlPacket(const FControlPacket& InOther) = default;
    FControlPacket(FControlPacket&& InOther) = default;

    FControlPacket& operator=(const FControlPacket& InOther) = default;
    FControlPacket& operator=(FControlPacket&& InOther) = default;

	uint16 GetVersion() const;
    const TArray<uint8>& GetPayload() const;
    uint32 GetPayloadSize() const;

private:

	FControlPacketHeader Header;
    TArray<uint8> Payload;
};
