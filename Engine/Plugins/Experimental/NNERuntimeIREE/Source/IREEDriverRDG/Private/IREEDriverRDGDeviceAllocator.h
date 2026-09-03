// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef WITH_IREE_DRIVER_RDG

#include "IREEDriverRDG.h"

namespace UE::IREE::HAL::RDG
{

iree_status_t DeviceAllocatorCreate(iree_allocator_t HostAllocator, iree_hal_allocator_t** OutDeviceAllocator);

} // UE::IREE

#endif // WITH_IREE_DRIVER_RDG