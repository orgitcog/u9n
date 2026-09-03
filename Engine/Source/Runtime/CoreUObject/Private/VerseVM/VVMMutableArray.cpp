// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMMutableArrayInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMOpResult.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VMutableArray);
DEFINE_TRIVIAL_VISIT_REFERENCES(VMutableArray);
TGlobalTrivialEmergentTypePtr<&VMutableArray::StaticCppClassInfo> VMutableArray::GlobalTrivialEmergentType;

void VMutableArray::Reset(FAllocationContext Context)
{
	SetBufferWithStoreBarrier(Context, VBuffer());
}

void VMutableArray::RemoveRange(uint32 StartIndex, uint32 Count)
{
	checkSlow(Num() - StartIndex >= Count);

	uint32 Remainder = Num() - StartIndex - Count;
	if (Remainder > 0)
	{
		// Copy the remainder of the array to close the gap
		uint8* Data = GetData<uint8>();
		const EArrayType ArrayType = GetArrayType();
		switch (ArrayType)
		{
			case EArrayType::VValue:
			{
				// Since VValues can contain pointers, and GC might run concurrently
				// we make sure to copy entire VValues at a time so no element is in an invalid state at any time
				// (assuming we run on a 64-bit processor which is true for all platforms currently supported by UE)
				// GC might see the same element twice but that doesn't hurt
				checkSlow(IsAligned(Data, sizeof(VValue)));
				static_assert(sizeof(VValue) == sizeof(uint64), "");
				uint64* Dst = BitCast<uint64*>(Data) + StartIndex;
				uint64* Src = Dst + Count;
				for (uint32 Index = 0; Index < Remainder; ++Index)
				{
					Dst[Index] = Src[Index];
				}
				break;
			}
			case EArrayType::Int32:
			case EArrayType::Char8:
			case EArrayType::Char32:
			case EArrayType::None:
			default:
			{
				// All other types aren't of interest to GC so just move them as we please
				uint32 ElementSize = ::Verse::ByteLength(ArrayType);
				uint8* Dst = Data + StartIndex * ElementSize;
				uint8* Src = Dst + Count * ElementSize;
				FMemory::Memmove(Dst, Src, Remainder * ElementSize);
				break;
			}
		}
	}

	// Now, shrink the buffer
	// No fence is needed since it doesn't matter if GC sees the old or new state
	Buffer.Get().GetHeader()->NumValues -= Count;
}

void VMutableArray::Append(FAllocationContext Context, VArrayBase& Array)
{
	if (!Buffer && Array.Num())
	{
		uint32 Num = 0;
		uint32 Capacity = Array.Num();
		VBuffer NewBuffer = VBuffer(Context, Num, Capacity, Array.GetArrayType());
		// We barrier because the GC needs to see the store to ArrayType/Num if
		// it sees the new buffer.
		SetBufferWithStoreBarrier(Context, NewBuffer);
	}
	else if (GetArrayType() != EArrayType::VValue && GetArrayType() != Array.GetArrayType())
	{
		ConvertDataToVValues(Context, Num() + Array.Num());
	}

	switch (GetArrayType())
	{
		case EArrayType::None:
			V_DIE_UNLESS(Array.GetArrayType() == EArrayType::None);
			// Empty-Untyped VMutableArray appending Empty-Untyped VMutableArray
			break;
		case EArrayType::VValue:
			Append<TWriteBarrier<VValue>>(Context, Array);
			break;
		case EArrayType::Int32:
			Append<int32>(Context, Array);
			break;
		case EArrayType::Char8:
			Append<UTF8CHAR>(Context, Array);
			break;
		case EArrayType::Char32:
			Append<UTF32CHAR>(Context, Array);
			break;
		default:
			V_DIE("Unhandled EArrayType encountered!");
	}
}

FOpResult VMutableArray::FreezeImpl(FAllocationContext Context)
{
	EArrayType ArrayType = GetArrayType();
	VArray& FrozenArray = VArray::New(Context, Num(), ArrayType);
	if (ArrayType != EArrayType::VValue)
	{
		if (Num())
		{
			FMemory::Memcpy(FrozenArray.GetData(), GetData(), ByteLength());
		}
	}
	else
	{
		for (uint32 I = 0; I < Num(); ++I)
		{
			FOpResult Result = VValue::Freeze(Context, GetValue(I));
			V_DIE_UNLESS(Result.IsReturn()); // Values inside a Verse-native array should always be valid.
			FrozenArray.SetValue(Context, I, Result.Value);
		}
	}
	V_RETURN(FrozenArray);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
