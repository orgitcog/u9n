// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TcpReaderWriter.h"

#include "Network/Error.h"

#include "Runtime/Sockets/Public/IPAddress.h"

#include "Common/TcpSocketBuilder.h"

#include "EngineLogs.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"

#include "Containers/UnrealString.h"

#include "Templates/UniquePtr.h"

#include "Misc/DateTime.h"

namespace UE::CaptureManager
{

using FSocketPtr = TUniquePtr<FSocket, FSocketDeleter>;

class CAPTUREUTILS_API FTcpClient final
{
public:

	static constexpr uint32 BufferSize = 2 * 1024 * 1024;
	static constexpr int32 DisconnectedError = -10;
	static constexpr int32 ReadError = -3;
	static constexpr int32 NoPendingDataError = -2;
	static constexpr int32 TimeoutError = -1;

	FTcpClient();
	~FTcpClient();

	TProtocolResult<void> Init();
	// Blocking function until connection is established
	TProtocolResult<void> Start(const FString& InServerAddress);
	TProtocolResult<void> Stop();

	bool IsRunning() const;

	TProtocolResult<void> SendMessage(const TArray<uint8>& InPayload);
	TProtocolResult<TArray<uint8>> ReceiveMessage(const uint64 InSize, const uint32 InWaitTimeoutMs = ITcpSocketReader::DefaultWaitTimeoutMs);

private:

	FSocketPtr TcpSocket;

	bool bRunning;
};

class CAPTUREUTILS_API FTcpClientReader final : public ITcpSocketReader
{
public:
	FTcpClientReader(FTcpClient& InClient);

	virtual TProtocolResult<TArray<uint8>> ReceiveMessage(const uint64 InSize, const uint32 InWaitTimeoutMs = DefaultWaitTimeoutMs) override;

private:
	FTcpClient& Client;
};

class CAPTUREUTILS_API FTcpClientWriter final : public ITcpSocketWriter
{
public:
	FTcpClientWriter(FTcpClient& InClient);

	virtual TProtocolResult<void> SendMessage(const TArray<uint8>& InPayload) override;

private:

	FTcpClient& Client;
};

}