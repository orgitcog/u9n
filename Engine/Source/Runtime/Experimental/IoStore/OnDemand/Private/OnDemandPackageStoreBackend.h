// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoStatus.h"
#include "IO/PackageStore.h"
#include "Templates/SharedPointer.h"

struct FIoContainerHeader;
class FOnDemandIoStore;
using FSharedContainerHeader = TSharedPtr<FIoContainerHeader>;

namespace UE::IoStore
{

enum class EOnDemandPackageStoreUpdateMode : uint8
{
	None,
	Full,
	ReferencedPackages,
};

class IOnDemandPackageStoreBackend
	: public IPackageStoreBackend
{
public:
	virtual ~IOnDemandPackageStoreBackend() = default;

	virtual void NeedsUpdate(EOnDemandPackageStoreUpdateMode Mode = EOnDemandPackageStoreUpdateMode::Full) = 0;
};

TSharedPtr<IOnDemandPackageStoreBackend> MakeOnDemandPackageStoreBackend(TWeakPtr<FOnDemandIoStore> OnDemandIoStore);

} // namespace UE
