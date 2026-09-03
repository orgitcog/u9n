// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MessageEndpointBuilder.h"
#include "MessageEndpoint.h"

#include "Messenger.h"

#include "Async/CaptureTimerManager.h"

#include "LiveLinkHubCaptureMessages.h"

class LIVELINKHUBCAPTUREMESSAGING_API FConnectStarter : 
	public FFeatureBase, 
	public TSharedFromThis<FConnectStarter>
{
public:

	DECLARE_DELEGATE_OneParam(FConnectHandler, const FConnectResponse& InResponse);
	DECLARE_DELEGATE(FDisconnectHandler);

	FConnectStarter();
	virtual ~FConnectStarter() override;

	void Connect(FConnectHandler InConnectHandler);
	void Disconnect();
	void SetDisconnectHandler(FDisconnectHandler InHandler);

	bool IsConnected() const;

protected:

	void Initialize(FMessageEndpointBuilder& InBuilder);

private:

	static const float KeepAliveInterval;
	static const float KeepAliveTimeout;

	DECLARE_DELEGATE_OneParam(FPingCallback, const FPongMessage& InResponse);
	FGuid SendPingRequest(FPingCallback InCallback);

	void HandleConnectResponse(const FConnectResponse& InResponse, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePingResponse(const FPongMessage& InResponse, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void AddContext(FGuid InGuid, FConnectHandler InHandler);

	void OnKeepAliveInterval();

	using FFeatureBase::Endpoint;
	using FFeatureBase::Address;

	FCriticalSection Mutex;
	TMap<FGuid, FConnectHandler> Contexts;

	FCriticalSection PingMutex;
	TMap<FGuid, FPingCallback> PingContexts;

	TSharedRef<UE::CaptureManager::FCaptureTimerManager> TimerManager;
	UE::CaptureManager::FCaptureTimerManager::FTimerHandle KeepAlive;
	FDisconnectHandler Handler;

	std::atomic_bool bConnected = false;
};