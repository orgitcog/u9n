// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Communication/ControlCommunication.h"

#include "Utility/Definitions.h"

namespace UE::CaptureManager
{

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
	if (Result.HasValue())
	{
		TSharedPtr<FCommunicationRunnable> CommunicationRunnable = MakeShared<FCommunicationRunnable>(*this);
		Runnable = CommunicationRunnable;

		SynchronizedRunnable.Add(CommunicationRunnable);
	}

	return Result;
}

void FControlCommunication::Stop()
{
	Client.Stop();
	
	if (TSharedPtr<FCommunicationRunnable> CommunicationRunnable = Runnable.Pin(); CommunicationRunnable)
	{
		CommunicationRunnable->Stop();
		CommunicationRunnable->Join();
	}
}

void FControlCommunication::CommunicationRunnableStopped()
{
	Runnable.Reset();
	CommunicationStoppedHandler.ExecuteIfBound();
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

void FControlCommunication::SetCommunicationStoppedHandler(FCommunicationStoppedHandler InCommunicationStoppedHandler)
{
	CommunicationStoppedHandler = MoveTemp(InCommunicationStoppedHandler);
}

void FControlCommunication::OnProcessReceivedPacket(FControlPacket InMessage)
{
	ReceiveHandler.ExecuteIfBound(MoveTemp(InMessage));
}

void FControlCommunication::OnProcessSentPacket(FControlPacket InMessage)
{
	FTcpClientWriter Writer(Client);

	TProtocolResult<void> SerializeResult = FControlPacket::Serialize(InMessage, Writer);
	if (SerializeResult.HasError())
	{
		FCaptureProtocolError Error = SerializeResult.StealError();
		UE_LOG(LogCPSControlCommunication, Error, TEXT("Failed to serialize message to tcp writer: '%s' code: %d"), *Error.GetMessage(), Error.GetCode());
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
	if (PacketDeserialize.HasError())
	{
		return PacketDeserialize.StealError();
	}

	SynchronizedReceiver.Add(PacketDeserialize.StealValue());

	return ResultOk;
}

FControlCommunication::FCommunicationRunnable::FCommunicationRunnable(FControlCommunication& InCommunication)
	: Communication(InCommunication)
	, bIsRunning(true)
{
}

FControlCommunication::FCommunicationRunnable::~FCommunicationRunnable() = default;

uint32 FControlCommunication::FCommunicationRunnable::Run()
{
	while (bIsRunning)
	{
		TProtocolResult<FControlPacketHeader> ReceiveHeaderResult = Communication.ReceiveControlHeader();
		if (ReceiveHeaderResult.HasError())
		{
			HandleError(ReceiveHeaderResult.StealError());
			continue;
		}

		TProtocolResult<void> ReceivePacketResult = Communication.ReceiveControlPacket(ReceiveHeaderResult.StealValue());
		if (ReceivePacketResult.HasError())
		{
			HandleError(ReceivePacketResult.StealError());
		}
	}

	DoneEvent->Trigger();

	Communication.CommunicationRunnableStopped();

	return 0;
}

void FControlCommunication::FCommunicationRunnable::Stop()
{
	UE_LOG(LogCPSControlCommunication, Verbose, TEXT("Stopping FCommunicationRunnable"))
	bIsRunning = false;
}

void FControlCommunication::FCommunicationRunnable::Join()
{
	DoneEvent->Wait();
}

void FControlCommunication::FCommunicationRunnable::HandleError(const FCaptureProtocolError& Error)
{
	switch (Error.GetCode())
	{
	case FTcpClient::DisconnectedError:
	case FTcpClient::NoPendingDataError:
	case FTcpClient::ReadError:
		UE_LOG(LogCPSControlCommunication, Verbose, TEXT("Unrecoverable FTcpClient error occurred when receiving control packet header: '%s' Code: %d."), *Error.GetMessage(), Error.GetCode())
		Stop();
	default:
		UE_LOG(LogCPSControlCommunication, Verbose, TEXT("Unhandled FTcpClient error occurred when receiving control packet header: '%s' Code: %d"), *Error.GetMessage(), Error.GetCode())
		break;
	}
}
	
}
