// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoStoreOnDemandInternals.h"

namespace UE::IoStore
{

///////////////////////////////////////////////////////////////////////////////
FOnDemandWeakContentHandle FOnDemandWeakContentHandle::FromUnsafeHandle(UPTRINT HandleId)
{
	FOnDemandInternalContentHandle& ContentHandle = *reinterpret_cast<FOnDemandInternalContentHandle*>(HandleId);
	return FOnDemandWeakContentHandle
	{
		.HandleId	= HandleId,
		.DebugName	= ContentHandle.DebugName
	};
}

///////////////////////////////////////////////////////////////////////////////
FOnDemandInternalContentHandle::~FOnDemandInternalContentHandle()
{
	if (TSharedPtr<IOnDemandIoStore, ESPMode::ThreadSafe> Pinned = IoStore.Pin(); Pinned.IsValid())
	{
		Pinned->ReleaseContent(*this);
	}
}

///////////////////////////////////////////////////////////////////////////////
FOnDemandContentHandle::FOnDemandContentHandle()
{
}

FOnDemandContentHandle::~FOnDemandContentHandle()
{
}

FOnDemandContentHandle FOnDemandContentHandle::Create()
{
	FOnDemandContentHandle NewHandle;
	NewHandle.Handle = MakeShared<FOnDemandInternalContentHandle, ESPMode::ThreadSafe>();
	return NewHandle;
}

FOnDemandContentHandle FOnDemandContentHandle::Create(FSharedString DebugName)
{
	FOnDemandContentHandle NewHandle;
	NewHandle.Handle = MakeShared<FOnDemandInternalContentHandle, ESPMode::ThreadSafe>(DebugName);
	return NewHandle;
}

FOnDemandContentHandle FOnDemandContentHandle::Create(FStringView DebugName)
{
	return Create(FSharedString(DebugName));
}

FString LexToString(const FOnDemandContentHandle& Handle)
{
	return Handle.IsValid() ? ::LexToString(*Handle.Handle) : TEXT("Invalid");
}

///////////////////////////////////////////////////////////////////////////////
FOnDemandInstallRequest::FOnDemandInstallRequest()
{
}

FOnDemandInstallRequest::FOnDemandInstallRequest(FWeakOnDemandIoStore InIoStore, FSharedInternalInstallRequest InternalRequest)
	: IoStore(InIoStore)
	, Request(InternalRequest)
{
}

FOnDemandInstallRequest::~FOnDemandInstallRequest()
{
}

FOnDemandInstallRequest::FOnDemandInstallRequest(FOnDemandInstallRequest&& Other)
	: IoStore(MoveTemp(Other.IoStore))
	, Request(MoveTemp(Other.Request))
{
}

FOnDemandInstallRequest& FOnDemandInstallRequest::operator=(FOnDemandInstallRequest&& Other)
{
	Request = MoveTemp(Other.Request);
	IoStore = MoveTemp(Other.IoStore);
	return *this;
}

FOnDemandInstallRequest::EStatus FOnDemandInstallRequest::GetStatus() const
{
	return Request.IsValid() ? Request->Status.load(std::memory_order_relaxed) : EStatus::None; 
}

void FOnDemandInstallRequest::Cancel()
{
	if (IsPending())
	{
		if (TSharedPtr<IOnDemandIoStore, ESPMode::ThreadSafe> Pinned = IoStore.Pin(); Pinned.IsValid())
		{
			Pinned->CancelInstallRequest(Request);
		}
	}
}

void FOnDemandInstallRequest::UpdatePriority(int32 NewPriority)
{
	if (IsPending())
	{
		if (TSharedPtr<IOnDemandIoStore, ESPMode::ThreadSafe> Pinned = IoStore.Pin(); Pinned.IsValid())
		{
			Pinned->UpdateInstallRequestPriority(Request, NewPriority);
		}
	}
}

} // namespace UE::IoStore

////////////////////////////////////////////////////////////////////////////////
FString LexToString(const UE::IoStore::FOnDemandInternalContentHandle& Handle)
{
	return FString::Printf(TEXT("Id=0x%llX DebugName='%s'"), Handle.HandleId(), *Handle.DebugName);
}
