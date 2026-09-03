// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterPipeline.h"

#include "MetaHumanPinnedSlotSelection.h"

void UMetaHumanCharacterPipeline::SetInstanceParameters(const FInstancedStruct& ParameterContext, const FInstancedPropertyBag& Parameters) const
{
	// Default implementation so that subclasses that don't have instance parameters don't have to implement this
	//
	// Implemented here so that we can change it in a hotfix if necessary.
}

TArrayView<const FMetaHumanPinnedSlotSelection> UMetaHumanCharacterPipeline::FilterPinnedSlotSelectionsToItem(
	TArrayView<const FMetaHumanPinnedSlotSelection> SlotSelections, 
	const FMetaHumanPaletteItemPath& FilteredItem)
{
	int32 StartIndex = SlotSelections.IndexOfByPredicate(
		[&FilteredItem](const FMetaHumanPinnedSlotSelection& Selection)
		{
			return Selection.Selection.GetSelectedItemPath().IsEqualOrChildPathOf(FilteredItem);
		});

	if (StartIndex == INDEX_NONE)
	{
		return TArrayView<const FMetaHumanPinnedSlotSelection>();
	}

	int32 EndIndex = SlotSelections.Num();
	for (int32 Index = StartIndex + 1; Index < SlotSelections.Num(); Index++)
	{
		if (!SlotSelections[Index].Selection.GetSelectedItemPath().IsEqualOrChildPathOf(FilteredItem))
		{
			EndIndex = Index;
			break;
		}
	}

	return MakeArrayView(&SlotSelections[StartIndex], EndIndex - StartIndex);
}

TArrayView<const FMetaHumanPaletteItemPath> UMetaHumanCharacterPipeline::FilterItemPaths(
	TArrayView<const FMetaHumanPaletteItemPath> ItemPaths, 
	const FMetaHumanPaletteItemPath& FilteredItem)
{
	int32 StartIndex = ItemPaths.IndexOfByPredicate(
		[&FilteredItem](const FMetaHumanPaletteItemPath& ItemPath)
		{
			return ItemPath.IsEqualOrChildPathOf(FilteredItem);
		});

	if (StartIndex == INDEX_NONE)
	{
		return TArrayView<const FMetaHumanPaletteItemPath>();
	}

	int32 EndIndex = ItemPaths.Num();
	for (int32 Index = StartIndex + 1; Index < ItemPaths.Num(); Index++)
	{
		if (!ItemPaths[Index].IsEqualOrChildPathOf(FilteredItem))
		{
			EndIndex = Index;
			break;
		}
	}

	return MakeArrayView(&ItemPaths[StartIndex], EndIndex - StartIndex);
}
