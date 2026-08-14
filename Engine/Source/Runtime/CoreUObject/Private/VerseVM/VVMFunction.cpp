// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMFunction.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMValueObjectInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMTextPrinting.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VFunction);
TGlobalTrivialEmergentTypePtr<&VFunction::StaticCppClassInfo> VFunction::GlobalTrivialEmergentType;

template <typename TVisitor>
void VFunction::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Procedure, TEXT("Procedure"));
	Visitor.Visit(Self, TEXT("Self"));
	Visitor.Visit(ParentScope, TEXT("ParentScope"));
}

void VFunction::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	if (IsCellFormat(Format))
	{
		Builder.Append(TEXT("Procedure="));
		Procedure->AppendToString(Builder, Context, Format, RecursionDepth + 1);
		if (Self)
		{
			Builder.Append(TEXT(", Self="));
			// NOTE: (yiliang.siew) `Self` should always be a class object instance, which should be a `VValueObject` or a `UObject`.
			// If no `Self` is present, it should be a `VFalse`.
			VValue SelfValue = Self.Get();
			SelfValue.AppendToString(Builder, Context, Format, RecursionDepth + 1);
		}
		if (ParentScope)
		{
			Builder.Append(TEXT(", ParentScope="));
			ParentScope->AppendToString(Builder, Context, Format, RecursionDepth + 1);
		}
	}
	else
	{
		Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
		Builder << Procedure->Name->AsStringView();
		Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
	}
}

void VFunction::SerializeLayout(FAllocationContext Context, VFunction*& This, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		This = new (Context.AllocateFastCell(sizeof(VFunction))) VFunction(Context, nullptr, VValue(), nullptr);
	}
}

void VFunction::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	Visitor.Visit(Procedure, TEXT("Procedure"));
	Visitor.Visit(Self, TEXT("Self"));
	Visitor.Visit(ParentScope, TEXT("ParentScope"));
}

bool VFunction::HasSelf() const
{
	return !Self.Get().IsUninitialized();
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
