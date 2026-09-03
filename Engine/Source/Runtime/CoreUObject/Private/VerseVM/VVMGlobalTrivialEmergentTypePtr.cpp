// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/VVMGlobalHeapRoot.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMType.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{

struct FGlobalTrivialEmergentTypePtrRoot : FGlobalHeapRoot
{
	FGlobalTrivialEmergentTypePtrRoot(FAccessContext Context, VEmergentType* Type)
		: EmergentType(Context, Type)
	{
	}

	COREUOBJECT_API void Visit(FMarkStackVisitor& Visitor) override
	{
		Visitor.Visit(EmergentType, TEXT("EmergentType"));
	}

	TWriteBarrier<VEmergentType> EmergentType;
};

VEmergentType& FGlobalTrivialEmergentTypePtr::GetSlow(FAllocationContext Context, VCppClassInfo* ClassInfo, bool bWithShape)
{
	VShape* Shape = bWithShape ? VShape::New(Context, {}) : nullptr;
	VEmergentType* Object = VEmergentType::New(Context, Shape, VTrivialType::Singleton.Get(), ClassInfo);
	VEmergentType* Expected = nullptr;
	EmergentType.compare_exchange_strong(Expected, Object);
	VEmergentType* Result;
	if (Expected)
	{
		Result = Expected;
	}
	else
	{
		Result = Object;
		new FGlobalTrivialEmergentTypePtrRoot(Context, Object);
	}
	V_DIE_UNLESS(EmergentType.load() == Result);
	return *Result;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
