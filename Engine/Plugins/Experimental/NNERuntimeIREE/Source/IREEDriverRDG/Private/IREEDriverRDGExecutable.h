// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef WITH_IREE_DRIVER_RDG

#include "IREEDriverRDG.h"

class FNNERuntimeIREEResource;

namespace UE::IREE::HAL::RDG
{

iree_status_t ExecutableCreate(iree_allocator_t HostAllocator, const FString& BasePath, const iree_hal_executable_params_t* ExecutableParams, iree_hal_executable_t** OutExecutable);

iree_status_t ExecutableGetResource(iree_hal_executable_t* Executable, int32 EntryPoint, const FNNERuntimeIREEResource** OutResource);

} // UE::IREE

#endif // WITH_IREE_DRIVER_RDG