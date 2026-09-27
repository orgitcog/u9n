// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef WITH_IREE_DRIVER_RDG

#include "IREEDriverRDG.h"

namespace UE::IREE::HAL::RDG
{

iree_status_t NoOpExecutableCacheCreate(iree_allocator_t HostAllocator, const FString& BasePath, iree_hal_executable_cache_t** OutExecutableCache);

} // UE::IREE

#endif // WITH_IREE_DRIVER_RDG