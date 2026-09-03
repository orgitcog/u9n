// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlPacket.h"

#include "Network/TcpClient.h"

#include "Async/QueueRunner.h"
#include "Utility/Error.h"

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"

namespace UE::CaptureManager
{

class FControlCommunication final
{
public:

	DECLARE_DELEGATE_OneParam(FOnPacketReceived, FControlPacket InPacket)
	DECLARE_DELEGATE(FCommunicationStoppedHandler)

	using FRunnerType = TQueueRunner<FControlPacket>;

	FControlCommunication();

	TProtocolResult<void> Init();
	TProtocolResult<void> Start(const FString& InServerIp, const uint16 InServerPort);
	void Stop();

	bool IsRunning() const;

	void SendMessage(FControlPacket InMessage);

	void SetReceiveHandler(FOnPacketReceived InReceiveHandler);
	void SetCommunicationStoppedHandler(FCommunicationStoppedHandler InConnectionStateHandler);

private:

	class FCommunicationRunnable :
		public FRunnable
	{
	public:
		FCommunicationRunnable(FControlCommunication& InCommunication);
		virtual ~FCommunicationRunnable();

		uint32 Run() override;
		void Stop() override;

		void Join();

	private:

		void HandleError(const FCaptureProtocolError& Error);

		FControlCommunication& Communication;
		bool bIsRunning;
		FEventRef DoneEvent;
	};


	void OnProcessReceivedPacket(FControlPacket InMessage);
	void OnProcessSentPacket(FControlPacket InMessage);
	void RunnableHandler(TSharedPtr<FRunnable> InRunnable);
	void CommunicationRunnableStopped();

	TProtocolResult<FControlPacketHeader> ReceiveControlHeader();
	TProtocolResult<void> ReceiveControlPacket(const FControlPacketHeader& InHeader);

	FTcpClient Client;
	FOnPacketReceived ReceiveHandler;
	FCommunicationStoppedHandler CommunicationStoppedHandler;

	FRunnerType SynchronizedReceiver;
	FRunnerType SynchronizedSender;
	TQueueRunner<TSharedPtr<FRunnable>> SynchronizedRunnable;
	TWeakPtr<FCommunicationRunnable> Runnable;
};

}