// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMTaskNativeHook.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VTaskNativeHook);
TGlobalTrivialEmergentTypePtr<&VTaskNativeHook::StaticCppClassInfo> VTaskNativeHook::GlobalTrivialEmergentType;

template <typename TVisitor>
void VTaskNativeHook::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Next, TEXT("Next"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)