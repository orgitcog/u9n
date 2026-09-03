// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/StringView.h"
#include "Misc/Optional.h"
#include "VerseVM/Inline/VVMMutableArrayInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMUniqueString.h"

namespace Verse
{
// A helper struct that maps strings to VValues
struct VNameValueMap
{
	VNameValueMap(FAllocationContext Context, uint32 Capacity)
		: NameAndValues(Context, &VMutableArray::New(Context, 0, Capacity, EArrayType::VValue))
	{
	}

	// We keep names at 2*Index and Values at 2*Index+1
	TWriteBarrier<VMutableArray> NameAndValues;

	uint32 Num() const
	{
		return NameAndValues->Num() / 2;
	}

	void Reset(FAllocationContext Context)
	{
		NameAndValues->Reset(Context);
	}

	VUniqueString& GetName(uint32 Index) const
	{
		checkSlow(Index < static_cast<int32>(Num()));
		VValue Value = NameAndValues->GetValue(2 * Index);
		return Value.StaticCast<VUniqueString>();
	}

	VValue GetValue(uint32 Index) const
	{
		checkSlow(Index < static_cast<int32>(Num()));
		return NameAndValues->GetValue(2 * Index + 1);
	}

	template <typename CellType>
	CellType& GetCell(uint32 Index) const
	{
		return GetValue(Index).StaticCast<CellType>();
	}

	void AddValue(FAllocationContext Context, VUniqueString& Name, VValue Value)
	{
		NameAndValues->AddValue(Context, VValue(Name));
		NameAndValues->AddValue(Context, Value);
	}

	TOptional<VValue> RemoveValue(FUtf8StringView Name)
	{
		uint32 Index = IndexOf(Name);
		if (Index == INDEX_NONE)
		{
			return {};
		}

		VValue RemovedValue = NameAndValues->GetValue(2 * Index + 1);
		NameAndValues->RemoveRange(2 * Index, 2);
		return RemovedValue;
	}

	uint32 IndexOf(FUtf8StringView Name) const
	{
		for (uint32 Index = 0, End = Num(); Index < End; ++Index)
		{
			if (GetName(Index).AsStringView().Equals(Name))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	VValue Lookup(FUtf8StringView Name) const
	{
		uint32 Index = IndexOf(Name);
		return Index != INDEX_NONE ? GetValue(Index) : VValue();
	}

	template <typename CellType>
	CellType* LookupCell(FUtf8StringView Name) const
	{
		VValue Value = Lookup(Name);
		if (Value.IsCell())
		{
			VCell& Cell = Value.AsCell();
			if (Cell.IsA<CellType>())
			{
				return &Cell.StaticCast<CellType>();
			}
		}
		return nullptr;
	}

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, VNameValueMap& Value)
	{
		Visitor.Visit(Value.NameAndValues, TEXT("NamesAndValues"));
	}
};
} // namespace Verse

#endif // WITH_VERSE_VM
