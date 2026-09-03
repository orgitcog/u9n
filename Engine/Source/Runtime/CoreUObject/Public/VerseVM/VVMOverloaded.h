// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/UnrealTemplate.h"

#include <type_traits>

template <typename... T>
struct FOverloaded : T...
{
	template <typename... U>
	explicit FOverloaded(U&&... Args)
		: T(::Forward<U>(Args))...
	{
	}

	using T::operator()...;
};

template <typename... T>
FOverloaded(T&&...) -> FOverloaded<std::decay_t<T>...>;

#endif
