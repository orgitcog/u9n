// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPinnedSlotSelection.h"

bool FMetaHumanPinnedSlotSelection::IsItemPinned(TConstArrayView<FMetaHumanPinnedSlotSelection> SortedSelections, const FMetaHumanPaletteItemPath& ItemPath)
{
	return SortedSelections.ContainsByPredicate([&ItemPath](const FMetaHumanPinnedSlotSelection& SortedSelection)
		{
			return SortedSelection.Selection.GetSelectedItemPath() == ItemPath;
		});
}

bool FMetaHumanPinnedSlotSelection::TryGetPinnedItem(
	TConstArrayView<FMetaHumanPinnedSlotSelection> SortedSelections, 
	const FMetaHumanPaletteItemPath& ItemPath, 
	const FMetaHumanPinnedSlotSelection*& OutPinnedItem)
{
	OutPinnedItem = SortedSelections.FindByPredicate([&ItemPath](const FMetaHumanPinnedSlotSelection& SortedSelection)
		{
			return SortedSelection.Selection.GetSelectedItemPath() == ItemPath;
		});

	return OutPinnedItem != nullptr;
}
