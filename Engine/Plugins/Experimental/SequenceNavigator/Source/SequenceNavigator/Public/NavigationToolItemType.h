// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolItemTypeConcepts.h"
#include "NavigationToolItemTypeId.h"
#include "NavigationToolItemTypeTraits.h"
#include "Templates/AndOrNot.h"
#include "Templates/Models.h"

namespace UE::SequenceNavigator
{

/**
 * Macro to provide type information (type id and super types).
 * This should be used within the class/struct scope only.
 * @see UE_NAVIGATIONTOOL_TYPE_EXTERNAL for providing type information outside the class/struct scope
 */
#define UE_NAVIGATIONTOOL_TYPE(InThisType, ...)                                                 \
	using FNavigationToolItemInherits = TNavigationToolItemInherits<InThisType, ##__VA_ARGS__>; \
	static FNavigationToolItemTypeId GetStaticTypeId()                                          \
	{                                                                                           \
		static const FNavigationToolItemTypeId TypeId(TEXT(#InThisType));                       \
		return TypeId;                                                                          \
	}                                                                                           \

/**
 * Macro to implement a specialization of TNavigationToolItemExternalType to specify the UE_NAVIGATIONTOOL_TYPE for a given type.
 * This should only be done for types where a UE_NAVIGATIONTOOL_TYPE internal implementation is not possible or desired.
 * @see UE_NAVIGATIONTOOL_TYPE
 */
#define UE_NAVIGATIONTOOL_TYPE_EXTERNAL(InExternalType, ...)   \
	template<>                                                 \
	struct TNavigationToolItemExternalType<InExternalType>     \
	{                                                          \
		UE_NAVIGATIONTOOL_TYPE(InExternalType, ##__VA_ARGS__); \
	};                                                         \

/**
 * Macro to implement UE_NAVIGATIONTOOL_TYPE for a given type and the overrides for the INavigationToolItemTypeCastable interface.
 * This should be used within a class/struct that ultimately inherits from INavigationToolItemTypeCastable
 * @see INavigationToolItemTypeCastable
 * @see UE_NAVIGATIONTOOL_TYPE
 */
#define UE_NAVIGATIONTOOL_INHERITS(InThisType, ...)                                        \
	UE_NAVIGATIONTOOL_TYPE(InThisType, ##__VA_ARGS__)                                      \
	virtual FNavigationToolItemTypeId GetTypeId() const override                           \
	{                                                                                      \
		return TNavigationToolItemType<InThisType>::GetTypeId();                           \
	}                                                                                      \
	virtual const void* CastTo_Impl(FNavigationToolItemTypeId InCastToType) const override \
	{                                                                                      \
		return FNavigationToolItemInherits::Cast(this, InCastToType);                      \
	}                                                                                      \

#define UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(InThisType, InSuperType, ...) \
	UE_NAVIGATIONTOOL_INHERITS(InThisType, InSuperType, ##__VA_ARGS__)      \
	using Super = InSuperType;                                              \

template<typename T>
struct TNavigationToolItemExternalType;

/**
 * Determines that a typename T is a valid "Navigation Tool Item Type" if a FNavigationToolItemTypeId can be retrieved by calling either
 * T::GetStaticTypeId() (via UE_NAVIGATIONTOOL_TYPE) or TNavigationToolItemExternalType<T>::GetStaticTypeId() (via UE_NAVIGATIONTOOL_TYPE_EXTERNAL)
 */
template<typename T>
using TIsValidNavigationToolItemType = TOr<TModels<CNavigationToolItemStaticTypeable, T>, TModels<CNavigationToolItemStaticTypeable, TNavigationToolItemExternalType<T>>>;

/**
 * Helps retrieve the FNavigationToolItemTypeId for a given type T that satisfies the TIsValidNavigationToolItemType requirement
 * If this is requirement is not satisfied, TNavigationToolItemType<T>::GetTypeId will be undefined for T.
 * @see TIsValidNavigationToolItemType
 */
template<typename T, bool = TIsValidNavigationToolItemType<T>::Value>
struct TNavigationToolItemType
{
	static FNavigationToolItemTypeId GetTypeId()
	{
		if constexpr (TModels_V<CNavigationToolItemStaticTypeable, T>)
		{
			return T::GetStaticTypeId();
		}
		else if constexpr (TModels_V<CNavigationToolItemStaticTypeable, TNavigationToolItemExternalType<T>>)
		{
			return TNavigationToolItemExternalType<T>::GetStaticTypeId();
		}
		else
		{
			checkNoEntry();
			return FNavigationToolItemTypeId::Invalid();
		}
	}
};

/** No support for types that do not have a way to get their FNavigationToolItemTypeId */
template<typename T>
struct TNavigationToolItemType<T, false>
{
};

/**
 * Holds the direct super types of a given type T.
 * Also serves to Cast a provided pointer to a desired type id.
 */
template<typename T, typename... InSuperTypes>
class TNavigationToolItemInherits
{
	static_assert(TIsValidNavigationToolItemType<T>::Value, "Typename does not satisfy the requirements to be an Navigation Tool Item Type. See: TIsValidNavigationToolItemType");
	static_assert(TIsDerivedFromAll<T, InSuperTypes...>::Value, "Some provided super types are not base types of T");

	template<typename, typename InSuperType, typename... InOtherSuperTypes>
	static const void* Cast(const T* InPtr, const FNavigationToolItemTypeId& InCastToType)
	{
		// Check first super type
		if (TNavigationToolItemType<InSuperType>::GetTypeId() == InCastToType)
		{
			return static_cast<const InSuperType*>(InPtr);
		}
		// Check rest of super types
		else if (const void* CastedPtr = Cast<void, InOtherSuperTypes...>(InPtr, InCastToType))
		{
			return CastedPtr;
		}
		// Recurse up the super type
		else if constexpr (TModels_V<CNavigationToolItemInheritable, InSuperType>)
		{
			return InSuperType::FNavigationToolItemInherits::Cast(InPtr, InCastToType);
		}
		// Recurse up the super type for External Types
		else if constexpr (TModels_V<CNavigationToolItemInheritable, TNavigationToolItemExternalType<InSuperType>>)
		{
			return TNavigationToolItemExternalType<InSuperType>::FNavigationToolItemInherits::Cast(InPtr, InCastToType);
		}
		else
		{
			return nullptr;
		}
	}

	template<typename>
	static const void* Cast(const T* InPtr, const FNavigationToolItemTypeId& InCastToType)
	{
		return nullptr;
	}

public:
	static const void* Cast(const T* InPtr, const FNavigationToolItemTypeId& InCastToType)
	{
		// Check that Cast To Type is valid
		if (!InCastToType.IsValid())
		{
			return nullptr;
		}
		// Check whether Cast To Type is this Type
		if (TNavigationToolItemType<T>::GetTypeId() == InCastToType)
		{
			return InPtr;
		}
		return Cast<void, InSuperTypes...>(InPtr, InCastToType);
	}
};

/**
 * Interface Class to perform actions like IsA, or CastTo
 * @see UE_NAVIGATIONTOOL_INHERITS
 */
class INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_TYPE(INavigationToolItemTypeCastable)

	virtual ~INavigationToolItemTypeCastable() = default;

	virtual FNavigationToolItemTypeId GetTypeId() const = 0;

	virtual const void* CastTo_Impl(FNavigationToolItemTypeId InId) const = 0;

	template<typename InType>
	bool IsA() const
	{
		return !!this->CastTo<InType>();
	}

	template<typename InType>
	bool IsExactlyA() const
	{
		return GetTypeId() == TNavigationToolItemType<InType>::GetTypeId();
	}

	template<typename InType>
	InType* CastTo()
	{
		const INavigationToolItemTypeCastable* ConstThis = this;
		return const_cast<InType*>(ConstThis->CastTo<InType>());
	}

	template<typename InType>
	const InType* CastTo() const
	{
		return static_cast<const InType*>(this->CastTo_Impl(TNavigationToolItemType<InType>::GetTypeId()));
	}
};

} // namespace UE::SequenceNavigator
