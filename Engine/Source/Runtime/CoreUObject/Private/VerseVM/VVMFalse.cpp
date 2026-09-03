// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMFalse.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VFalse);
DEFINE_TRIVIAL_VISIT_REFERENCES(VFalse);
TGlobalTrivialEmergentTypePtr<&VFalse::StaticCppClassInfo> VFalse::GlobalTrivialEmergentType;

TGlobalHeapPtr<VFalse> GlobalFalsePtr;
TGlobalHeapPtr<VOption> GlobalTruePtr;

void VFalse::InitializeGlobals(Verse::FAllocationContext Context)
{
	GlobalFalsePtr.Set(Context, &VFalse::New(Context));

	VValue True(*GlobalFalsePtr.Get());
	GlobalTruePtr.Set(Context, &VOption::New(Context, True));
}

void VFalse::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	Builder << UTF8TEXT("false");
}

void VFalse::SerializeLayout(FAllocationContext Context, VFalse*& This, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		This = &GlobalFalse();
	}
}

void VFalse::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)