// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZenAsyncCbPackageReceiver.h"

#if UE_WITH_ZEN

#include "Containers/UnrealString.h"
#include "Experimental/ZenServerInterface.h"
#include "Serialization/MemoryReader.h"
#include "ZenSerialization.h"

namespace UE::Zen
{

FAsyncCbPackageReceiver::FAsyncCbPackageReceiver(
	THttpUniquePtr<IHttpRequest>&& InRequest,
	Zen::FZenServiceInstance& InZenServiceInstance,
	FOnComplete&& InOnComplete,
	int InMaxAttempts)
	: Request(MoveTemp(InRequest))
	, ZenServiceInstance(InZenServiceInstance)
	, BaseReceiver(Package, this)
	, OnCompleteCallback(MoveTemp(InOnComplete))
	, MaxAttempts(InMaxAttempts)
	, Attempt(0)
{
}

void FAsyncCbPackageReceiver::SendAsync()
{
	Request->SendAsync(this, Response);
}

IHttpReceiver* FAsyncCbPackageReceiver::OnCreate(IHttpResponse& LocalResponse)
{
	return &BaseReceiver;
}

IHttpReceiver* FAsyncCbPackageReceiver::OnComplete(IHttpResponse& LocalResponse)
{
	if ((++Attempt < MaxAttempts) && FCbPackageReceiver::ShouldRecoverAndRetry(ZenServiceInstance, LocalResponse) && ZenServiceInstance.TryRecovery())
	{
		BaseReceiver.Reset();
		SendAsync();
		return nullptr;
	}

	Request.Reset();
	if (OnCompleteCallback)
	{
		// Ensuring that the OnComplete method is destroyed by the time we exit this method by moving it to a local scope variable
		FOnComplete LocalOnComplete = MoveTemp(OnCompleteCallback);

		// Calling LocalOnComplete may result in "this" being deleted, so no further access can happen to anything on "this"
		LocalOnComplete(this, Response, Package);
	}
	return nullptr;
}

FString FAsyncCbPackageReceiver::GetPayloadAsString() const
{
	EHttpMediaType ContentType = Response->GetContentType();
	FMemoryView BodyView = BaseReceiver.Body();
	switch (ContentType)
	{
		case EHttpMediaType::Text:
		case EHttpMediaType::Yaml:
		case EHttpMediaType::Json:
			if (BodyView.GetSize() > 0)
			{
				TStringBuilder<128> PayloadText;
				PayloadText << FAnsiStringView((const ANSICHAR*)BodyView.GetData(), (int)(BodyView.GetSize()));
				return *PayloadText;
			}
			return {};
		default:
		{
			TStringBuilder<128> SizeText;
			SizeText << "Payload (" << LexToString(ContentType) << "): " << BodyView.GetSize() << " bytes";
			return *SizeText;
		}
	}
}

}
#endif // UE_WITH_ZEN
