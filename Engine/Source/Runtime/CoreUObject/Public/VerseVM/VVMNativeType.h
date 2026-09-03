// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"

namespace Verse
{
class FNativeType;
struct FNativeConverter;
template <typename NativeType, typename>
struct TToVValue;
} // namespace Verse

template <>
struct TIsZeroConstructType<Verse::FNativeType>
{
	static constexpr bool Value = true;
};

namespace Verse
{

enum class EDefaultConstructNativeType
{
	UnsafeDoNotUse
}; // So we can construct FNativeTypes

class FNativeTypeBase
{
public:
	FNativeTypeBase() = delete;
	FNativeTypeBase(EDefaultConstructNativeType) {}

	FNativeTypeBase(UClass* InType)
		: Type(InType)
	{
	}

	bool IsNullUnsafeDoNotUse() const { return !Type; }
	bool IsEqualUnsafeDoNotUse(const FNativeTypeBase& Other) const { return Type == Other.Type; }
	UClass* AsUEClassNullableUnsafeDoNotUse() const { return Type; }

protected:
	friend struct Verse::FNativeConverter;
	template <typename NativeType, typename>
	friend struct Verse::TToVValue;

	UClass* AsUEClassChecked() const
	{
		check(Type);
		return Type;
	}

	bool IsTypeOf(UObject* Obj) const
	{
		return Obj->IsA(Type);
	}

	// Do not change, we map this to a `FClassProperty` so it must be bitwise compatible
	TObjectPtr<UClass> Type;
};

// Opaque wrapper around VM-specific type representation
class FNativeType : public FNativeTypeBase
{
public:
	using FNativeTypeBase::FNativeTypeBase;

	UClass* AsUEClassCheckedUnsafeDoNotUse() const { return FNativeTypeBase::AsUEClassChecked(); }
	bool IsTypeOfUnsafeDoNotUse(UObject* Obj) const { return FNativeTypeBase::IsTypeOf(Obj); }
};

template <class T, class BaseType = FNativeType>
class TNativeSubtype : public BaseType
{
public:
	TNativeSubtype() = delete;
	TNativeSubtype(EDefaultConstructNativeType)
		: BaseType(EDefaultConstructNativeType::UnsafeDoNotUse) {}
	TNativeSubtype(BaseType&& Other)
		: BaseType(MoveTemp(Other)) { CheckValid(); }
	TNativeSubtype(const BaseType& Other)
		: BaseType(Other) { CheckValid(); }
	TNativeSubtype& operator=(BaseType&& Other)
	{
		BaseType::operator=(MoveTemp(Other));
		CheckValid();
		return *this;
	}
	TNativeSubtype& operator=(const BaseType& Other)
	{
		BaseType::operator=(Other);
		CheckValid();
		return *this;
	}

	using BaseType::AsUEClassChecked;
	using BaseType::IsTypeOf;

	TNativeSubtype(UClass* InType)
		: BaseType(InType)
	{
		CheckValid();
	}

private:
	void CheckValid() const { check(FNativeTypeBase::AsUEClassChecked()->IsChildOf(T::StaticClass())); }
};

// Support for castable types

class FNativeCastableType : public FNativeType
{
public:
	using FNativeType::FNativeType;
	using FNativeType::IsTypeOf;
};

template <class T>
using TNativeCastableSubtype = TNativeSubtype<T, FNativeCastableType>;

} // namespace Verse
