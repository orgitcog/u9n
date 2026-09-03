// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if (defined(__AUTORTFM) && __AUTORTFM)

#include "AutoRTFMConstants.h"
#include "FunctionMap.h"
#include "Utils.h"

namespace AutoRTFM
{

UE_AUTORTFM_FORCEINLINE
void* FunctionMapLookupUsingMagicPrefix(void* OpenFn)
{
	// We use prefix data in our custom LLVM pass to stuff some data just
	// before the address of all open function pointers (that we have
	// definitions for!). We verify the special Magic Prefix constant in the
	// top 16-bits of the function pointer address as a magic constant check
	// to give us a much higher confidence that there is actually a closed
	// variant pointer residing 8-bytes before our function address.
	uint64_t PrefixData;
	memcpy(&PrefixData, reinterpret_cast<char*>(OpenFn) - sizeof(uint64_t), sizeof(uint64_t));
	if (AUTORTFM_LIKELY(Constants::MagicPrefix == (PrefixData & 0xffff'0000'0000'0000)))
	{
		return reinterpret_cast<void*>(PrefixData & 0x0000'ffff'ffff'ffff);
	}
	// UBSAN adds a type hash prefix to the function as a "prologue" that
	// ends preceding our Magic Prefix. They use 0xc105cafe in the
	// lower 32-bits to distinguish the 64-bit word containing their type
	// hash. If we see it, check the preceding 64-bit word for our prefix.
	else if (AUTORTFM_UNLIKELY(0xc105'cafe == (PrefixData & 0x0000'0000'ffff'ffff)))
	{
		memcpy(&PrefixData, reinterpret_cast<char*>(OpenFn) - sizeof(uint64_t) * 2, sizeof(uint64_t));
		if (AUTORTFM_LIKELY(Constants::MagicPrefix == (PrefixData & 0xffff'0000'0000'0000)))
		{
			return reinterpret_cast<void*>(PrefixData & 0x0000'ffff'ffff'ffff);
		}
	}

	return nullptr;
}

inline void* FunctionMapLookup(void* OpenFn, const char* Where)
{
	if (void* ClosedFn = FunctionMapLookupUsingMagicPrefix(OpenFn); AUTORTFM_LIKELY(ClosedFn))
	{
		return ClosedFn;
	}

	AUTORTFM_MUST_TAIL return FunctionMapLookupExhaustive(OpenFn, Where);
}

template<typename TReturnType, typename... TParameterTypes>
auto FunctionMapLookup(TReturnType (*OpenFn)(TParameterTypes...), const char* Where) -> TReturnType (*)(TParameterTypes...)
{
    return reinterpret_cast<TReturnType (*)(TParameterTypes...)>(FunctionMapLookup(reinterpret_cast<void*>(OpenFn), Where));
}

} // namespace AutoRTFM

#endif // (defined(__AUTORTFM) && __AUTORTFM)
