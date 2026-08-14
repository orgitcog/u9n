// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "VVMContext.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMInt.h"
#include "VVMValue.h"

namespace Verse
{
enum class EValueStringFormat;

struct VRational : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TWriteBarrier<VInt> Numerator;
	TWriteBarrier<VInt> Denominator;

	COREUOBJECT_API static VRational& Add(FAllocationContext Context, VRational& Lhs, VRational& Rhs);
	COREUOBJECT_API static VRational& Sub(FAllocationContext Context, VRational& Lhs, VRational& Rhs);
	COREUOBJECT_API static VRational& Mul(FAllocationContext Context, VRational& Lhs, VRational& Rhs);
	COREUOBJECT_API static VRational& Div(FAllocationContext Context, VRational& Lhs, VRational& Rhs);
	COREUOBJECT_API static VRational& Neg(FAllocationContext Context, VRational& N);
	COREUOBJECT_API static bool Eq(FAllocationContext Context, VRational& Lhs, VRational& Rhs);
	COREUOBJECT_API static bool Eq(FAllocationContext Context, VRational& Lhs, VInt Rhs);
	COREUOBJECT_API static bool Gt(FAllocationContext Context, VRational& Lhs, VRational& Rhs);
	COREUOBJECT_API static bool Lt(FAllocationContext Context, VRational& Lhs, VRational& Rhs);
	COREUOBJECT_API static bool Gte(FAllocationContext Context, VRational& Lhs, VRational& Rhs);
	COREUOBJECT_API static bool Lte(FAllocationContext Context, VRational& Lhs, VRational& Rhs);

	COREUOBJECT_API VInt Floor(FAllocationContext Context) const;
	COREUOBJECT_API VInt Ceil(FAllocationContext Context) const;

	COREUOBJECT_API void Reduce(FAllocationContext Context);
	COREUOBJECT_API void NormalizeSigns(FAllocationContext Context);
	bool IsZero() const { return Numerator.Get().IsZero(); }
	bool IsReduced() const { return bIsReduced; }

	static VRational& New(FAllocationContext Context, VInt InNumerator, VInt InDenominator)
	{
		return *new (Context.AllocateFastCell(sizeof(VRational))) VRational(Context, InNumerator, InDenominator);
	}

	COREUOBJECT_API ECompares EqualImpl(FAllocationContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);

	COREUOBJECT_API uint32 GetTypeHashImpl();

	COREUOBJECT_API void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);

	static void SerializeLayout(FAllocationContext Context, VRational*& This, FStructuredArchiveVisitor& Visitor);
	COREUOBJECT_API void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);

private:
	VRational(FAllocationContext Context, VInt InNumerator, VInt InDenominator)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, bIsReduced(false)
	{
		checkSlow(!InDenominator.IsZero());
		Numerator.Set(Context, InNumerator);
		Denominator.Set(Context, InDenominator);
	}

	bool bIsReduced;
};

} // namespace Verse
#endif // WITH_VERSE_VM
