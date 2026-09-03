// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMFloatType.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMFloatPrinting.h"
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VFloatType);
DEFINE_TRIVIAL_VISIT_REFERENCES(VFloatType);
TGlobalTrivialEmergentTypePtr<&VFloatType::StaticCppClassInfo> VFloatType::GlobalTrivialEmergentType;

bool VFloatType::SubsumesImpl(FAllocationContext Context, VValue Value)
{
	if (!Value.IsFloat())
	{
		return false;
	}

	VFloat Float = Value.AsFloat();
	return GetMin() <= Float && (GetMax().IsNaN() || GetMax() >= Float);
}

void VFloatType::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	const bool bUnconstrainedMin = GetMin() == -VFloat::Infinity();
	// If there is a lower bound, then NaN is already excluded, and we can treat NaN and +Infinity as equivalently unconstrained upper bounds.
	const bool bUnconstrainedMax = GetMax().IsNaN()
								|| (GetMax().IsInfinite() && !bUnconstrainedMin);

	Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
	if (bUnconstrainedMin && bUnconstrainedMax)
	{
		Builder << UTF8TEXT("float");
	}
	else if (bUnconstrainedMin)
	{
		Builder << UTF8TEXT("type{:float<=");
		AppendDecimalToString(Builder, GetMax());
		Builder << UTF8TEXT('}');
	}
	else if (bUnconstrainedMax)
	{
		Builder << UTF8TEXT("type{:float>=");
		AppendDecimalToString(Builder, GetMin());
		Builder << UTF8TEXT('}');
	}
	else if (GetMin() == GetMax())
	{
		Builder << UTF8TEXT("type{");
		AppendDecimalToString(Builder, GetMin());
		Builder << UTF8TEXT('}');
	}
	else
	{
		Builder << UTF8TEXT("type{:float>=");
		AppendDecimalToString(Builder, GetMin());
		Builder << UTF8TEXT("<=");
		AppendDecimalToString(Builder, GetMax());
		Builder << UTF8TEXT('}');
	}
	Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
}

void VFloatType::SerializeLayout(FAllocationContext Context, VFloatType*& This, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		This = &VFloatType::New(Context, VFloat(), VFloat());
	}
}

void VFloatType::SerializeImpl(FAllocationContext, FStructuredArchiveVisitor& Visitor)
{
	double ScratchMin = Min.AsDouble();
	double ScratchMax = Max.AsDouble();
	Visitor.Visit(ScratchMin, TEXT("Min"));
	Visitor.Visit(ScratchMax, TEXT("Max"));
	if (Visitor.IsLoading())
	{
		Min = VFloat(ScratchMin);
		Max = VFloat(ScratchMax);
	}
}

} // namespace Verse

#endif