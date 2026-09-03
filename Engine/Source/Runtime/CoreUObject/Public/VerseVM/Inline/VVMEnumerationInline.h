// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMEnumeration.h"

namespace Verse
{

inline VEnumerator& VEnumeration::GetEnumeratorChecked(int32 IntValue) const
{
	V_DIE_UNLESS(IntValue >= 0 && IntValue < int32(NumEnumerators));
	return *Enumerators[IntValue].Get();
}

inline VEnumeration& VEnumeration::New(FAllocationContext Context, VPackage* Package, VArray* RelativePath, VArray* EnumName, VArray* AttributeIndices, VArray* Attributes, UEnum* ImportEnum, bool bNative, const TArray<VEnumerator*>& Enumerators)
{
	return *new (Context.AllocateFastCell(sizeof(VEnumeration) + Enumerators.Num() * sizeof(TWriteBarrier<VEnumerator>))) VEnumeration(Context, Package, RelativePath, EnumName, AttributeIndices, Attributes, ImportEnum, bNative, Enumerators);
}

} // namespace Verse
#endif // WITH_VERSE_VM
