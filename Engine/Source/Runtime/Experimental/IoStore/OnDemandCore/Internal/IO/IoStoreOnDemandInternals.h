// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoStoreOnDemand.h"
#include "HAL/PlatformTime.h"

#ifndef WITH_IOSTORE_ONDEMAND_TESTS
#define WITH_IOSTORE_ONDEMAND_TESTS 0
#endif

#define UE_API IOSTOREONDEMANDCORE_API

namespace UE::IoStore
{

///////////////////////////////////////////////////////////////////////////////
struct FOnDemandWeakContentHandle
{
	inline bool operator==(const FOnDemandWeakContentHandle& Other) const
	{
		return HandleId == Other.HandleId;
	}

	inline bool operator!=(const FOnDemandWeakContentHandle& Other) const
	{
		return HandleId != Other.HandleId;
	}

	inline friend uint32 GetTypeHash(const FOnDemandWeakContentHandle& Handle)
	{
		return GetTypeHash(Handle.HandleId);
	}

	UE_API static FOnDemandWeakContentHandle FromUnsafeHandle(UPTRINT HandleId);

	UPTRINT			HandleId = 0;
	FSharedString	DebugName;
};

///////////////////////////////////////////////////////////////////////////////
class FOnDemandInternalContentHandle
{
public:
	FOnDemandInternalContentHandle()
		: DebugName(TEXT("NoName"))
	{ }

	FOnDemandInternalContentHandle(FSharedString InDebugName)
		: DebugName(InDebugName)
	{ }

	UE_API ~FOnDemandInternalContentHandle();

	UPTRINT HandleId() const { return UPTRINT(this); }

	FSharedString			DebugName;
	FWeakOnDemandIoStore	IoStore;
};

////////////////////////////////////////////////////////////////////////////////
class FOnDemandInternalInstallRequest
{
public:
	using EStatus = FOnDemandRequest::EStatus;
	
	explicit FOnDemandInternalInstallRequest(UPTRINT InInstallerRequest)
		: InstallerRequest(InInstallerRequest) { }

	UPTRINT					InstallerRequest = 0;
	std::atomic<EStatus>	Status{EStatus::Pending};
};

} // namespace UE::IoStore

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UE_API FString LexToString(const UE::IoStore::FOnDemandInternalContentHandle& Handle);

#undef UE_API
