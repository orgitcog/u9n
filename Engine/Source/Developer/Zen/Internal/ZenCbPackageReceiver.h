// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Experimental/ZenGlobals.h"

#if UE_WITH_ZEN

#include "Containers/Array.h"
#include "Http/HttpClient.h"

class FCbPackage;

namespace UE::Zen
{

class FZenServiceInstance;

class FCbPackageReceiver final : public IHttpReceiver
{
public:
	FCbPackageReceiver(const FCbPackageReceiver&) = delete;
	FCbPackageReceiver& operator=(const FCbPackageReceiver&) = delete;

	ZEN_API explicit FCbPackageReceiver(FCbPackage& OutPackage, IHttpReceiver* InNext = nullptr);

	ZEN_API void Reset();
	ZEN_API const FMemoryView Body() const;

	ZEN_API static bool ShouldRecoverAndRetry(FZenServiceInstance& ZenServiceInstance, IHttpResponse& LocalResponse);

private:
	ZEN_API IHttpReceiver* OnCreate(IHttpResponse& Response) final;
	ZEN_API IHttpReceiver* OnComplete(IHttpResponse& Response) final;

private:
	FCbPackage& Package;
	IHttpReceiver* Next;
	TArray64<uint8> BodyArray;
	FHttpByteArrayReceiver BodyReceiver{BodyArray, this};
};

} // namespace UE::Zen

#endif // UE_WITH_ZEN