// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMIntType.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMRational.h"
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VIntType);
TGlobalTrivialEmergentTypePtr<&VIntType::StaticCppClassInfo> VIntType::GlobalTrivialEmergentType;

template <typename TVisitor>
void VIntType::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Min, TEXT("Min"));
	Visitor.Visit(Max, TEXT("Max"));
}

bool VIntType::SubsumesImpl(FAllocationContext Context, VValue Value)
{
	auto SubsumesInt = [&](VInt Int) -> bool {
		return (GetMin().IsUninitialized() || VInt::Lte(Context, GetMin(), Int))
			&& (GetMax().IsUninitialized() || VInt::Gte(Context, GetMax(), Int));
	};
	if (Value.IsInt())
	{
		return SubsumesInt(Value.AsInt());
	}
	else if (VRational* Rational = Value.DynamicCast<VRational>())
	{
		Rational->Reduce(Context);
		Rational->NormalizeSigns(Context);

		if (Rational->Denominator.Get() == VInt(1))
		{
			return SubsumesInt(Rational->Numerator.Get());
		}
	}
	return false;
}

void VIntType::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
	if (GetMin().IsUninitialized() && GetMax().IsUninitialized())
	{
		Builder << UTF8TEXT("int");
	}
	else if (GetMax().IsUninitialized() && GetMin().IsZero())
	{
		Builder << UTF8TEXT("nat");
	}
	else
	{
		Builder << UTF8TEXT("type{");
		if (GetMin().IsUninitialized())
		{
			Builder << UTF8TEXT(":int<=");
			GetMax().AppendToString(Builder, Context, Format, RecursionDepth + 1);
		}
		else if (GetMax().IsUninitialized())
		{
			Builder << UTF8TEXT(":int>=");
			GetMin().AppendToString(Builder, Context, Format, RecursionDepth + 1);
		}
		else if (GetMin() == GetMax())
		{
			GetMin().AppendToString(Builder, Context, Format, RecursionDepth + 1);
		}
		else
		{
			GetMin().AppendToString(Builder, Context, Format, RecursionDepth + 1);
			Builder << UTF8TEXT("..");
			GetMax().AppendToString(Builder, Context, Format, RecursionDepth + 1);
		}
		Builder << UTF8TEXT('}');
	}
	Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
}

void VIntType::SerializeLayout(FAllocationContext Context, VIntType*& This, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		This = &VIntType::New(Context, VInt(), VInt());
	}
}

void VIntType::SerializeImpl(FAllocationContext, FStructuredArchiveVisitor& Visitor)
{
	Visitor.Visit(Min, TEXT("Min"));
	Visitor.Visit(Max, TEXT("Max"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
