// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZenCbPackageReceiver.h"

#if UE_WITH_ZEN

#include "Experimental/ZenServerInterface.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Serialization/MemoryReader.h"
#include "ZenSerialization.h"

namespace UE::Zen
{

FCbPackageReceiver::FCbPackageReceiver(FCbPackage& OutPackage, IHttpReceiver* InNext)
	: Package(OutPackage)
	, Next(InNext)
{
}

void FCbPackageReceiver::Reset()
{
	BodyArray.Reset();
}

const FMemoryView FCbPackageReceiver::Body() const
{
	return MakeMemoryView(BodyArray);
}

bool FCbPackageReceiver::ShouldRecoverAndRetry(Zen::FZenServiceInstance& ZenServiceInstance, IHttpResponse& LocalResponse)
{
	if (!ZenServiceInstance.IsServiceRunningLocally())
	{
		return false;
	}

	if ((LocalResponse.GetErrorCode() == EHttpErrorCode::Connect) ||
		(LocalResponse.GetErrorCode() == EHttpErrorCode::TlsConnect) ||
		(LocalResponse.GetErrorCode() == EHttpErrorCode::TimedOut))
	{
		return true;
	}

	return false;
}

IHttpReceiver* FCbPackageReceiver::OnCreate(IHttpResponse& Response)
{
	return &BodyReceiver;
}

IHttpReceiver* FCbPackageReceiver::OnComplete(IHttpResponse& Response)
{
	if (Response.GetErrorCode() == EHttpErrorCode::None)
	{
		EHttpMediaType ContentType = Response.GetContentType();
		switch (ContentType)
		{
			case EHttpMediaType::CbPackage:
			{
				const FMemoryView MemoryView = Body();
				{
					FMemoryReaderView Ar(MemoryView);
					if (Zen::Http::TryLoadCbPackage(Package, Ar))
					{
						BodyArray.Reset();
						return Next;
					}
				}
				FMemoryReaderView Ar(MemoryView);
				if (Package.TryLoad(Ar))
				{
					BodyArray.Reset();
				}
			}
			break;
		}
	}
	return Next;
}

}
#endif // UE_WITH_ZEN
