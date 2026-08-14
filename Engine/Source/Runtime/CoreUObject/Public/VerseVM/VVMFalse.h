// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VVMGlobalHeapPtr.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMOption.h"
#include "VVMType.h"

namespace Verse
{
enum class EValueStringFormat;

struct VFalse : VType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static void InitializeGlobals(FAllocationContext Context);

	COREUOBJECT_API void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);

	static constexpr bool SerializeIdentity = false;
	COREUOBJECT_API static void SerializeLayout(FAllocationContext Context, VFalse*& This, FStructuredArchiveVisitor& Visitor);
	COREUOBJECT_API void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);

private:
	VFalse(FAllocationContext Context)
		: VType(Context, &GlobalTrivialEmergentType.Get(Context))
	{
	}

	static VFalse& New(FAllocationContext Context)
	{
		return *new (Context.AllocateFastCell(sizeof(VFalse))) VFalse(Context);
	}
};

COREUOBJECT_API extern TGlobalHeapPtr<VFalse> GlobalFalsePtr;
COREUOBJECT_API extern TGlobalHeapPtr<VOption> GlobalTruePtr;

// True is represented as VOption(VFalse)
inline VOption& GlobalTrue()
{
	return *GlobalTruePtr.Get();
}

inline VFalse& GlobalFalse()
{
	return *GlobalFalsePtr.Get();
}

} // namespace Verse
#endif // WITH_VERSE_VM
