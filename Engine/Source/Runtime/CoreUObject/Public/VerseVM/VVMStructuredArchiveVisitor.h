// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "Misc/TVariant.h"
#include "Serialization/StructuredArchiveSlots.h"
#include "UObject/ObjectResource.h"
#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
template <typename T>
constexpr bool bAlwaysFalse = false;

struct FStructuredArchiveVisitor
{
	FStructuredArchiveVisitor(FAllocationContext InContext, FStructuredArchiveSlot InSlot)
		: Context(InContext)
		, Archive(InSlot.GetUnderlyingArchive())
		, CurrentSlot(TInPlaceType<FStructuredArchiveSlot>{}, InSlot)
	{
	}

	FStructuredArchiveVisitor(FAllocationContext InContext, FStructuredArchiveRecord InRecord)
		: Context(InContext)
		, Archive(InRecord.GetUnderlyingArchive())
		, CurrentSlot(TInPlaceType<FStructuredArchiveRecord>{}, InRecord)
	{
	}

	// Canonical visit methods.

	void Visit(VCell*& Value, const TCHAR* ElementName);
	void Visit(UObject*& Value, const TCHAR* ElementName);

	void Visit(VValue& Value, const TCHAR* ElementName);
	void Visit(VInt& Value, const TCHAR* ElementName);

	void Visit(bool& Value, const TCHAR* ElementName);
	void Visit(uint8& Value, const TCHAR* ElementName);
	void Visit(int32& Value, const TCHAR* ElementName);
	void Visit(uint32& Value, const TCHAR* ElementName);
	void Visit(double& Value, const TCHAR* ElementName);
	void Visit(FUtf8String& Value, const TCHAR* ElementName);
	void VisitBulkData(void* Data, uint64 DataSize, const TCHAR* ElementName);

	// Convenience methods. These forward to the canonical methods above.

	template <typename T>
	void Visit(TWriteBarrier<T>& Value, const TCHAR* ElementName)
	{
		if (IsLoading())
		{
			if constexpr (TWriteBarrier<T>::bIsVValue)
			{
				T ScratchValue{};
				Visit(ScratchValue, ElementName);
				Value.Set(Context, ScratchValue);
			}
			else if constexpr (!TWriteBarrier<T>::bIsAux)
			{
				VCell* ScratchValue = nullptr;
				Visit(ScratchValue, ElementName);
				Value.Set(Context, ScratchValue ? &ScratchValue->StaticCast<T>() : nullptr);
			}
			else
			{
				static_assert(bAlwaysFalse<T>, "Cannot serialize TAux");
			}
		}
		else
		{
			if constexpr (TWriteBarrier<T>::bIsVValue)
			{
				T ScratchValue = Value.Get();
				Visit(ScratchValue, ElementName);
			}
			else if constexpr (!TWriteBarrier<T>::bIsAux)
			{
				VCell* ScratchValue = Value.Get();
				Visit(ScratchValue, ElementName);
			}
			else
			{
				static_assert(bAlwaysFalse<T>, "Cannot serialize TAux");
			}
		}
	}

	template <typename T>
	void Visit(T& Value, const TCHAR* ElementName)
	{
		VisitObject(ElementName, [this, &Value] {
			using Verse::Visit;
			Visit(*this, Value);
		});
	}

	template <typename T>
	void Visit(T Begin, T End, const TCHAR* ElementName)
	{
		VisitArray(ElementName, [this, &Begin, &End] {
			for (; Begin != End; ++Begin)
			{
				auto&& Element = *Begin;
				Visit(Element, TEXT(""));
			}
		});
	}

	template <typename T>
	void Visit(T* Values, uint64 Count, const TCHAR* ElementName)
	{
		Visit(Values, Values + Count, ElementName);
	}

	template <typename F>
	void VisitObject(const TCHAR* ElementName, const F& VisitFields)
	{
		WithSlot(Slot(ElementName).EnterRecord(), VisitFields);
	}

	template <typename F>
	void VisitArray(const TCHAR* ElementName, const F& VisitElements)
	{
		WithSlot(Slot(ElementName).EnterStream(), VisitElements);
	}

	bool IsLoading()
	{
		return Archive.IsLoading();
	}

private:
	enum class EEncodedType : uint8
	{
		None,
		Cell,
		Object,
		Char,
		Char32,
		Float,
		Int,

		Count,
	};

	// Read/Write the element type description
	static const TArray<FName, TFixedAllocator<uint32(EEncodedType::Count)>>& EncodedTypeNames();
	void WriteElementType(FStructuredArchiveRecord Record, EEncodedType EncodedType);
	EEncodedType ReadElementType(FStructuredArchiveRecord Record);

	// Read/Write a value
	void WriteValueBody(FStructuredArchiveRecord Record, VValue InValue, bool bAllowBatch = false);
	VValue ReadValueBody(FStructuredArchiveRecord Record, EEncodedType EncodedType, bool bAllowBatch = false);

	FStructuredArchiveSlot Slot(const TCHAR* ElementName);

	template <typename SlotType, typename F>
	void WithSlot(SlotType Slot, const F& VisitSlot)
	{
		TVariant OriginalSlot = MoveTemp(CurrentSlot);
		CurrentSlot.Emplace<SlotType>(Slot);

		VisitSlot();

		CurrentSlot = MoveTemp(OriginalSlot);
	}

	FAllocationContext Context;
	FArchive& Archive;
	TVariant<FStructuredArchiveSlot, FStructuredArchiveRecord, FStructuredArchiveStream> CurrentSlot;
};

} // namespace Verse

#endif // WITH_VERSE_VM
