// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMap.h"
#include "Async/ExternalMutex.h"
#include "Async/UniqueLock.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMMapInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/Inline/VVMVarInline.h"
#include "VerseVM/VVMDebuggerVisitor.h"
#include "VerseVM/VVMOpResult.h"
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "VerseVM/VVMTransaction.h"
#include "VerseVM/VVMValuePrinting.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VMapBase);

template <typename TVisitor>
void VMapBase::VisitReferencesImpl(TVisitor& Visitor)
{
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);

	Visitor.VisitAux(Data.Get().GetPtr(), TEXT("Data")); // Visit the buffer we allocated for the array as Aux memory
	Visitor.VisitAux(SequenceData.Get().GetPtr(), TEXT("SequenceData"));
	Visitor.Visit(begin(), end(), TEXT("Elements"));
}

ECompares VMapBase::EqualImpl(FAllocationContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VMapBase>())
	{
		return ECompares::Neq;
	}

	VMapBase& OtherMap = Other->StaticCast<VMapBase>();
	if (Num() != OtherMap.Num())
	{
		return ECompares::Neq;
	}

	for (int32 i = 0; i < Num(); ++i)
	{
		VValue K0 = GetKey(i);
		VValue K1 = OtherMap.GetKey(i);
		VValue V0 = GetValue(i);
		VValue V1 = OtherMap.GetValue(i);
		if (ECompares CmpKey = VValue::Equal(Context, K0, K1, HandlePlaceholder); CmpKey != ECompares::Eq)
		{
			return CmpKey;
		}
		if (ECompares CmpVal = VValue::Equal(Context, V0, V1, HandlePlaceholder); CmpVal != ECompares::Eq)
		{
			return CmpVal;
		}
	}
	return ECompares::Eq;
}

uint32 VMapBase::GetTypeHashImpl()
{
	uint32 Result = 0;
	for (auto MapIt : *this)
	{
		Result = ::HashCombineFast(Result, ::HashCombineFast(GetTypeHash(MapIt.Key), GetTypeHash(MapIt.Value)));
	}
	return Result;
}

void VMapBase::VisitMembersImpl(FAllocationContext Context, FDebuggerVisitor& Visitor)
{
	Visitor.VisitMap([this, &Visitor] {
		for (TPair<VValue, VValue> Pair : *this)
		{
			Visitor.Visit(Pair.Key, "Key");
			Visitor.Visit(Pair.Value, "Value");
		}
	});
}

void VMapBase::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	const bool bJSON = Format == EValueStringFormat::JSON;
	if (!IsCellFormat(Format))
	{
		Builder << (bJSON ? UTF8TEXT("{") : UTF8TEXT("map{"));
	}

	FUtf8StringView Separator = UTF8TEXT("");
	for (TPair<VValue, VValue> Pair : *this)
	{
		Builder << Separator;
		Separator = UTF8TEXT(", ");

		Pair.Key.AppendToString(Builder, Context, Format, RecursionDepth + 1);
		Builder << (bJSON ? UTF8TEXT(": ") : UTF8TEXT(" => "));
		Pair.Value.AppendToString(Builder, Context, Format, RecursionDepth + 1);
	}

	if (!IsCellFormat(Format))
	{
		Builder << UTF8TEXT("}");
	}
}

void VMapBase::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		int32 ScratchNumElements = 0;
		Visitor.Visit(ScratchNumElements, TEXT("NumElements"));
		Visitor.VisitArray(TEXT("Elements"), [this, Context, &Visitor, ScratchNumElements] {
			Reserve(Context, ScratchNumElements * 2);
			for (uint64 I = 0; I < ScratchNumElements; ++I)
			{
				TPair<VValue, VValue> Pair;
				Visitor.Visit(Pair, TEXT(""));

				uint32 KeyHash = GetTypeHash(Pair.Key);
				AddWithoutLocking(Context, KeyHash, Pair.Key, Pair.Value);
			}
		});
	}
	else
	{
		Visitor.Visit(NumElements, TEXT("NumElements"));
		Visitor.Visit(begin(), end(), TEXT("Elements"));
	}
}

