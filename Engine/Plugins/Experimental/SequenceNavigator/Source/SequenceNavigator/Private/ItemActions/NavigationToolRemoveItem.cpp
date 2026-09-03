// Copyright Epic Games, Inc. All Rights Reserved.

#include "ItemActions/NavigationToolRemoveItem.h"
#include "ItemActions/NavigationToolAddItem.h"
#include "Items/INavigationToolItem.h"
#include "NavigationTool.h"
#include "Widgets/Views/STableRow.h"

namespace UE::SequenceNavigator
{

FNavigationToolRemoveItem::FNavigationToolRemoveItem(const FNavigationToolRemoveItemParams& InRemoveItemParams)
	: RemoveParams(InRemoveItemParams)
{
	if (RemoveParams.Item.IsValid())
	{
		RemoveParams.Item->AddFlags(ENavigationToolItemFlags::PendingRemoval);
	}
}

void FNavigationToolRemoveItem::Execute(FNavigationTool& InTool)
{
	if (RemoveParams.Item.IsValid())
	{
		FNavigationToolItemFlagGuard Guard(RemoveParams.Item, ENavigationToolItemFlags::IgnorePendingKill);

		//Copy the Array since we may modify it below on Add/Remove Child
		TArray<FNavigationToolItemPtr> Children = RemoveParams.Item->GetChildren();

		FNavigationToolItemPtr Parent       = RemoveParams.Item->GetParent();
		FNavigationToolItemPtr RelativeItem = RemoveParams.Item;

		//Search the lowest parent that is not pending removal
		while (Parent.IsValid() && Parent->HasAnyFlags(ENavigationToolItemFlags::PendingRemoval))
		{
			RelativeItem = Parent;
			Parent       = Parent->GetParent();
		}

		//Reparent the Item's Children to the Valid Parent found above
		if (Parent.IsValid())
		{
			FNavigationToolAddItemParams ReparentParams;
			ReparentParams.RelativeItem             = RelativeItem;
			ReparentParams.RelativeDropZone         = EItemDropZone::BelowItem;
			ReparentParams.Flags                    = ENavigationToolAddItemFlags::Select;

			TArray<FNavigationToolItemPtr> ItemsToReparent;

			ItemsToReparent.Append(Children);
			while (ItemsToReparent.Num() > 0)
			{
				ReparentParams.Item = ItemsToReparent.Pop();

				//Then Parent it to the Item's Parent.
				//If we couldn't add the Child, then it means either the Child Item itself is invalid
				//or the underlying info (e.g. object) is invalid (e.g. actor pending kill)
				if (Parent->AddChild(ReparentParams) == false)
				{
					//so try reparenting the children of this invalid child (since this invalid child will be removed)
					if (ReparentParams.Item.IsValid())
					{
						ItemsToReparent.Append(ReparentParams.Item->GetChildren());
					}
				}
			}

			//In case the Parent is still the same, Remove Child Item, else the Parent is going to be removed anyways
			if (Parent == RemoveParams.Item->GetParent())
			{
				Parent->RemoveChild(RemoveParams.Item);
			}
		}
		else
		{
			for (const FNavigationToolItemPtr& Child : Children)
			{
				RemoveParams.Item->RemoveChild(Child);
			}
		}

		InTool.UnregisterItem(RemoveParams.Item->GetItemId());
		InTool.SetToolModified();
	}
}

void FNavigationToolRemoveItem::OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap, bool bRecursive)
{
	if (RemoveParams.Item.IsValid())
	{
		RemoveParams.Item->OnObjectsReplaced(InReplacementMap, bRecursive);
	}
}

} // namespace UE::SequenceNavigator
