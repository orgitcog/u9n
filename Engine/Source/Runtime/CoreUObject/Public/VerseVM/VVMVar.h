// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMRestValue.h"
#include "VVMType.h"

namespace Verse
{
enum class EValueStringFormat;

struct VVar : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static VVar& New(FAllocationContext Context)
	{
		return *new (Context.AllocateFastCell(sizeof(VVar))) VVar(Context);
	}

	VValue Get(FAllocationContext Context)
	{
		return Value.Get(Context);
	}

	void Set(FAllocationContext Context, VValue NewValue);

	void SetNonTransactionally(FAccessContext Context, VValue NewValue)
	{
		return Value.Set(Context, NewValue);
	}

	COREUOBJECT_API void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);

private:
	VRestValue Value;

	VVar(FAllocationContext Context)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		// TODO: Figure out what split depth meets here.
		, Value(0)
	{
	}
};

} // namespace Verse
#endif // WITH_VERSE_VM
