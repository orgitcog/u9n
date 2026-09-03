// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMArrayBase.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMDebuggerVisitor.h"
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/VVMOpResult.h"
#include "VerseVM/VVMTextPrinting.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VArrayBase);

template <typename TVisitor>
void VArrayBase::VisitReferencesImpl(TVisitor& Visitor)
{
	VBuffer ThisBuffer = Buffer.Get();
	Visitor.VisitAux(ThisBuffer.GetPtr(), TEXT("Buffer")); // Visit the buffer we allocated for the array as Aux memory

	if (ThisBuffer.GetArrayType() == EArrayType::VValue) // Check if we contain elements requiring marking
	{
		// This can race with the mutator while the mutator is growing the array.
		// The reason we don't read garbage VValues is that the mutator will fence
		// between storing the new Value and incrementing Num. So the GC is guaranteed
		// to see the new VValue before it sees the new Num. Therefore, the array the
		// GC sees here is guaranteed to have non-garbage VValues from 0..Num.
		//
		// It's also OK if the GC misses VValues that the mutator adds because the
		// mutator will barrier those new VValues.
		//
		// TODO: In the future we need to support concurrently shrinking arrays.
		// This will happen in the future for two reasons:
		// - STM rollback.
		// - We'll eventually add Verse stdlib APIs that allow elements to be removed from arrays.
		Visitor.Visit(ThisBuffer.GetData<TWriteBarrier<VValue>>(), ThisBuffer.Num(), TEXT("Elements"));
	}
}

ECompares VArrayBase::EqualImpl(FAllocationContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VArrayBase>())
	{
		return ECompares::Neq;
	}

	VArrayBase& OtherArray = Other->StaticCast<VArrayBase>();
	if (Num() != OtherArray.Num())
	{
		return ECompares::Neq;
	}

	if (DetermineCombinedType(GetArrayType(), OtherArray.GetArrayType()) != EArrayType::VValue)
	{
		if (Num() && FMemory::Memcmp(GetData(), OtherArray.GetData(), ByteLength()) != 0)
		{
			return ECompares::Neq;
		}
	}
	else
	{
		for (uint32 Index = 0, End = Num(); Index < End; ++Index)
		{
			if (ECompares Cmp = VValue::Equal(Context, GetValue(Index), OtherArray.GetValue(Index), HandlePlaceholder); Cmp != ECompares::Eq)
			{
				return Cmp;
			}
		}
	}
	return ECompares::Eq;
}

uint32 VArrayBase::GetTypeHashImpl()
{
	switch (GetArrayType())
	{
		case EArrayType::None:
			return 0; // Empty-Untyped VMutableArray
		case EArrayType::VValue:
			return ::GetArrayHash(GetData<TWriteBarrier<VValue>>(), Num());
		case EArrayType::Int32:
			return ::GetArrayHash(GetData<int32>(), Num());
		case EArrayType::Char8:
			return ::GetArrayHash(GetData<UTF8CHAR>(), Num());
		case EArrayType::Char32:
			return ::GetArrayHash(GetData<UTF32CHAR>(), Num());
		default:
			V_DIE("Unhandled EArrayType encountered!");
	}
}

VValue VArrayBase::MeltImpl(FAllocationContext Context)
{
	EArrayType ArrayType = GetArrayType();
	if (ArrayType != EArrayType::VValue)
	{
		VMutableArray& MeltedArray = VMutableArray::New(Context, Num(), Num(), ArrayType);
		if (Num())
		{
			FMemory::Memcpy(MeltedArray.GetData(), GetData(), ByteLength());
		}
		return MeltedArray;
	}

	VMutableArray& MeltedArray = VMutableArray::New(Context, 0, Num(), EArrayType::VValue);
	for (uint32 I = 0; I < Num(); ++I)
	{
		VValue Result = VValue::Melt(Context, GetValue(I));
		if (Result.IsPlaceholder())
		{
			return Result;
		}
		MeltedArray.AddValue(Context, Result);
	}
	return MeltedArray;
}

void VArrayBase::VisitMembersImpl(FAllocationContext Context, FDebuggerVisitor& Visitor)
{
	Visitor.VisitArray([this, &Visitor] {
		for (VValue Element : *this)
		{
			Visitor.Visit(Element, "");
		}
	});
}

void VArrayBase::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	// We print UTF8 Arrays as strings for ease of reading when debugging and logging.
	if (Num() || ::Verse::IsNullTerminatedString(GetArrayType()))
	{
		if (TOptional<FUtf8String> Utf8String = AsOptionalUtf8String())
		{
			AppendVerseToString(Builder, *Utf8String);
			return;
		}
	}

	FUtf8StringView Terminator = UTF8TEXT("");
	if (!IsCellFormat(Format))
	{
		if (Format == EValueStringFormat::JSON)
		{
			Builder << UTF8TEXT("[");
			Terminator = UTF8TEXT("]");
		}
		else if (Num() == 1)
		{
			Builder << UTF8TEXT("array{");
			Terminator = UTF8TEXT("}");
		}
		else
		{
			Builder << UTF8TEXT('(');
			Terminator = UTF8TEXT(")");
		}
	}

	FUtf8StringView Separator = UTF8TEXT("");
	for (VValue Element : *this)
	{
		Builder << Separator;
		Separator = UTF8TEXT(", ");

		Element.AppendToString(Builder, Context, Format, RecursionDepth + 1);
	}

	Builder << Terminator;
}

void VArrayBase::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	uint32 NumValues = Num();
	Visitor.Visit(NumValues, TEXT("NumValues"));

	std::underlying_type_t<EArrayType> ArrayType = static_cast<std::underlying_type_t<EArrayType>>(GetArrayType());
	if (NumValues == 0)
	{
		ArrayType = static_cast<std::underlying_type_t<EArrayType>>(EArrayType::None);
	}
	Visitor.Visit(ArrayType, TEXT("ArrayType"));

	if (Visitor.IsLoading() && NumValues)
	{
		SetBufferWithStoreBarrier(Context, VBuffer(Context, NumValues, NumValues, EArrayType{ArrayType}));
	}

	if (GetArrayType() != EArrayType::VValue)
	{
		Visitor.VisitBulkData(GetData(), ByteLength(), TEXT("Elements"));
	}
	else
	{
		Visitor.Visit(GetData<TWriteBarrier<VValue>>(), Num(), TEXT("Elements"));
	}
}

VArrayBase::FConstIterator VArrayBase::begin() const
{
	switch (GetArrayType())
	{
		case EArrayType::None:
			return GetData(); // Empty-Untyped VMutableArray
		case EArrayType::VValue:
			return GetData<TWriteBarrier<VValue>>();
		case EArrayType::Int32:
			return GetData<int32>();
		case EArrayType::Char8:
			return GetData<UTF8CHAR>();
		case EArrayType::Char32:
			return GetData<UTF32CHAR>();
		default:
			V_DIE("Unhandled EArrayType encountered!");
	}
}

VArrayBase::FConstIterator VArrayBase::end() const
{
	switch (GetArrayType())
	{
		case EArrayType::None:
			return GetData(); // Empty-Untyped VMutableArray
		case EArrayType::VValue:
			return GetData<TWriteBarrier<VValue>>() + Num();
		case EArrayType::Int32:
			return GetData<int32>() + Num();
		case EArrayType::Char8:
			return GetData<UTF8CHAR>() + Num();
		case EArrayType::Char32:
			return GetData<UTF32CHAR>() + Num();
		default:
			V_DIE("Unhandled EArrayType encountered!");
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
