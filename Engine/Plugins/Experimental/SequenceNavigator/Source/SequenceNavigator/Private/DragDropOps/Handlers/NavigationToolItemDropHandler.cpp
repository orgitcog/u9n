// Copyright Epic Games, Inc. All Rights Reserved.

#include "DragDropOps/Handlers/NavigationToolItemDropHandler.h"
#include "DragDropOps/NavigationToolItemDragDropOp.h"

namespace UE::SequenceNavigator
{

void FNavigationToolItemDropHandler::Initialize(const FNavigationToolItemDragDropOp& InDragDropOp)
{
	Items = InDragDropOp.GetItems();
	ActionType = InDragDropOp.GetActionType();

	// Remove all Items that are Invalid or out of the Scope of this Handler
	Items.RemoveAll([this](const FNavigationToolItemPtr& InItem)
		{
			return !InItem.IsValid() || !IsDraggedItemSupported(InItem);
		});
}

} // namespace UE::SequenceNavigator
