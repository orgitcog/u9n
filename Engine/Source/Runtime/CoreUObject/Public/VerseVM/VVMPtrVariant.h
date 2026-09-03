// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/TypeCompatibleBytes.h"
#include "Templates/TypeHash.h"

namespace Verse
{

template <typename T, typename... Ts>
struct TagOf;

template <typename T, typename... Ts>
struct TagOf<T, T, Ts...>
{
	static constexpr uintptr_t Value = 0;
};

template <typename T, typename U, typename... Ts>
struct TagOf<T, U, Ts...>
{
	static constexpr uintptr_t Value = 1 + TagOf<T, Ts...>::Value;
};

// Can't use std::bit_ceil yet.
inline constexpr size_t BitCeil(size_t X)
{
	size_t Pow2 = 1;
	while (Pow2 < X)
	{
		Pow2 *= 2;
	}
	return Pow2;
}

// Ts... are types which are ultimately pointers.
// We tag which of Ts... the variant holds by tagging the lower bits, so the pointers
// must be at least log(sizeof(Ts)...) + 1 byte aligned.
template <typename... Ts>
struct TPtrVariant
{
	static_assert(((sizeof(Ts) == sizeof(uintptr_t)) && ...));

	static constexpr uintptr_t Mask = BitCeil(sizeof...(Ts)) - 1;

	template <typename T>
	TPtrVariant(T InT)
	{
		uintptr_t IncomingPtr = BitCast<uintptr_t>(InT);
		uintptr_t TTag = TagOf<T, Ts...>::Value;
		checkSlow(!(IncomingPtr & Mask));
		Ptr = IncomingPtr | TTag;
	}

	template <typename T>
	bool Is()
	{
		static_assert((std::is_same_v<T, Ts> || ...));
		return (Ptr & Mask) == TagOf<T, Ts...>::Value;
	}

	template <typename T>
	T As()
	{
		static_assert((std::is_same_v<T, Ts> || ...));
		checkSlow(Is<T>());
		return BitCast<T>(Ptr & ~Mask);
	}

	bool operator==(TPtrVariant Other) const
	{
		return Ptr == Other.Ptr;
	}

	uintptr_t RawPtr() const { return Ptr; }

private:
	uintptr_t Ptr;
};

template <typename... Ts>
inline uint32 GetTypeHash(TPtrVariant<Ts...> Ptr)
{
	return PointerHash(BitCast<void*>(Ptr.RawPtr()));
}

} // namespace Verse
