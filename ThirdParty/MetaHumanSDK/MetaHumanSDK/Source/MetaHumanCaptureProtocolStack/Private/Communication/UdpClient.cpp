// Copyright Epic Games, Inc. All Rights Reserved.

#include "Communication/UdpClient.h"

FUdpClient::FUdpClient() 
    : bRunning(false)
{
}

FUdpClient::~FUdpClient()
{
    Stop();
}

TProtocolResult<void> FUdpClient::Init(FUdpClientConfigure InConfig, FOnSocketDataReceived InReceiveHandler)
{
    if (bRunning)
    {
        return FCaptureProtocolError(TEXT("Can't initialize the client while running."));
    }

    FIPv4Endpoint Endpoint(FIPv4Address::Any, InConfig.ListenPort);

    // Prepare socket
    UdpSocket.Reset(FUdpSocketBuilder(TEXT("CPS UDP Socket"))
                    .AsNonBlocking()
                    .AsReusable()
                    .BoundToEndpoint(Endpoint)
                    .WithReceiveBufferSize(BufferSize)
                    .WithSendBufferSize(BufferSize)
                    .Build());

    if (!UdpSocket)
    {
        return FCaptureProtocolError(TEXT("Failed to create a client socket"));
    }

	if (!InConfig.MulticastIpAddress.IsEmpty())
	{
		FIPv4Endpoint MulticastEndpoint;
		FString MulticastHostAnDPort = FString::Format(TEXT("{0}:{1}"), { InConfig.MulticastIpAddress, InConfig.ListenPort });

		CHECK_BOOL(FIPv4Endpoint::FromHostAndPort(MulticastHostAnDPort, MulticastEndpoint));
        CHECK_BOOL(UdpSocket->JoinMulticastGroup(*MulticastEndpoint.ToInternetAddr()));
        CHECK_BOOL(UdpSocket->SetMulticastLoopback(true));
    }

    // Prepare receiver
    UdpReceiver.Reset(new FUdpSocketReceiver(UdpSocket.Get(), FTimespan::FromMilliseconds(ThreadWaitTime), TEXT("CPS RECEIVER-FUdpCommunication")));

    UdpReceiver->OnDataReceived() = MoveTemp(InReceiveHandler);

    return ResultOk;
}

TProtocolResult<void> FUdpClient::Start()
{
    if (bRunning)
    {
        return FCaptureProtocolError(TEXT("The client is already started"));
    }

    UdpReceiver->Start();

    bRunning = true;
        
    return ResultOk;
}

TProtocolResult<void> FUdpClient::Stop()
{
    if (!bRunning)
    {
        return FCaptureProtocolError(TEXT("The client is already stopped"));
    }

    UdpReceiver->Stop();
    UdpReceiver = nullptr;

    UdpSocket->Close();
    UdpSocket = nullptr;

    bRunning = false;

    return ResultOk;
}

TProtocolResult<int32> FUdpClient::SendMessage(const TArray<uint8>& InPayload, const FString& InEndpoint)
{
    if (!UdpSocket)
    {
        return FCaptureProtocolError(TEXT("Udp socket not configured"));
    }

    FIPv4Endpoint Endpoint;
    FIPv4Endpoint::FromHostAndPort(InEndpoint, Endpoint);

    TSharedRef<FInternetAddr> Address = Endpoint.ToInternetAddr();

    int32 Sent = 0;
    bool Result = UdpSocket->SendTo(InPayload.GetData(), InPayload.Num(), Sent, *Address);
    if (!Result)
    {
        return FCaptureProtocolError(TEXT("Failed to send the data"));
    }

    return Sent;
}

