// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMScope.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMScopeInline.h"
#include "VerseVM/VVMStructuredArchiveVisitor.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VScope)
TGlobalTrivialEmergentTypePtr<&VScope::StaticCppClassInfo> VScope::GlobalTrivialEmergentType;

template <typename TVisitor>
void VScope::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(SuperClass, TEXT("SuperClass"));
}

void VScope::SerializeLayout(FAllocationContext Context, VScope*& This, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		This = &VScope::New(Context, nullptr);
	}
}

void VScope::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	Visitor.Visit(SuperClass, TEXT("SuperClass"));
}
} // namespace Verse

#endif
