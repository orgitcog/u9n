// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkHubCaptureMessages.h"

#include "MessageEndpointBuilder.h"
#include "MessageEndpoint.h"

#include "Async/Future.h"

#include "Templates/ValueOrError.h"


class LIVELINKHUBCAPTUREMESSAGING_API FFeatureBase
{
protected:

	virtual ~FFeatureBase() = default;

	virtual void Initialize(FMessageEndpointBuilder& InBuilder) = 0;

	void SetEndpoint(TSharedPtr<FMessageEndpoint> InEndpoint);
	void SetAddress(const FMessageAddress& InAddress);

	TSharedPtr<FMessageEndpoint> Endpoint;
	FMessageAddress Address;
};

template<class ... Features>
class FMessenger
	: public Features...
{
public:

	inline static const FString Name = TEXT("Messenger");

	FMessenger()
		: Builder(*Name)
	{
		Builder.ReceivingOnAnyThread();

		(Features::Initialize(Builder), ...);

		Endpoint = Builder.Build();

		(Features::SetEndpoint(Endpoint), ...);
	}

	~FMessenger()
	{
		FMessageEndpoint::SafeRelease(Endpoint);
	}

	void SetAddress(FMessageAddress InAddress)
	{
		Address = MoveTemp(InAddress);

		(Features::SetAddress(Address), ...);
	}

	FMessageAddress GetAddress() const
	{
		return Address;
	}

	FMessageAddress GetOwnAddress() const
	{
		return Endpoint->GetAddress();
	}

	void SendDiscoveryResponse(FDiscoveryResponse* InResponse, FMessageAddress InReceiver)
	{
		Endpoint->Send(InResponse, InReceiver);
	}

private:

	FMessageEndpointBuilder Builder;
	TSharedPtr<FMessageEndpoint> Endpoint;
	FMessageAddress Address;
};
