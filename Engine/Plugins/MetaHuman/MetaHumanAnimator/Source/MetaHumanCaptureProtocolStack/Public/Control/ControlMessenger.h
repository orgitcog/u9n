// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utility/Definitions.h"

#include "Control/Communication/ControlCommunication.h"
#include "Control/Communication/ControlPacket.h"

#include "Messages/ControlMessage.h"
#include "Messages/ControlRequest.h"
#include "Messages/ControlResponse.h"
#include "Messages/ControlUpdate.h"

#include "Utility/Error.h"

#include "Misc/ScopedEvent.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"

class FKeepAliveCounter
{
public:
	FKeepAliveCounter();

	void Increment();
	void Reset();
	bool HasReached(uint16 InBound);

private:

	std::atomic<uint16> Counter;
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FControlMessenger final
{
public:

    template<class RequestType>
    using FOnControlResponse = TDelegate<void(TProtocolResult<typename RequestType::ResponseType> InResult)>;

	DECLARE_DELEGATE_OneParam(FOnDisconnect, const FString& InCause);

    static constexpr int32 ResponseWaitTime = 3; // Seconds
    static constexpr int32 KeepAliveInterval = 5; // Seconds
    static const TCHAR* HandshakeSessionId;

    FControlMessenger();
    ~FControlMessenger();
    
    void RegisterUpdateHandler(FString InAddressPath, FControlUpdate::FOnUpdateMessage InUpdateHandler);
	void RegisterDisconnectHandler(FOnDisconnect InOnDisconnectHandler);

	TProtocolResult<void> Start(const FString& InServerIp, const uint16 InServerPort);
	TProtocolResult<void> Stop();

	TProtocolResult<void> StartSession();
    TProtocolResult<FGetServerInformationResponse> GetServerInformation();

    template<class RequestType>
	TProtocolResult<typename RequestType::ResponseType> SendRequest(RequestType InRequest)
    {
        FControlMessage Message(InRequest.GetAddressPath(), FControlMessage::EType::Request, InRequest.GetBody());

        uint32 TransactionId = GenerateTransactionId();

        {
            FScopeLock SessionLock(&SessionIdMutex);
            Message.SetSessionId(SessionId);
        }
        
        Message.SetTransactionId(TransactionId);
        Message.SetTimestamp(GetTimestamp());
        
		TProtocolResult<FControlPacket> SerializeResult = FControlMessage::Serialize(Message);

        if (SerializeResult.IsError())
        {
            return FCaptureProtocolError(TEXT("Failed to serialize a request."));
        }

        TPromise<TProtocolResult<FControlMessage>> Promise;
        TFuture<TProtocolResult<FControlMessage>> Future = Promise.GetFuture();

        {
            FScopeLock Lock(&RequestsMutex);
            RequestContexts.Emplace(TransactionId, MakeUnique<FRequestContext>(MoveTemp(Message), MoveTemp(Promise)));
        }

        SendPacket(SerializeResult.ClaimResult());

        if (!Future.WaitFor(FTimespan::FromSeconds(ResponseWaitTime)))
        {
            FScopeLock Lock(&RequestsMutex);

            const TUniquePtr<FRequestContext>& RequestContext = RequestContexts[TransactionId];

			RequestContext->Promise.SetValue(FCaptureProtocolError("Broken promise")); // Actual error to avoid the destructor error.

            RequestContexts.Remove(TransactionId);

            return FCaptureProtocolError(TEXT("Server failed to respond within 3 seconds."));
        }

        FScopeLock Lock(&RequestsMutex);
        const TUniquePtr<FRequestContext>& RequestContext = RequestContexts[TransactionId];

        const TProtocolResult<FControlMessage>& FutureResult = Future.Get();
        check(FutureResult.IsValid());

        const FControlMessage& ResponseMessage = FutureResult.GetResult();

        {
            FScopeLock SessionLock(&SessionIdMutex);
            if (ResponseMessage.GetSessionId() != SessionId)
            {
                return FCaptureProtocolError(TEXT("Invalid session ID arrived"));
            }
        }

        if (!ResponseMessage.GetErrorName().IsEmpty())
        {
            return FCaptureProtocolError(TEXT("Server respond with error: ") + ResponseMessage.GetErrorName());
        }

        typename RequestType::ResponseType Response;
        RequestContexts.Remove(TransactionId);

		TProtocolResult<void> ParseResult = Response.Parse(ResponseMessage.GetBody());
        if (ParseResult.IsError())
        {
            return FCaptureProtocolError(TEXT("Failed to parse the response: ") + ParseResult.ClaimError().GetMessage());
        }
        
        return Response;
    }

    template<class RequestType>
    void SendAsyncRequest(RequestType InRequest, FOnControlResponse<RequestType> InOnResponse)
    {
		AsyncRequestRunner.Add(FAsyncRequestDelegate::CreateLambda([this, Request(MoveTemp(InRequest)), OnResponse(MoveTemp(InOnResponse))]() mutable
		{
			TProtocolResult<typename RequestType::ResponseType> Result = SendRequest(MoveTemp(Request));

			OnResponse.ExecuteIfBound(MoveTemp(Result));
		}));
    }

private:
	DECLARE_DELEGATE(FAsyncRequestDelegate)

    struct FRequestContext
    {
        FRequestContext(FControlMessage InRequest, TPromise<TProtocolResult<FControlMessage>> InPromise)
            : Request(MoveTemp(InRequest))
            , Promise(MoveTemp(InPromise))
        {
        }

        FControlMessage Request;

        TPromise<TProtocolResult<FControlMessage>> Promise;
    };

    void SendPacket(FControlPacket InPacket);
    void KeepAlive();
    void MessageHandler(FControlPacket InPacket);

	uint32 GenerateTransactionId() const;
	uint64 GetTimestamp() const;

    void StartKeepAliveTimer();
    void StopKeepAliveTimer();

	void OnAsyncRequestProcess(FAsyncRequestDelegate InAsyncDelegate);

    FControlCommunication Communication;

    FCriticalSection SessionIdMutex;
    FString SessionId;

    FCriticalSection RequestsMutex;
    TMap<uint32, TUniquePtr<FRequestContext>> RequestContexts;

    FCriticalSection UpdatesMutex;
    TMap<FString, FControlUpdate::FOnUpdateMessage> UpdateHandlers;

	FTSTicker::FDelegateHandle KeepAliveTimer;
	FKeepAliveCounter KeepAliveFailures;

	TQueueRunner<FAsyncRequestDelegate> AsyncRequestRunner;
	FOnDisconnect OnDisconnectHandler;

	FRandomStream RandomStream;
};