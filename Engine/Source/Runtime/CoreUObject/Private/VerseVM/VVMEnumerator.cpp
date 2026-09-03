// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMEnumerator.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEnumeration.h"
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "VerseVM/VVMTextPrinting.h"
#include "VerseVM/VVMUniqueString.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VEnumerator);
TGlobalTrivialEmergentTypePtr<&VEnumerator::StaticCppClassInfo> VEnumerator::GlobalTrivialEmergentType;

template <typename TVisitor>
void VEnumerator::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Enumeration, TEXT("Enumeration"));
	Visitor.Visit(Name, TEXT("Name"));
}

uint32 VEnumerator::GetTypeHashImpl()
{
	return PointerHash(this);
}

void VEnumerator::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
	GetEnumeration()->AppendQualifiedName(Builder);
	Builder << UTF8TEXT('.');
	Builder << GetName()->AsStringView();
	Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
}

void VEnumerator::SerializeLayout(FAllocationContext Context, VEnumerator*& This, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		This = &VEnumerator::New(Context, nullptr, 0);
	}
}

void VEnumerator::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	Visitor.Visit(Enumeration, TEXT("Enumeration"));
	Visitor.Visit(Name, TEXT("Name"));
	Visitor.Visit(IntValue, TEXT("IntValue"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
