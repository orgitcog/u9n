// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::SequenceNavigator
{

/** Describes a type that has a GetStaticTypeId() static function that returns FNavigationToolItemTypeId */
struct CNavigationToolItemStaticTypeable
{
	template <typename T>
	auto Requires(class FNavigationToolItemTypeId& Value) -> decltype(
		Value = T::GetStaticTypeId()
	);
};

/**
 * Describes a type with a FNavigationToolItemInherits typedef or using declaration
 * @see UE_NAVIGATIONTOOL_TYPE macro
 */
struct CNavigationToolItemInheritable
{
	template<typename T>
	auto Requires()->typename T::FNavigationToolItemInherits&;
};

} // namespace UE::SequenceNavigator
