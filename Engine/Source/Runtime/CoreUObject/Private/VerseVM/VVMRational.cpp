// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMRational.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VRational);
TGlobalTrivialEmergentTypePtr<&VRational::StaticCppClassInfo> VRational::GlobalTrivialEmergentType;

VRational& VRational::Add(FAllocationContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get(), Rhs.Denominator.Get()))
	{
		return VRational::New(Context,
			VInt::Add(Context, Lhs.Numerator.Get(), Rhs.Denominator.Get()),
			Lhs.Denominator.Get());
	}

	return VRational::New(
		Context,
		VInt::Add(Context,
			VInt::Mul(Context, Lhs.Numerator.Get(), Rhs.Denominator.Get()),
			VInt::Mul(Context, Rhs.Numerator.Get(), Lhs.Denominator.Get())),
		VInt::Mul(Context, Lhs.Denominator.Get(), Rhs.Denominator.Get()));
}

VRational& VRational::Sub(FAllocationContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get(), Rhs.Denominator.Get()))
	{
		return VRational::New(Context,
			VInt::Sub(Context, Lhs.Numerator.Get(), Rhs.Denominator.Get()),
			Lhs.Denominator.Get());
	}

	return VRational::New(
		Context,
		VInt::Sub(Context,
			VInt::Mul(Context, Lhs.Numerator.Get(), Rhs.Denominator.Get()),
			VInt::Mul(Context, Rhs.Numerator.Get(), Lhs.Denominator.Get())),
		VInt::Mul(Context, Lhs.Denominator.Get(), Rhs.Denominator.Get()));
}

VRational& VRational::Mul(FAllocationContext Context, VRational& Lhs, VRational& Rhs)
{
	return VRational::New(Context,
		VInt::Mul(Context, Lhs.Numerator.Get(), Rhs.Numerator.Get()),
		VInt::Mul(Context, Lhs.Denominator.Get(), Rhs.Denominator.Get()));
}

VRational& VRational::Div(FAllocationContext Context, VRational& Lhs, VRational& Rhs)
{
	return VRational::New(Context,
		VInt::Mul(Context, Lhs.Numerator.Get(), Rhs.Denominator.Get()),
		VInt::Mul(Context, Lhs.Denominator.Get(), Rhs.Numerator.Get()));
}

VRational& VRational::Neg(FAllocationContext Context, VRational& N)
{
	return VRational::New(Context, VInt::Neg(Context, N.Numerator.Get()), N.Denominator.Get());
}

bool VRational::Eq(FAllocationContext Context, VRational& Lhs, VRational& Rhs)
{
	Lhs.Reduce(Context);
	Lhs.NormalizeSigns(Context);
	Rhs.Reduce(Context);
	Rhs.NormalizeSigns(Context);

	return VInt::Eq(Context, Lhs.Numerator.Get(), Rhs.Numerator.Get())
		&& VInt::Eq(Context, Lhs.Denominator.Get(), Rhs.Denominator.Get());
}

bool VRational::Eq(FAllocationContext Context, VRational& Lhs, VInt Rhs)
{
	Lhs.Reduce(Context);
	Lhs.NormalizeSigns(Context);

	return Lhs.Denominator.Get() == VInt(1)
		&& VInt::Eq(Context, Lhs.Numerator.Get(), Rhs);
}

bool VRational::Gt(FAllocationContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get(), Rhs.Denominator.Get()))
	{
		return VInt::Gt(Context, Lhs.Numerator.Get(), Rhs.Numerator.Get());
	}

	return VInt::Gt(Context,
		VInt::Mul(Context, Lhs.Numerator.Get(), Rhs.Denominator.Get()),
		VInt::Mul(Context, Rhs.Numerator.Get(), Lhs.Denominator.Get()));
}

bool VRational::Lt(FAllocationContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get(), Rhs.Denominator.Get()))
	{
		return VInt::Lt(Context, Lhs.Numerator.Get(), Rhs.Numerator.Get());
	}

	return VInt::Lt(Context,
		VInt::Mul(Context, Lhs.Numerator.Get(), Rhs.Denominator.Get()),
		VInt::Mul(Context, Rhs.Numerator.Get(), Lhs.Denominator.Get()));
}

bool VRational::Gte(FAllocationContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get(), Rhs.Denominator.Get()))
	{
		return VInt::Gte(Context, Lhs.Numerator.Get(), Rhs.Numerator.Get());
	}

	return VInt::Gte(Context,
		VInt::Mul(Context, Lhs.Numerator.Get(), Rhs.Denominator.Get()),
		VInt::Mul(Context, Rhs.Numerator.Get(), Lhs.Denominator.Get()));
}

