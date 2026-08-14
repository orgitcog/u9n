// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Communication/TcpReaderWriter.h"

#include "Utility/Error.h"

#include "Common/TcpSocketBuilder.h"
#include "Common/TcpListener.h"

#include "EngineLogs.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Async/Async.h"

#include "Containers/UnrealString.h"

#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

#include "Delegates/Delegate.h"

using FSocketPtr = TUniquePtr<FSocket, FSocketDeleter>;

class FTcpClientHandler final
{
public:

    static constexpr uint32 MaxBufferSize = 500 * 1024;
    static constexpr int32 DisconnectedError = -10;
	static constexpr int32 TimeoutError = -1;

    FTcpClientHandler(FSocketPtr InSocket, FString InEndpoint);
    ~FTcpClientHandler();

	TProtocolResult<void> SendMessage(const TArray<uint8>& InData);
	TProtocolResult<TArray<uint8>> ReceiveMessage(const uint64 InSize, const uint32 InWaitTimeoutMs = ITcpSocketReader::DefaultWaitTimeoutMs);

    const FString& GetEndpoint() const;

    bool operator==(const FTcpClientHandler& InOther);

private:

    FSocketPtr Socket;
    FString Endpoint;
};

class FTcpServer
{
public:

    static constexpr uint32 ThreadWaitTime = 500; // Milliseconds
	static constexpr uint16 AnyPort = 0; // OS assigned port

    DECLARE_DELEGATE_TwoParams(FConnectionHandler, TWeakPtr<FTcpClientHandler> InClient, bool bConnected)

    FTcpServer(const uint32 InMaxNumberOfClients);

	TProtocolResult<uint16> Start(const uint16 InListenPort = AnyPort);
	TProtocolResult<void> Stop();

	TProtocolResult<void> SendMessage(const TArray<uint8>& InMessage, const FString& InEndpoint);

    void DisconnectClient(const FString& InEndpoint);
    void DisconnectClientAsync(const FString& InEndpoint);
    void SetConnectionHandler(FConnectionHandler InOnConnectionHandler);

	int32 GetPort() const;

private:

    TUniquePtr<FTcpListener> Listener;
	FSocketPtr Socket;

    uint32 MaxNumberOfClients;
    TMap<FString, TSharedPtr<FTcpClientHandler>> Clients;

    FConnectionHandler OnConnectionHandler;
    FRWLock Mutex;

    bool bRunning;
};

class FTcpConnectionReader final : public ITcpSocketReader
{
public:
    FTcpConnectionReader(FTcpClientHandler& InClient);

    virtual TProtocolResult<TArray<uint8>> ReceiveMessage(const uint64 InSize, const uint32 InWaitTimeoutMs = DefaultWaitTimeoutMs) override;

private:
    FTcpClientHandler& Client;
};

class FTcpConnectionWriter final : public ITcpSocketWriter
{
public:
    FTcpConnectionWriter(FTcpClientHandler& InClient);

    virtual TProtocolResult<void> SendMessage(const TArray<uint8>& InPayload) override;

private:

    FTcpClientHandler& Client;
};
