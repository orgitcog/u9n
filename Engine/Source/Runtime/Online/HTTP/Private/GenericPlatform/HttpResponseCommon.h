// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IHttpResponse.h"

class FHttpRequestCommon;

/**
 * Contains implementation of some common functions that don't vary between implementations of different platforms
 */
class FHttpResponseCommon : public IHttpResponse
{
	friend FHttpRequestCommon;

public:
	FHttpResponseCommon(const FHttpRequestCommon& HttpRequest);

	// IHttpBase
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual const FString& GetURL() const override;
	virtual const FString& GetEffectiveURL() const override;
	virtual EHttpRequestStatus::Type GetStatus() const override;
	virtual EHttpFailureReason GetFailureReason() const override;
	virtual int32 GetResponseCode() const override;
	virtual FUtf8StringView GetContentAsUtf8StringView() const override;

protected:
	void SetRequestStatus(EHttpRequestStatus::Type InCompletionStatus);
	void SetRequestFailureReason(EHttpFailureReason InFailureReason);
	void SetEffectiveURL(const FString& InEffectiveURL);
	void SetResponseCode(int32 InResponseCode);

	FString URL;
	FString EffectiveURL;
	EHttpRequestStatus::Type CompletionStatus;
	EHttpFailureReason FailureReason;
	int32 ResponseCode = EHttpResponseCodes::Unknown;
};
