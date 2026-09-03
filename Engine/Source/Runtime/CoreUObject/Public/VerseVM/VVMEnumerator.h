// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/StringFwd.h"
#include "VVMCell.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMWriteBarrier.h"

namespace Verse
{
enum class EValueStringFormat;
struct VEnumeration;
struct VUniqueString;

struct VEnumerator : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	int32 GetIntValue() const { return IntValue; }

	void SetEnumeration(FAccessContext Context, VEnumeration* InEnumeration)
	{
		checkf(!Enumeration.Get(), TEXT("Cannot set VEnumerator::Enumeration after it is initialized."));
		Enumeration.Set(Context, InEnumeration);
	}
	VEnumeration* GetEnumeration() const
	{
		return Enumeration.Get();
	}
	VUniqueString* GetName() const
	{
		return Name.Get();
	}

	static VEnumerator& New(FAllocationContext Context, VUniqueString* InName, int32 IntValue)
	{
		return *new (Context.AllocateFastCell(sizeof(VEnumerator))) VEnumerator(Context, InName, IntValue);
	}

	void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);
	static void SerializeLayout(FAllocationContext Context, VEnumerator*& This, FStructuredArchiveVisitor& Visitor);
	void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);

private:
	COREUOBJECT_API uint32 GetTypeHashImpl();

	VEnumerator(FAllocationContext Context, VUniqueString* InName, int32 InIntValue)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, Name(Context, InName)
		, IntValue(InIntValue)
	{
	}

	TWriteBarrier<VEnumeration> Enumeration;
	TWriteBarrier<VUniqueString> Name;
	int32 IntValue;
};

} // namespace Verse
#endif // WITH_VERSE_VM
