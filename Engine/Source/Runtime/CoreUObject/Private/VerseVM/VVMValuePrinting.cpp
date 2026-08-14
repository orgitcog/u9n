// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMValuePrinting.h"
#include "Containers/UnrealString.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMUniqueStringInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/VVMFloatPrinting.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMObjectPrinting.h"
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMRational.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMTextPrinting.h"
#include "VerseVM/VVMVar.h"
#include <inttypes.h>

namespace Verse
{
void VInt::AppendDecimalToString(FUtf8StringBuilderBase& Builder, FAllocationContext Context) const
{
	if (IsInt64())
	{
		const int64 Int64 = AsInt64();
		Builder << Int64;
	}
	else
	{
		StaticCast<VHeapInt>().AppendDecimalToString(Builder, Context);
	}
}

void VInt::AppendHexToString(FUtf8StringBuilderBase& Builder) const
{
	if (IsInt64())
	{
		const int64 Int64 = AsInt64();
		if (Int64 < 0)
		{
			Builder.Appendf(UTF8TEXT("-0x%x"), -Int64);
		}
		else
		{
			Builder.Appendf(UTF8TEXT("0x%x"), Int64);
		}
	}
	else
	{
		StaticCast<VHeapInt>().AppendHexToString(Builder);
	}
}

void VValue::AppendToString(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth) const
{
	// Don't deeply recurse into data structures to guard against reference cycles.
	if (RecursionDepth > 10)
	{
		Builder << UTF8TEXT("\"...\"");
		return;
	}

	if (IsInt())
	{
		if (IsCellFormat(Format) && *this == VValue::EffectDoneMarker())
		{
			AsInt().AppendHexToString(Builder);
		}
		else
		{
			AsInt().AppendDecimalToString(Builder, Context);
		}
	}
	else if (IsCell())
	{
		AsCell().AppendToString(Builder, Context, Format, RecursionDepth);
	}
	else if (IsUObject())
	{
		::Verse::AppendToString(Builder, AsUObject(), Format, RecursionDepth);
	}
	else if (IsPlaceholder())
	{
		VPlaceholder& Placeholder = AsPlaceholder();
		VValue Pointee = Placeholder.Follow();
		if (Format == EValueStringFormat::Verse || Format == EValueStringFormat::Diagnostic)
		{
			if (Pointee.IsPlaceholder())
			{
				Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"_\"") : UTF8TEXT("_"));
			}
			else
			{
				Pointee.AppendToString(Builder, Context, Format, RecursionDepth + 1);
			}
		}
		else
		{
			Builder.Appendf(UTF8TEXT("Placeholder(0x%" PRIxPTR "->"), &Placeholder);
			if (Pointee.IsPlaceholder())
			{
				Builder.Appendf(UTF8TEXT("0x%" PRIxPTR), &Pointee.AsPlaceholder());
			}
			else
			{
				Pointee.AppendToString(Builder, Context, Format, RecursionDepth + 1);
			}
			Builder.Append(TEXT(")"));
		}
	}
	else
	{
		const bool bNeedsQuotes = Format == EValueStringFormat::JSON && (!IsFloat() || !AsFloat().IsFinite());
		Builder.Append(bNeedsQuotes ? UTF8TEXT("\"") : UTF8TEXT(""));
		if (IsFloat())
		{
			AppendDecimalToString(Builder, AsFloat());
		}
		else if (IsChar())
		{
			AppendVerseToString(Builder, AsChar());
		}
		else if (IsChar32())
		{
			AppendVerseToString(Builder, AsChar32());
		}
		else if (IsRoot())
		{
			Builder.Appendf(UTF8TEXT("Root(%u)"), GetSplitDepth());
		}
		else if (IsUninitialized())
		{
			Builder.Append(UTF8TEXT("Uninitialized"));
		}
		else
		{
			V_DIE("Unhandled Verse value encoding: 0x%" PRIxPTR, GetEncodedBits());
		}
		Builder.Append(bNeedsQuotes ? UTF8TEXT("\"") : UTF8TEXT(""));
	}
}

void VRestValue::AppendToString(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth) const
{
	Value.Get().AppendToString(Builder, Context, Format, RecursionDepth);
}

void VCell::AppendToString(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	// Don't deeply recurse into data structures to guard against reference cycles.
	if (RecursionDepth > 10)
	{
		Builder << UTF8TEXT("\"...\"");
		return;
	}

	// Logical values are handled via two globally unique cells.
	// For concision, the cell formats omit the C++ class name to match the other formats as a special case.
	if (this == &GlobalFalse())
	{
		Builder << UTF8TEXT("false");
		return;
	}
	else if (this == &GlobalTrue())
	{
		Builder << UTF8TEXT("true");
		return;
	}

	VEmergentType* EmergentType = GetEmergentType();

	if (IsCellFormat(Format))
	{
		FString DebugName = EmergentType->CppClassInfo->DebugName();
		FStringView DebugNameWithoutV = DebugName;
		if (DebugName[0] == TEXT('V'))
		{
			DebugNameWithoutV = DebugNameWithoutV.Mid(1);
		}
		Builder << DebugNameWithoutV;
		if (Format == EValueStringFormat::CellsWithAddresses)
		{
			Builder << UTF8TEXT('@');
			Builder.Appendf(UTF8TEXT("0x%p"), this);
		}
		Builder << UTF8TEXT('(');
	}

	EmergentType->CppClassInfo->AppendToString(this, Builder, Context, Format, RecursionDepth);

	if (IsCellFormat(Format))
	{
		Builder << UTF8TEXT(')');
	}
}

void VCell::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	Builder << UTF8TEXT("\"");
	Builder << GetEmergentType()->CppClassInfo->DebugName();
	Builder << UTF8TEXT("\{}\"");
}

void AppendToString(VCell* Cell, FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	Cell->AppendToString(Builder, Context, Format, RecursionDepth);
}

void AppendToString(VValue Value, FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	Value.AppendToString(Builder, Context, Format, RecursionDepth);
}

FUtf8String VValue::ToString(FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth) const
{
	TUtf8StringBuilder<64> Builder;
	AppendToString(Builder, Context, Format, RecursionDepth);
	return FUtf8String(Builder);
}

FUtf8String VRestValue::ToString(FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth) const
{
	return Value.Get().ToString(Context, Format, RecursionDepth);
}

FUtf8String VCell::ToString(FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	TUtf8StringBuilder<64> Builder;
	AppendToString(Builder, Context, Format, RecursionDepth);
	return FUtf8String(Builder);
}

FUtf8String ToString(VCell* Cell, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	TUtf8StringBuilder<64> Builder;
	Cell->AppendToString(Builder, Context, Format, RecursionDepth);
	return FUtf8String(Builder);
}

FUtf8String ToString(VValue Value, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	TUtf8StringBuilder<64> Builder;
	Value.AppendToString(Builder, Context, Format, RecursionDepth);
	return FUtf8String(Builder);
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
