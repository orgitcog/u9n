// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/UnrealMemory.h"
#include "Misc/CoreMiscDefines.h"
#include "Math/UnrealMathUtility.h"

namespace Audio
{
	class FSimpleAllocBase
	{
	public:
		UE_NONCOPYABLE(FSimpleAllocBase);
		FSimpleAllocBase() = default;
		virtual ~FSimpleAllocBase() = default;
	
		[[nodiscard]] virtual void* Malloc(const SIZE_T, const uint32 InAlignment=DEFAULT_ALIGNMENT) = 0;
		virtual void Free(void*) {};
		virtual uint32 GetCurrentLifetime() const { return 0; };
		virtual void Reset() {};
	};

	namespace SimpleAllocBasePrivate
	{
		// This is what's defined in MemoryBase, so follow it here. 
		FORCEINLINE static uint32 GetDefaultSizeToAlignment(const uint32 InSize)
		{
			check(InSize > 0);
			return InSize >= 16 ? 16 : 8;
		};

		// When given an alignment and offset return a new rounded offset that would honor the alignment
		FORCEINLINE static uint32 RoundUpToAlignment(const uint32 InOffset, const uint32 InAlignment)
		{
			check(InAlignment > 0);
			check(FMath::IsPowerOfTwo(InAlignment));
			const uint32 Mod = InOffset & (InAlignment-1);
			const uint32 RoundUp = Mod > 0 ? InAlignment - Mod : 0;
			return InOffset + RoundUp;
		}

		FORCEINLINE static bool IsAligned(const void* InPtr, const uint32 InAlignment)
		{
			check(FMath::IsPowerOfTwo(InAlignment));
			return (reinterpret_cast<UPTRINT>(InPtr) & (InAlignment - 1)) == 0;
		}
	} //namespace FSimpleAllocBasePrivate
}