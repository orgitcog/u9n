// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlResponse.h"
#include "Misc/Optional.h"

class METAHUMANCAPTUREPROTOCOLSTACK_API FControlRequest
{
public:

    FControlRequest(FString InAddressPath);
    virtual ~FControlRequest() = default;

    const FString& GetAddressPath() const;
    virtual TSharedPtr<FJsonObject> GetBody() const;

private:

    FString AddressPath;
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FKeepAliveRequest final : public FControlRequest
{
public:

    using ResponseType = FKeepAliveResponse;

    FKeepAliveRequest();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FStartSessionRequest final : public FControlRequest
{
public:

    using ResponseType = FStartSessionResponse;

    FStartSessionRequest();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FStopSessionRequest final : public FControlRequest
{
public:

    using ResponseType = FStopSessionResponse;

    FStopSessionRequest();
};


class METAHUMANCAPTUREPROTOCOLSTACK_API FGetServerInformationRequest final : public FControlRequest
{
public:

    using ResponseType = FGetServerInformationResponse;

    FGetServerInformationRequest();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FSubscribeRequest final : public FControlRequest
{
public:

    using ResponseType = FSubscribeResponse;

    FSubscribeRequest();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FUnsubscribeRequest final : public FControlRequest
{
public:

    using ResponseType = FUnsubscribeResponse;

    FUnsubscribeRequest();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FGetStateRequest final : public FControlRequest
{
public:

    using ResponseType = FGetStateResponse;

    FGetStateRequest();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FStartRecordingTakeRequest final : public FControlRequest
{
public:

    using ResponseType = FStartRecordingTakeResponse;

    FStartRecordingTakeRequest(FString InSlateName, 
                               uint16 InTakeNumber, 
                               TOptional<FString> InSubject = TOptional<FString>(),
							   TOptional<FString> InScenario = TOptional<FString>(),
                               TOptional<TArray<FString>> InTags = TOptional<TArray<FString>>());

    virtual TSharedPtr<FJsonObject> GetBody() const override;

private:

    FString SlateName;
    uint16 TakeNumber;
	TOptional<FString> Subject;
	TOptional<FString> Scenario;
	TOptional<TArray<FString>> Tags;
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FStopRecordingTakeRequest final : public FControlRequest
{
public:

    using ResponseType = FStopRecordingTakeResponse;

    FStopRecordingTakeRequest();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FAbortRecordingTakeRequest final : public FControlRequest
{
public:

	using ResponseType = FAbortRecordingTakeResponse;

	FAbortRecordingTakeRequest();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FGetTakeListRequest final : public FControlRequest
{
public:

    using ResponseType = FGetTakeListResponse;

    FGetTakeListRequest();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FGetTakeMetadataRequest final : public FControlRequest
{
public:

    using ResponseType = FGetTakeMetadataResponse;

    FGetTakeMetadataRequest(TArray<FString> InNames);

    virtual TSharedPtr<FJsonObject> GetBody() const override;

private:

    TArray<FString> Names;
};
