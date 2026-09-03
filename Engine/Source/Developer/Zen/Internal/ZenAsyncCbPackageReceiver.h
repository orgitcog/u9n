// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Experimental/ZenGlobals.h"

#if UE_WITH_ZEN

#include "Containers/StringFwd.h"
#include "Http/HttpClient.h"
#include "Serialization/CompactBinaryPackage.h"
#include "ZenCbPackageReceiver.h"

namespace UE::Zen
{

class FZenServiceInstance;

class FAsyncCbPackageReceiver : public IHttpReceiver
{
public:
	using FOnComplete = TUniqueFunction<void(FAsyncCbPackageReceiver* Receiver, const THttpUniquePtr<IHttpResponse>& HttpResponse, const FCbPackage& ResponsePackage)>;

	FAsyncCbPackageReceiver(const FAsyncCbPackageReceiver&) = delete;
	FAsyncCbPackageReceiver& operator=(const FAsyncCbPackageReceiver&) = delete;

	ZEN_API FAsyncCbPackageReceiver(
		THttpUniquePtr<IHttpRequest>&& InRequest,
		FZenServiceInstance& InZenServiceInstance,
		FOnComplete&& InOnComplete,
		int InMaxAttempts = 1);

	ZEN_API void SendAsync();

protected:
	// IHttpReceiver Interface

	ZEN_API IHttpReceiver* OnCreate(IHttpResponse& LocalResponse);
	ZEN_API IHttpReceiver* OnComplete(IHttpResponse& LocalResponse);

	ZEN_API FString GetPayloadAsString() const;

protected:
	THttpUniquePtr<IHttpRequest> Request;
	THttpUniquePtr<IHttpResponse> Response;
	FZenServiceInstance& ZenServiceInstance;
	FCbPackage Package;
	UE::Zen::FCbPackageReceiver BaseReceiver;
	FOnComplete OnCompleteCallback;
	const int MaxAttempts;
	int Attempt;
};

} // namespace UE::Zen

#endif // UE_WITH_ZEN