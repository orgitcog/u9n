// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utility/Error.h"

#include "Containers/Array.h"

class ITcpSocketReader
{
public:
	static constexpr uint32 DefaultWaitTimeoutMs = 1000;

    virtual ~ITcpSocketReader() = default;

    virtual TProtocolResult<TArray<uint8>> ReceiveMessage(const uint64 InSize, const uint32 InWaitTimeoutMs = DefaultWaitTimeoutMs) = 0;
};

class ITcpSocketWriter
{
public:
    virtual ~ITcpSocketWriter() = default;

    virtual TProtocolResult<void> SendMessage(const TArray<uint8>& InPayload) = 0;
};