// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMFloat.h"
#include "VerseVM/VVMType.h"

namespace Verse
{

// An float type with constraints.
struct VFloatType : VType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static VFloatType& New(FAllocationContext Context, VFloat InMin, VFloat InMax)
	{
		return *new (Context.AllocateFastCell(sizeof(VFloatType))) VFloatType(Context, InMin, InMax);
	}
	static bool Equals(const VType& Type, VFloat Min, VFloat Max)
	{
		if (Type.IsA<VFloatType>())
		{
			const VFloatType& Other = Type.StaticCast<VFloatType>();
			return Min == Other.GetMin() && Max == Other.GetMax();
		}
		return false;
	}

	const VFloat& GetMin() const
	{
		return Min;
	}

	const VFloat& GetMax() const
	{
		return Max;
	}

	bool SubsumesImpl(FAllocationContext Context, VValue);

	void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);

	static constexpr bool SerializeIdentity = false;
	static void SerializeLayout(FAllocationContext Context, VFloatType*& This, FStructuredArchiveVisitor& Visitor);
	void SerializeImpl(FAllocationContext, FStructuredArchiveVisitor& Visitor);

private:
	explicit VFloatType(FAllocationContext& Context, VFloat InMin, VFloat InMax)
		: VType(Context, &GlobalTrivialEmergentType.Get(Context))
		, Min(InMin)
		, Max(InMax)
	{
	}

	VFloat Min;
	VFloat Max;
};
} // namespace Verse

#endif