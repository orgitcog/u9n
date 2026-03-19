// Copyright Epic Games, Inc. All Rights Reserved.

#include "ItemActions/NavigationToolAddItem.h"
#include "Items/INavigationToolItem.h"
#include "NavigationTool.h"
#include "NavigationToolScopedSelection.h"
#include "Widgets/Views/STableRow.h"

namespace UE::SequenceNavigator
{

FNavigationToolAddItem::FNavigationToolAddItem(const FNavigationToolAddItemParams& InAddItemParams)
	: AddParams(InAddItemParams)
{
}

bool FNavigationToolAddItem::ShouldTransact() const
{
	return EnumHasAnyFlags(AddParams.Flags, ENavigationToolAddItemFlags::Transact);
}

void FNavigationToolAddItem::Execute(FNavigationTool& InTool)
{
	if (!AddParams.Item.IsValid())
	{
		return;
	}

	// Try to Create Children on Find
	if (EnumHasAnyFlags(AddParams.Flags, ENavigationToolAddItemFlags::AddChildren))
	{
		constexpr bool bRecursiveFind = true;
		TArray<FNavigationToolItemPtr> Children;
		AddParams.Item->FindValidChildren(Children, bRecursiveFind);
	}

	const FNavigationToolItemPtr ParentItem = AddParams.Item->GetParent();

	// If this array has elements in it, then we need to stop a circular dependency from forming
	const TArray<FNavigationToolItemPtr> PathToRelativeItem = AddParams.Item->FindPath({ AddParams.RelativeItem });
	if (PathToRelativeItem.Num() > 0 && ParentItem.IsValid())
	{
		FNavigationToolAddItemParams CircularSolverParams;
		CircularSolverParams.Item             = PathToRelativeItem[0];
		CircularSolverParams.RelativeItem     = AddParams.Item;
		CircularSolverParams.RelativeDropZone = EItemDropZone::AboveItem;
		CircularSolverParams.Flags            = AddParams.Flags;

		ParentItem->AddChild(MoveTemp(CircularSolverParams));
	}

	if (AddParams.RelativeItem.IsValid())
	{
		const FNavigationToolItemPtr RelativeItemParent = AddParams.RelativeItem->GetParent();

		// If it's onto item, the Relative Item is going to be the Parent
		if (!AddParams.RelativeDropZone.IsSet() || AddParams.RelativeDropZone == EItemDropZone::OntoItem)
		{
			//If the Relative Item is Onto and it's the same as the Current Parent, shift Item up in the Hierarchy
			//(as long as the parent is a valid one)
			if (AddParams.RelativeItem == ParentItem && RelativeItemParent)
			{
				AddParams.RelativeDropZone = EItemDropZone::BelowItem;
				RelativeItemParent->AddChild(AddParams);
			}
			else
			{
				AddParams.RelativeItem->AddChild(AddParams);
			}
		}
		//else we place it as a Sibling to the Relative Item
		else if (RelativeItemParent)
		{
			RelativeItemParent->AddChild(AddParams);
		}
		//if no parent, then add it to the tree root
		else
		{
			InTool.GetTreeRoot()->AddChild(AddParams);
		}
	}
	else
	{
		//If no Relative Item, add to tree root
		InTool.GetTreeRoot()->AddChild(AddParams);
	}

	const FNavigationToolScopedSelection ScopedSelection(*InTool.GetSequencer(), ENavigationToolScopedSelectionPurpose::Read);

	// Automatically select item if it's selected
	if (AddParams.Item->IsSelected(ScopedSelection))
	{
		// Select in Navigation Tool but don't signal selection as we already have it selected in Mode Tools
		AddParams.Flags = ENavigationToolAddItemFlags::Select;
		AddParams.SelectionFlags &= ~ENavigationToolItemSelectionFlags::SignalSelectionChange;
	}
	// Signal selection change when we attempt to select this item in the Navigation Tool but it isn't selected in Sequencer
	else if (EnumHasAnyFlags(AddParams.Flags, ENavigationToolAddItemFlags::Select))
	{
		AddParams.SelectionFlags |= ENavigationToolItemSelectionFlags::SignalSelectionChange;
	}

	if (EnumHasAnyFlags(AddParams.Flags, ENavigationToolAddItemFlags::Select))
	{
		InTool.SelectItems({ AddParams.Item }, AddParams.SelectionFlags);
	}

	InTool.SetToolModified();
}

void FNavigationToolAddItem::OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap, bool bRecursive)
{
	if (AddParams.Item.IsValid())
	{
		AddParams.Item->OnObjectsReplaced(InReplacementMap, bRecursive);
	}
	if (AddParams.RelativeItem.IsValid())
	{
		AddParams.RelativeItem->OnObjectsReplaced(InReplacementMap, bRecursive);
	}
}

} // namespace UE::SequenceNavigator
