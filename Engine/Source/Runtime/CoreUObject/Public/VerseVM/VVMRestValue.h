// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VVMValue.h"
#include "VVMWriteBarrier.h"

namespace Verse
{
struct VFrame;

enum class EDefaultConstructVRestValue
{
	UnsafeDoNotUse
}; // So we can construct empty VRestValues for VNI

struct VRestValue
{
	VRestValue(EDefaultConstructVRestValue) {}
	VRestValue(const VRestValue&) = default;
	VRestValue& operator=(const VRestValue&) = default;

	explicit VRestValue(uint16 SplitDepth)
	{
		Reset(SplitDepth);
	}

	void Reset(uint16 SplitDepth)
	{
		SetNonCellNorPlaceholder(VValue::Root(SplitDepth));
	}

	void Set(FAccessContext Context, VValue NewValue)
	{
		checkSlow(!NewValue.IsRoot());
		Value.Set(Context, NewValue);
	}

	void SetTransactionally(FAllocationContext Context, VValue NewValue);

	void SetNonCellNorPlaceholder(VValue NewValue)
	{
		Value.SetNonCellNorPlaceholder(NewValue);
	}

	bool CanDefQuickly() const
	{
		return Value.Get().IsRoot();
	}

	VValue Get(FAllocationContext Context);
	COREUOBJECT_API VValue GetSlow(FAllocationContext Context);
	inline bool IsUninitialized() const
	{
		return Value.Get().IsUninitialized();
	}

	bool operator==(const VRestValue& Other) const;

	COREUOBJECT_API void AppendToString(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth = 0) const;
	COREUOBJECT_API FUtf8String ToString(FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth = 0) const;

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, VRestValue& InValue)
	{
		Visitor.Visit(InValue.Value, TEXT(""));
	}

	friend uint32 GetTypeHash(VRestValue RestValue);

private:
	VRestValue() = default;
	TWriteBarrier<VValue> Value;

	friend struct VArray;
	friend struct VFrame;
	friend struct VObject;
	friend struct VValue;
	friend struct VValueObject;
	friend struct VArchetype;
	friend struct VNativeConstructorWrapper;
	friend struct FStructuredArchiveVisitor;
};
} // namespace Verse
#endif // WITH_VERSE_VM