bool VRational::Lte(FAllocationContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get(), Rhs.Denominator.Get()))
	{
		return VInt::Lte(Context, Lhs.Numerator.Get(), Rhs.Numerator.Get());
	}

	return VInt::Lte(Context,
		VInt::Mul(Context, Lhs.Numerator.Get(), Rhs.Denominator.Get()),
		VInt::Mul(Context, Rhs.Numerator.Get(), Lhs.Denominator.Get()));
}

VInt VRational::Floor(FAllocationContext Context) const
{
	VInt IntNumerator(Numerator.Get());
	VInt IntDenominator(Denominator.Get());
	bool bHasNonZeroRemainder = false;
	VInt IntQuotient = VInt::Div(Context, IntNumerator, IntDenominator, &bHasNonZeroRemainder);
	if (bHasNonZeroRemainder && (IntNumerator.IsNegative() != IntDenominator.IsNegative()))
	{
		IntQuotient = VInt::Sub(Context, IntQuotient, VInt(1));
	}
	return IntQuotient;
}

VInt VRational::Ceil(FAllocationContext Context) const
{
	VInt IntNumerator(Numerator.Get());
	VInt IntDenominator(Denominator.Get());
	bool bHasNonZeroRemainder = false;
	VInt IntQuotient = VInt::Div(Context, IntNumerator, IntDenominator, &bHasNonZeroRemainder);
	if (bHasNonZeroRemainder && (IntNumerator.IsNegative() == IntDenominator.IsNegative()))
	{
		IntQuotient = VInt::Add(Context, IntQuotient, VInt(1));
	}
	return IntQuotient;
}

void VRational::Reduce(FAllocationContext Context)
{
	if (bIsReduced)
	{
		return;
	}

	VInt A = Numerator.Get();
	VInt B = Denominator.Get();
	while (!B.IsZero())
	{
		VInt Remainder = VInt::Mod(Context, A, B);
		A = B;
		B = Remainder;
	}

	VInt NewNumerator = VInt::Div(Context, Numerator.Get(), A);
	VInt NewDenominator = VInt::Div(Context, Denominator.Get(), A);

	Numerator.Set(Context, NewNumerator);
	Denominator.Set(Context, NewDenominator);
	bIsReduced = true;
}

void VRational::NormalizeSigns(FAllocationContext Context)
{
	VInt Denom = Denominator.Get();
	if (Denom.IsNegative())
	{
		// The denominator is < 0, so we need to normalize the signs
		VInt NewNumerator = VInt::Neg(Context, Numerator.Get());
		VInt NewDenominator = VInt::Neg(Context, Denom);

		Numerator.Set(Context, NewNumerator);
		Denominator.Set(Context, NewDenominator);
	}
}

template <typename TVisitor>
void VRational::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Numerator, TEXT("Numerator"));
	Visitor.Visit(Denominator, TEXT("Denominator"));
}

void VRational::SerializeLayout(FAllocationContext Context, VRational*& This, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		This = &VRational::New(Context, VInt(0), VInt(1));
	}
}

void VRational::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	Visitor.Visit(Numerator, TEXT("Numerator"));
	Visitor.Visit(Denominator, TEXT("Denominator"));
}

ECompares VRational::EqualImpl(FAllocationContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VRational>())
	{
		return ECompares::Neq;
	}
	return Eq(Context, *this, Other->StaticCast<VRational>()) ? ECompares::Eq : ECompares::Neq;
}

uint32 VRational::GetTypeHashImpl()
{
	if (!bIsReduced)
	{
		// TLS lookup to reduce rationals before hashing
		// FRunningContextPromise PromiseContext;
		FRunningContext Context((FRunningContextPromise()));
		Reduce(Context);
		NormalizeSigns(Context);
	}

	const uint32 NumeratorHash = GetTypeHash(Numerator.Get());
	return Denominator.Get() == VInt(1)
			 ? NumeratorHash
			 : ::HashCombineFast(NumeratorHash, GetTypeHash(Denominator.Get()));
}

void VRational::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	Reduce(Context);
	NormalizeSigns(Context);

	Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
	Numerator.Get().AppendDecimalToString(Builder, Context);
	if (!Numerator.Get().IsZero() && Denominator.Get() != VInt(1))
	{
		Builder << UTF8TEXT('/');
		Denominator.Get().AppendDecimalToString(Builder, Context);
	}
	Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)