// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimpleAllocBase.h"

namespace Audio
{
	class FSimpleHeapAllocator final : public FSimpleAllocBase
	{
	public:
		FSimpleHeapAllocator() = default;
		virtual void* Malloc(const SIZE_T InSizeBytes, const uint32 InAlignment=DEFAULT_ALIGNMENT) override
		{
			return FMemory::Malloc(InSizeBytes, InAlignment);
		}
		virtual void Free(void* InPtr) override
		{
			return FMemory::Free(InPtr);
		}
	};
}
