// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlPacket.h"

#include "Communication/TcpClient.h"

#include "Utility/QueueRunner.h"
#include "Utility/Error.h"

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

class FControlCommunication final
{
public:

    DECLARE_DELEGATE_OneParam(FOnPacketReceived, FControlPacket InPacket)

    using FRunnerType = TQueueRunner<FControlPacket>;

    FControlCommunication();

	TProtocolResult<void> Init();
	TProtocolResult<void> Start(const FString& InServerIp, const uint16 InServerPort);
	TProtocolResult<void> Stop();

	bool IsRunning() const;

    void SendMessage(FControlPacket InMessage);

    void SetReceiveHandler(FOnPacketReceived InReceiveHandler);

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

		FControlCommunication& Communication;
		bool bIsRunning;
		FEvent* DoneEvent;
	};


    void OnProcessReceivedPacket(FControlPacket InMessage);
    void OnProcessSentPacket(FControlPacket InMessage);
	void RunnableHandler(TSharedPtr<FRunnable> InRunnable);

	TProtocolResult<FControlPacketHeader> ReceiveControlHeader();
	TProtocolResult<void> ReceiveControlPacket(const FControlPacketHeader& InHeader);

    FTcpClient Client;
    FOnPacketReceived ReceiveHandler;

    FRunnerType SynchronizedReceiver;
    FRunnerType SynchronizedSender;
	TQueueRunner<TSharedPtr<FRunnable>> SynchronizedRunnable;
	TWeakPtr<FCommunicationRunnable> Runnable;
};