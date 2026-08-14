// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlResponse.h"
#include "Misc/Optional.h"

namespace UE::CaptureManager
{

class CAPTUREPROTOCOLSTACK_API FControlRequest
{
public:

	FControlRequest(FString InAddressPath);
	virtual ~FControlRequest() = default;

	const FString& GetAddressPath() const;
	virtual TSharedPtr<FJsonObject> GetBody() const;

private:

	FString AddressPath;
};

class CAPTUREPROTOCOLSTACK_API FKeepAliveRequest final : public FControlRequest
{
public:

	using ResponseType = FKeepAliveResponse;

	FKeepAliveRequest();
};

class CAPTUREPROTOCOLSTACK_API FStartSessionRequest final : public FControlRequest
{
public:

	using ResponseType = FStartSessionResponse;

	FStartSessionRequest();
};

class CAPTUREPROTOCOLSTACK_API FStopSessionRequest final : public FControlRequest
{
public:

	using ResponseType = FStopSessionResponse;

	FStopSessionRequest();
};


class CAPTUREPROTOCOLSTACK_API FGetServerInformationRequest final : public FControlRequest
{
public:

	using ResponseType = FGetServerInformationResponse;

	FGetServerInformationRequest();
};

class CAPTUREPROTOCOLSTACK_API FSubscribeRequest final : public FControlRequest
{
public:

	using ResponseType = FSubscribeResponse;

	FSubscribeRequest();
};

class CAPTUREPROTOCOLSTACK_API FUnsubscribeRequest final : public FControlRequest
{
public:

	using ResponseType = FUnsubscribeResponse;

	FUnsubscribeRequest();
};

class CAPTUREPROTOCOLSTACK_API FGetStateRequest final : public FControlRequest
{
public:

	using ResponseType = FGetStateResponse;

	FGetStateRequest();
};

class CAPTUREPROTOCOLSTACK_API FStartRecordingTakeRequest final : public FControlRequest
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

class CAPTUREPROTOCOLSTACK_API FStopRecordingTakeRequest final : public FControlRequest
{
public:

	using ResponseType = FStopRecordingTakeResponse;

	FStopRecordingTakeRequest();
};

class CAPTUREPROTOCOLSTACK_API FAbortRecordingTakeRequest final : public FControlRequest
{
public:

	using ResponseType = FAbortRecordingTakeResponse;

	FAbortRecordingTakeRequest();
};

class CAPTUREPROTOCOLSTACK_API FGetTakeListRequest final : public FControlRequest
{
public:

	using ResponseType = FGetTakeListResponse;

	FGetTakeListRequest();
};

class CAPTUREPROTOCOLSTACK_API FGetTakeMetadataRequest final : public FControlRequest
{
public:

	using ResponseType = FGetTakeMetadataResponse;

	FGetTakeMetadataRequest(TArray<FString> InNames);

	virtual TSharedPtr<FJsonObject> GetBody() const override;

private:

	TArray<FString> Names;
};

class CAPTUREPROTOCOLSTACK_API FGetStreamingSubjectsRequest final : public FControlRequest
{
public:
	using ResponseType = FGetStreamingSubjectsResponse;

	FGetStreamingSubjectsRequest();
};

class CAPTUREPROTOCOLSTACK_API FStartStreamingRequest final : public FControlRequest
{
public:

	struct FSubject
	{
		FString Id;
		TOptional<FString> Name;

		FSubject() = delete;
		
		explicit FSubject(const FString& InId)
		: Id(InId)
		{
		}
		
		FSubject(const FString& InId, const FString& InName)
		: Id(InId)
		, Name(InName)
		{
		}
	};
	
	using ResponseType = FStartStreamingResponse;

	FStartStreamingRequest(uint16 InStreamPort, TArray<FSubject> InSubjects);

	virtual TSharedPtr<FJsonObject> GetBody() const override;
	
private:
	
	uint16 StreamPort;
	TArray<FSubject> Subjects;
};

class CAPTUREPROTOCOLSTACK_API FStopStreamingRequest final : public FControlRequest
{
public:

	using ResponseType = FStopStreamingResponse;

	FStopStreamingRequest(TOptional<TArray<FString>> InSubjectIds = TOptional<TArray<FString>>());
	
	virtual TSharedPtr<FJsonObject> GetBody() const override;
	
private:
	TOptional<TArray<FString>> SubjectIds;
};

}