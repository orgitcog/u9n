// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MessageEndpointBuilder.h"
#include "MessageEndpoint.h"

#include "LiveLinkHubCaptureMessages.h"

#include "Messenger.h"

#include "Async/CaptureTimerManager.h"

class LIVELINKHUBCAPTUREMESSAGING_API FConnectAcceptor : public FFeatureBase
{
public:

	DECLARE_DELEGATE_RetVal_TwoParams(FConnectResponse*, FConnectAccepted, const FConnectRequest& InRequest, const FMessageAddress& InAddress);
	DECLARE_DELEGATE_OneParam(FConnectionLostHandler, const FMessageAddress& InAddress);

	FConnectAcceptor();
	virtual ~FConnectAcceptor() override;

	void SetConnectionHandler(FConnectAccepted InConnectHandler,
							  FConnectionLostHandler InConnectionLostHandler);
	void Disconnect();
	bool IsConnected() const;

protected:

	void Initialize(FMessageEndpointBuilder& InBuilder);

private:

	void ConnectRequestHandler(const FConnectRequest& InRequest, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleKeepAlive(const FPingMessage& InRequest, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);

	void CheckConnectionActivity();

	double LastPingRequest = 0.0f;
	double InactivityTimeout = 20.0f; // seconds

	FConnectAccepted ConnectHandler;
	FConnectionLostHandler ConnectionLostHandler;

	using FFeatureBase::Endpoint;
	using FFeatureBase::Address;

	TSharedRef<UE::CaptureManager::FCaptureTimerManager> TimerManager;
	UE::CaptureManager::FCaptureTimerManager::FTimerHandle ClientActivity;

	std::atomic_bool bConnected = false;
};