inline VValue FindInPairDataByHashWithSlot(FAllocationContext Context, VMapBase::PairType* PairData, uint32 Capacity, uint32 Hash, VValue Key, uint32* OutSlot)
{
	check(Capacity > 0);
	uint32 HashMask = Capacity - 1;
	uint32 Slot = Hash & HashMask;
	uint32 LoopCount = 0;
	while (!PairData[Slot].Key.Get().IsUninitialized() && LoopCount++ < Capacity)
	{
		if (ECompares Cmp = VValue::Equal(Context, PairData[Slot].Key.Get(), Key, [](VValue Left, VValue Right) {}); Cmp == ECompares::Eq)
		{
			*OutSlot = Slot;
			return PairData[Slot].Value.Get();
		}
		Slot = (Slot + 1) & HashMask; // dumb linear probe, @TODO: something better
	}
	*OutSlot = Slot;
	return VValue();
}

VValue VMapBase::FindByHashWithSlot(FAllocationContext Context, uint32 Hash, VValue Key, uint32* OutSlot)
{
	return FindInPairDataByHashWithSlot(Context, GetPairTable(), Capacity, Hash, Key, OutSlot);
}

void VMapBase::Reserve(FAllocationContext Context, uint32 InCapacity)
{
	uint32 NewCapacity = FMath::RoundUpToPowerOfTwo(InCapacity < 8 ? 8 : InCapacity);
	if (NewCapacity <= Capacity)
	{
		return; // should we support shrinking?
	}
	TAux<PairType> NewData = TAux<PairType>(Context.AllocateAuxCell(GetPairTableSizeForCapacity(NewCapacity)));
	TAux<SequenceType> NewSequenceData = TAux<SequenceType>(Context.AllocateAuxCell(GetSequenceTableSizeForCapacity(NewCapacity)));

	FMemory::Memzero(NewData.GetPtr(), GetPairTableSizeForCapacity(NewCapacity));

	if (Data)
	{
		PairType* OldPairTable = GetPairTable();
		SequenceType* OldSequenceTable = GetSequenceTable();

		PairType* NewPairTable = static_cast<PairType*>(NewData.GetPtr());
		SequenceType* NewSequenceTable = static_cast<SequenceType*>(NewSequenceData.GetPtr());

		int32 NumElementsInsertedIntoNewData = 0;

		for (int32 ElemIdx = 0; ElemIdx < NumElements; ++ElemIdx)
		{
			PairType* OldPair = OldPairTable + OldSequenceTable[ElemIdx];
			uint32 NewSlot;
			VValue ExistingValInNewTable = FindInPairDataByHashWithSlot(Context, NewPairTable, NewCapacity, GetTypeHash(OldPair->Key), OldPair->Key.Get(), &NewSlot);
			check(ExistingValInNewTable.IsUninitialized()); // duplicate keys should be impossible since we're building from an existing set of data
			while (!NewPairTable[NewSlot].Key.Get().IsUninitialized())
			{
				NewSlot = (NewSlot + 1) & (NewCapacity - 1); // dumb linear probe, @TODO: something better
			}
			NewPairTable[NewSlot] = {OldPair->Key, OldPair->Value};
			NewSequenceTable[NumElementsInsertedIntoNewData++] = NewSlot;
		}
	}

	Data.Set(Context, NewData);
	SequenceData.Set(Context, NewSequenceData);
	Capacity = NewCapacity;
}

