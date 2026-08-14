// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MessageEndpointBuilder.h"
#include "MessageEndpoint.h"

#include "Messenger.h"

#include "LiveLinkHubCaptureMessages.h"

class LIVELINKHUBCAPTUREMESSAGING_API FUploadStateHandler : public FFeatureBase
{
public:

	DECLARE_DELEGATE_ThreeParams(FUploadStateCallback, const FGuid& InCaptureSourceId, const FGuid& InTakeUploadId, double InProgress);
	DECLARE_DELEGATE_FourParams(FUploadFinishedCallback, const FGuid& InCaptureSourceId, const FGuid& InTakeUploadId, FString InMessage, int32 InCode);

	FUploadStateHandler();

	void SetUploadCallbacks(FUploadStateCallback InStateCallback,
							FUploadFinishedCallback InFinishedCallback);

protected:

	void Initialize(FMessageEndpointBuilder& InBuilder);

private:

	using FFeatureBase::Endpoint;
	using FFeatureBase::Address;

	void HandleUploadStateMessage(const FUploadState& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleUploadFinishedMessage(const FUploadFinished& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);

	FUploadStateCallback StateCallback;
	FUploadFinishedCallback FinishedCallback;
};