// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Async/ExternalMutex.h"
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMType.h"
#include "VerseVM/VVMUniqueString.h"
#include "VerseVM/VVMVerseStruct.h"

class UVerseStruct;

namespace Verse
{

struct VPackage;
struct VPropertyType;

struct VTupleType : VType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	VUniqueString& GetUEMangledName() const { return *UEMangledName.Get(); }
	UVerseStruct* GetOrCreateUStruct(FAllocationContext Context, VPackage* Scope, bool bIsInstanced);

	static VTupleType& New(FAllocationContext Context, VUniqueString& UEMangledName, TArrayView<VValue> ElementTypes)
	{
		return *new (Context.Allocate(FHeap::DestructorSpace, sizeof(VTupleType) + ElementTypes.Num() * sizeof(TWriteBarrier<VValue>))) VTupleType(Context, UEMangledName, ElementTypes);
	}

	uint32 NumElements;
	TWriteBarrier<VValue>* GetElementTypes() { return (TWriteBarrier<VValue>*)((char*)this + sizeof(*this)); }

private:
	VTupleType(FAllocationContext Context, VUniqueString& InUEMangledName, TArrayView<VValue> ElementTypes)
		: VType(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumElements(ElementTypes.Num())
		, UEMangledName(Context, InUEMangledName)
	{
		TWriteBarrier<VValue>* ElementStorage = GetElementTypes();
		for (uint32 Index = 0; Index < NumElements; ++Index)
		{
			new (&ElementStorage[Index]) TWriteBarrier<VValue>(Context, ElementTypes[Index]);
		}
	}

	COREUOBJECT_API UVerseStruct* CreateUStruct(FAllocationContext Context, VPackage* Scope, bool bIsInstanced);

	TWriteBarrier<VUniqueString> UEMangledName;

	struct FUStructMapKeyFuncs : TDefaultMapKeyFuncs<TWriteBarrier<VPackage>, TWriteBarrier<VValue>, /*bInAllowDuplicateKeys*/ false>
	{
		static bool Matches(KeyInitType A, KeyInitType B) { return A == B; }
		static bool Matches(KeyInitType A, VPackage* B) { return A.Get() == B; }
		static uint32 GetKeyHash(KeyInitType Key) { return ::PointerHash(Key.Get()); }
		static uint32 GetKeyHash(VPackage* Key) { return ::PointerHash(Key); }
	};
	TMap<TWriteBarrier<VPackage>, TWriteBarrier<VValue>, FDefaultSetAllocator, FUStructMapKeyFuncs> AssociatedUStructs;
};

inline UVerseStruct* VTupleType::GetOrCreateUStruct(FAllocationContext Context, VPackage* Scope, bool bIsInstanced)
{
	if (TWriteBarrier<VValue>* Entry = AssociatedUStructs.Find({Context, Scope}))
	{
		return CastChecked<UVerseStruct>(Entry->Get().AsUObject());
	}

	return CreateUStruct(Context, Scope, bIsInstanced);
}

} // namespace Verse
#endif // WITH_VERSE_VM
