// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMWeakCellMapInline.h"
#include "VerseVM/VVMNamedType.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMTupleType.h"
#include "VerseVM/VVMWeakCellMap.h"

namespace Verse
{

template <typename FunctorType> // FunctorType is (VTupleType*) -> void
void VPackage::ForEachUsedTupleType(FunctorType&& F)
{
	if (UsedTupleTypes)
	{
		UsedTupleTypes->ForEach([&](VCell* Key, VCell* Value) { F(&Key->StaticCast<VTupleType>()); });
	}
}

template <typename FunctorType> // FunctorType is (VNamedType*) -> void
void VPackage::ForEachUsedImport(FunctorType&& F)
{
	if (UsedImports)
	{
		for (uint32 Index = 0; Index < UsedImports->Num(); ++Index)
		{
			F(&UsedImports->GetValue(Index).StaticCast<VNamedType>());
		}
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
