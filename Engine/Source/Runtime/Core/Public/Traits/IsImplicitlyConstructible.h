// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <type_traits>

namespace UE::Core::Private
{
	template <typename T, typename... ArgTypes>
	struct TIsImplicitlyConstructibleImpl
	{
		template <typename U>
		static void TakesU(U&&);

#if defined(__cpp_concepts)
		static constexpr bool value = (!std::is_aggregate_v<T> || sizeof...(ArgTypes) == 0) && requires{ TakesU<T>({ std::declval<ArgTypes>()... }); };
#else
		template <typename U, typename, typename... OtherArgTypes>
		struct TDetect
		{
			static constexpr bool value = false;
		};
		template <typename U, typename... OtherArgTypes>
		struct TDetect<U, decltype(TakesU<U>({ std::declval<OtherArgTypes>()... })), OtherArgTypes...>
		{
			static constexpr bool value = true;
		};

		static constexpr bool value = (!std::is_aggregate_v<T> || sizeof...(ArgTypes) == 0) && TDetect<T, void, ArgTypes...>::value;
		static constexpr bool Value = value;
#endif
	};
	template <typename T, typename ArgType>
	struct TIsImplicitlyConstructibleImpl<T, ArgType>
	{
		static constexpr bool value = std::is_convertible_v<ArgType, T>;
		static constexpr bool Value = value;
	};
}

template <typename T, typename... ArgTypes>
struct TIsImplicitlyConstructible : UE::Core::Private::TIsImplicitlyConstructibleImpl<T, ArgTypes...>
{
};

template <typename T, typename... ArgTypes>
inline constexpr bool TIsImplicitlyConstructible_V = UE::Core::Private::TIsImplicitlyConstructibleImpl<T, ArgTypes...>::value;
