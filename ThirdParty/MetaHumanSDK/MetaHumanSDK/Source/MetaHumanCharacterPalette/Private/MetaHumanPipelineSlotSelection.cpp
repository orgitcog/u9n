// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPipelineSlotSelection.h"

bool operator<(const FMetaHumanPipelineSlotSelection& A, const FMetaHumanPipelineSlotSelection& B)
{
	const int32 ParentPathComparison = A.ParentItemPath.Compare(B.ParentItemPath);

	if (ParentPathComparison != 0)
	{
		return ParentPathComparison < 0;
	}

	if (A.SelectedItem < B.SelectedItem)
	{
		return true;
	}

	return A.SelectedItem == B.SelectedItem
		&& A.SlotName.CompareIndexes(B.SlotName) < 0;
}

FMetaHumanPaletteItemPath FMetaHumanPipelineSlotSelection::GetSelectedItemPath() const
{
	return FMetaHumanPaletteItemPath(ParentItemPath, SelectedItem);
}