TPair<uint32, bool> VMapBase::AddWithoutLocking(FAllocationContext Context, uint32 KeyHash, VValue Key, VValue Value, bool bTransactional)
{
	checkSlow(!Key.IsUninitialized());
	checkSlow(!Value.IsUninitialized());

	bool bGrewCapacity = false;
	uint32 OldCapacity;
	TAux<PairType> OldData;
	TAux<SequenceType> OldSequenceData;

	if (2 * NumElements >= Capacity) // NumElements >= Capacity/2
	{
		if (bTransactional)
		{
			bGrewCapacity = true;
			OldCapacity = Capacity;
			OldData = Data.Get();
			OldSequenceData = SequenceData.Get();
		}

		Reserve(Context, Capacity * 2);
	}

	bool bAddedNewEntry = false;

	uint32 Slot;
	VValue ExistingVal = FindByHashWithSlot(Context, KeyHash, Key, &Slot);

	if (ExistingVal.IsUninitialized())
	{
		GetSequenceTable()[NumElements++] = Slot;
		bAddedNewEntry = true;
	}

	if (ExistingVal != Value)
	{
		PairType* PairTable = GetPairTable();
		checkSlow(PairTable[Slot].Key.Get().IsUninitialized() || ECompares::Eq == VValue::Equal(Context, PairTable[Slot].Key.Get(), Key, [](VValue R, VValue L) {}));
		// See comment below. These can be reverted without locking because the
		// table is zero initialized. So if the GC races with the stores to revert
		// these values, it's guaranteed to see a valid VValue.
		if (bTransactional)
		{
			PairTable[Slot].Get<0>().SetTransactionally(Context, Key);
			PairTable[Slot].Get<1>().SetTransactionally(Context, Value);
		}
		else
		{
			PairTable[Slot].Get<0>().Set(Context, Key);
			PairTable[Slot].Get<1>().Set(Context, Value);
		}
	}

	if (bTransactional && (bGrewCapacity || bAddedNewEntry))
	{
		const AutoRTFM::EContextStatus Status = AutoRTFM::Close([this, bAddedNewEntry, bGrewCapacity, OldCapacity, OldData, OldSequenceData] {
			// TODO: Check that `this` always lives long enough!
			AutoRTFM::OnAbort([this, bAddedNewEntry, bGrewCapacity, OldCapacity, OldData, OldSequenceData] {
				// It's safe to do this in a different critical section to reverting the
				// stores to key/value because the pair table is zero initialized. The
				// GC is guaranteed to visit valid VValues even if we race with it. It
				// might see uninitialized, the new value, or the old value -- all which
				// are valid VValues.
				UE::FExternalMutex ExternalMutex(Mutex);
				UE::TUniqueLock Lock(ExternalMutex);

				if (bAddedNewEntry)
				{
					--NumElements;
				}

				if (bGrewCapacity)
				{
					FRunningContext CurrentContext = FRunningContext(FRunningContextPromise());
					Capacity = OldCapacity;
					Data.Set(CurrentContext, OldData);
					SequenceData.Set(CurrentContext, OldSequenceData);
				}
			});
		});

		check(AutoRTFM::EContextStatus::OnTrack == Status);
	}

	bool bReplacedExistingEntry = !bAddedNewEntry;
	return {Slot, bReplacedExistingEntry};
}

template <typename MapType, typename TranslationFunc>
VValue VMapBase::FreezeMeltImpl(FAllocationContext Context, TranslationFunc&& Func)
{
	VMapBase& MapCopy = VMapBase::New<MapType>(Context, Num());

	PairType* PairTable = GetPairTable();
	SequenceType* SequenceTable = GetSequenceTable();
	for (uint32 I = 0; I < NumElements; ++I)
	{
		PairType* Pair = PairTable + SequenceTable[I];
		VValue Key = Pair->Key.Get();
		VValue Val = Func(Context, Pair->Value.Get());
		if (Val.IsPlaceholder())
		{
			return Val;
		}
		MapCopy.AddWithoutLocking(Context, GetTypeHash(Key), Key, Val);
	}
	return MapCopy;
}

VValue VMapBase::MeltImpl(FAllocationContext Context)
{
	return FreezeMeltImpl<VMutableMap>(
		Context,
		[](FAllocationContext Context, VValue Value) {
			return VValue::Melt(Context, Value);
		});
}

FOpResult VMutableMap::FreezeImpl(FAllocationContext Context)
{
	VValue MapCopy = FreezeMeltImpl<VMap>(
		Context,
		[](FAllocationContext Context, VValue Value) {
			FOpResult Result = VValue::Freeze(Context, Value);
			V_DIE_UNLESS(Result.IsReturn()); // Map values should always be valid.
			return Result.Value;
		});
	V_RETURN(MapCopy);
}

DEFINE_DERIVED_VCPPCLASSINFO(VMap);
DEFINE_TRIVIAL_VISIT_REFERENCES(VMap);
TGlobalTrivialEmergentTypePtr<&VMap::StaticCppClassInfo> VMap::GlobalTrivialEmergentType;

DEFINE_DERIVED_VCPPCLASSINFO(VMutableMap);
DEFINE_TRIVIAL_VISIT_REFERENCES(VMutableMap);
TGlobalTrivialEmergentTypePtr<&VMutableMap::StaticCppClassInfo> VMutableMap::GlobalTrivialEmergentType;

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
