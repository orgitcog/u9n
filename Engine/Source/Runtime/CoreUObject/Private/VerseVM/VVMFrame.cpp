// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMFrame.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMProcedure.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VFrame);
TGlobalTrivialEmergentTypePtr<&VFrame::StaticCppClassInfo> VFrame::GlobalTrivialEmergentType;

template <typename TVisitor>
void VFrame::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(CallerFrame, TEXT("CallerFrame"));
	Visitor.Visit(ReturnSlot, TEXT("ReturnSlot"));
	Visitor.Visit(Procedure, TEXT("Procedure"));
	Visitor.Visit(Registers, NumRegisters, TEXT("Registers"));
}

TGlobalHeapPtr<VFrame> VFrame::GlobalEmptyFrame;

void VFrame::InitializeGlobals(FAllocationContext Context)
{
	VProcedure& EmptyProcedure = VProcedure::NewUninitialized(Context, 0, 0, 0, 0, 0, 0, 0, 0);
	GlobalEmptyFrame.Set(Context, &VFrame::New(Context, nullptr, nullptr, nullptr, EmptyProcedure));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
