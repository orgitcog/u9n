// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Communication/ControlCommunication.h"

#include "Utility/Definitions.h"
#include "HAL/Event.h"

DEFINE_LOG_CATEGORY(LogCPSControlCommunication)

FControlCommunication::FControlCommunication()
    : ReceiveHandler(nullptr)
    , SynchronizedReceiver(FRunnerType::FOnProcess::CreateRaw(this, &FControlCommunication::OnProcessReceivedPacket))
    , SynchronizedSender(FRunnerType::FOnProcess::CreateRaw(this, &FControlCommunication::OnProcessSentPacket))
	, SynchronizedRunnable(TQueueRunner<TSharedPtr<FRunnable>>::FOnProcess::CreateRaw(this, &FControlCommunication::RunnableHandler))
{
}

TProtocolResult<void> FControlCommunication::Init()
{
    return Client.Init();
}

TProtocolResult<void> FControlCommunication::Start(const FString& InServerIp, const uint16 InServerPort)
{
	TProtocolResult<void> Result = Client.Start(InServerIp + TEXT(":") + FString::FromInt(InServerPort));
	if (Result.IsValid())
	{
		TSharedPtr<FCommunicationRunnable> CommunicationRunnable = MakeShared<FCommunicationRunnable>(*this);
		Runnable = CommunicationRunnable;

		SynchronizedRunnable.Add(CommunicationRunnable);
	}

    return Result;
}

TProtocolResult<void> FControlCommunication::Stop()
{
	if (TSharedPtr<FCommunicationRunnable> CommunicationRunnable = Runnable.Pin(); CommunicationRunnable)
	{
		CommunicationRunnable->Stop();
		CommunicationRunnable->Join();
		Runnable.Reset();
	}

    return Client.Stop();
}

bool FControlCommunication::IsRunning() const
{
	return Client.IsRunning();
}

void FControlCommunication::SendMessage(FControlPacket InMessage)
{
    SynchronizedSender.Add(MoveTemp(InMessage));
}

void FControlCommunication::SetReceiveHandler(FOnPacketReceived InReceiveHandler)
{
    ReceiveHandler = MoveTemp(InReceiveHandler);
}

void FControlCommunication::OnProcessReceivedPacket(FControlPacket InMessage)
{
    ReceiveHandler.ExecuteIfBound(MoveTemp(InMessage));
}

void FControlCommunication::OnProcessSentPacket(FControlPacket InMessage)
{
    FTcpClientWriter Writer(Client);

    TProtocolResult<void> SerializeResult = FControlPacket::Serialize(InMessage, Writer);
    if (SerializeResult.IsError())
    {
        UE_LOG(LogCPSControlCommunication, Error, TEXT("Invalid message: %s"), *(SerializeResult.ClaimError().GetMessage()));
        return;
    }
}

void FControlCommunication::RunnableHandler(TSharedPtr<FRunnable> InRunnable)
{
	InRunnable->Run();
}

TProtocolResult<FControlPacketHeader> FControlCommunication::ReceiveControlHeader()
{
    FTcpClientReader Reader(Client);

    return FControlPacketHeader::Deserialize(Reader);
}

TProtocolResult<void> FControlCommunication::ReceiveControlPacket(const FControlPacketHeader& InHeader)
{
    FTcpClientReader Reader(Client);

    TProtocolResult<FControlPacket> PacketDeserialize = FControlPacket::Deserialize(InHeader, Reader);
    if (PacketDeserialize.IsError())
    {
        return PacketDeserialize.ClaimError();
    }

    SynchronizedReceiver.Add(PacketDeserialize.ClaimResult());

    return ResultOk;
}

FControlCommunication::FCommunicationRunnable::FCommunicationRunnable(FControlCommunication& InCommunication)
	: Communication(InCommunication)
	, bIsRunning(true)
{
	DoneEvent = FGenericPlatformProcess::GetSynchEventFromPool(false);
	check(DoneEvent);
}

FControlCommunication::FCommunicationRunnable::~FCommunicationRunnable()
{
	FGenericPlatformProcess::ReturnSynchEventToPool(DoneEvent);
}

uint32 FControlCommunication::FCommunicationRunnable::Run()
{
	while (bIsRunning)
	{
		TProtocolResult<FControlPacketHeader> ReceiveHeaderResult = Communication.ReceiveControlHeader();
		if (ReceiveHeaderResult.IsError())
		{
			FCaptureProtocolError Error = ReceiveHeaderResult.ClaimError();
			if (Error.GetCode() == FTcpClient::DisconnectedError)
			{
				Stop();
			}

			continue;
		}

		TProtocolResult<void> ReceivePacketResult = Communication.ReceiveControlPacket(ReceiveHeaderResult.ClaimResult());
		if (ReceivePacketResult.IsError())
		{
			FCaptureProtocolError Error = ReceivePacketResult.ClaimError();
			if (Error.GetCode() == FTcpClient::DisconnectedError)
			{
				Stop();
			}
		}
	}

	DoneEvent->Trigger();

	return 0;
}

void FControlCommunication::FCommunicationRunnable::Stop()
{
	bIsRunning = false;
}

void FControlCommunication::FCommunicationRunnable::Join()
{
	DoneEvent->Wait();
}