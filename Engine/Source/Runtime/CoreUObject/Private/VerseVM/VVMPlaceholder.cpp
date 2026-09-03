// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/VVMCppClassInfo.h"
namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VPlaceholder);
TGlobalTrivialEmergentTypePtr<&VPlaceholder::StaticCppClassInfo> VPlaceholder::GlobalTrivialEmergentType;

template <typename TVisitor>
void VPlaceholder::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Value, TEXT("Value"));
}

VValue VPlaceholder::Follow()
{
	const VPlaceholder* Current = this;
	while (true)
	{
		// TODO: Should we path compress? We should figure out if that makes sense
		// once we're fully transactional.
		if (Current->HasValue())
		{
			return Current->GetValue();
		}
		if (Current->HasSuspension())
		{
			return VValue::Placeholder(*Current);
		}
		Current = Current->GetParent();
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
