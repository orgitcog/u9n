// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MessageEndpointBuilder.h"
#include "MessageEndpoint.h"

#include "Messenger.h"

#include "LiveLinkHubCaptureMessages.h"

class LIVELINKHUBCAPTUREMESSAGING_API FUploadStateSender : public FFeatureBase
{
public:

	FUploadStateSender();

	void SendUploadStateMessage(const FGuid& InCaptureSourceId, const FGuid& InTakeUploadId, double InProgress);
	void SendUploadDoneMessage(const FGuid& InCaptureSourceId, const FGuid& InTakeUploadId, FString InMessage, int32 InCode);

protected:

	void Initialize(FMessageEndpointBuilder& InBuilder);

private:

	EStatus ConvertStatus(int32 InCode);

	using FFeatureBase::Endpoint;
	using FFeatureBase::Address;
};