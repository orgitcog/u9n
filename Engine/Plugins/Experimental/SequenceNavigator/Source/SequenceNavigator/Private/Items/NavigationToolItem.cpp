// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/NavigationToolItem.h"
#include "DragDropOps/NavigationToolItemDragDropOp.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "ItemActions/NavigationToolAddItem.h"
#include "ItemActions/NavigationToolRemoveItem.h"
#include "Items/NavigationToolItemUtils.h"
#include "Misc/Optional.h"
#include "NavigationTool.h"
#include "NavigationToolView.h"
#include "Providers/NavigationToolProvider.h"
#include "Styling/StyleColors.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Columns/SNavigationToolLabelItem.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "NavigationToolItem"

namespace UE::SequenceNavigator
{

FNavigationToolItem::FNavigationToolItem(INavigationTool& InTool, const FNavigationToolItemPtr& InParentItem)
	: Tool(InTool)
	, WeakParent(InParentItem)
{
}

INavigationTool& FNavigationToolItem::GetOwnerTool() const
{
	return Tool;
}

TSharedPtr<FNavigationToolProvider> FNavigationToolItem::GetProvider() const
{
	return WeakProvider.Pin();
}

bool FNavigationToolItem::IsItemValid() const
{
	return true;
}

void FNavigationToolItem::RefreshChildren()
{
	TArray<FNavigationToolItemPtr> FoundChildren;
	FindValidChildren(FoundChildren, /*bRecursiveFind=*/false);

	TArray<FNavigationToolItemPtr> Sortable;
	TArray<FNavigationToolItemPtr> Unsortable;
	SplitSortableAndUnsortableItems(FoundChildren, Sortable, Unsortable);

	// Start with all Sortable/Unsortable Items, and remove every item seen by iterating Children
	TSet<FNavigationToolItemPtr> NewSortableChildren(Sortable);
	TSet<FNavigationToolItemPtr> NewUnsortableChildren(Unsortable);
	
	// Remove items from "Children" that were not present in the Sortable Found Children (we'll add non-sortable later)
	// Result is have Children only contain Items that existed previously
	for (TArray<FNavigationToolItemPtr>::TIterator ItemIter(Children); ItemIter; ++ItemIter)
	{
		const FNavigationToolItemPtr Item(*ItemIter);

		if (!Item.IsValid() || NewUnsortableChildren.Contains(Item))
		{
			ItemIter.RemoveCurrent();
		}
		else if (!NewSortableChildren.Contains(Item) || !Item->IsItemValid())
		{
			Item->SetParent(nullptr);
			ItemIter.RemoveCurrent();
		}

		NewSortableChildren.Remove(Item);
		NewUnsortableChildren.Remove(Item);
	}

	// Find Children for New Children in case these new children have grand children
	// Note: This does not affect any of the current containers. It's just called for discovery
	auto FindGrandChildren = [](const TSet<FNavigationToolItemPtr>& InChildren)
		{
			for (const FNavigationToolItemPtr& Child : InChildren)
			{
				TArray<FNavigationToolItemPtr> GrandChildren;
				Child->FindValidChildren(GrandChildren, /*bRecursiveFind=*/true);
			}
		};

	FindGrandChildren(NewUnsortableChildren);
	FindGrandChildren(NewSortableChildren);

	// After removing Children not present in Sortable
	// Children should either be equal in size with Sortable (which means no new sortable children were added)
	// or Sortable has more entries which means there are new items to add
	if (Sortable.Num() > Children.Num())
	{
		check(!NewSortableChildren.IsEmpty());
		HandleNewSortableChildren(NewSortableChildren.Array());
	}

	// Rearrange so that Children are arranged like so:
	// [Unsortable Children][Sortable Children]
	Unsortable.Append(MoveTemp(Children));
	Children = MoveTemp(Unsortable);

	// Update the Parents of every Child in the List
	const FNavigationToolItemPtr This = SharedThis(this);
	for (const FNavigationToolItemPtr& Child : Children)
	{
		Child->SetParent(This);
	}
}

void FNavigationToolItem::ResetChildren()
{
	for (const FNavigationToolItemPtr& Item : GetChildren())
	{
		if (Item.IsValid())
		{
			Item->SetParent(nullptr);
		}
	}
	GetChildrenMutable().Reset();	
};

void FNavigationToolItem::FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive)
{
	FNavigationTool& ToolPrivate = static_cast<FNavigationTool&>(Tool);

	TArray<TSharedPtr<FNavigationToolItemProxy>> ItemProxies;
	ToolPrivate.GetItemProxiesForItem(SharedThis(this), ItemProxies);
	OutChildren.Reserve(OutChildren.Num() + ItemProxies.Num());

	for (const TSharedPtr<FNavigationToolItemProxy>& ItemProxy : ItemProxies)
	{
		OutChildren.Add(ItemProxy);
		if (bInRecursive)
		{
			ItemProxy->FindChildren(OutChildren, bInRecursive);
		}
	}
}

void FNavigationToolItem::FindValidChildren(TArray<FNavigationToolItemPtr>& OutChildren, bool bInRecursive)
{
	FindChildren(OutChildren, bInRecursive);

	OutChildren.RemoveAll(
		[](const FNavigationToolItemPtr& InItem)
		{
			return !InItem.IsValid() || !InItem->IsAllowedInTool();
		});
}

TArray<FNavigationToolItemPtr> FNavigationToolItem::FindPath(const TArray<FNavigationToolItemPtr>& InItems) const
{
	const TSharedPtr<const INavigationToolItem> This = SharedThis(this);
	TArray<FNavigationToolItemPtr> Path;
	for (const FNavigationToolItemPtr& Item : InItems)
	{
		Path.Reset();
		FNavigationToolItemPtr CurrentItem = Item;
		while (CurrentItem.IsValid())
		{
			if (This == CurrentItem)
			{
				Algo::Reverse(Path);
				return Path;
			}
			Path.Add(CurrentItem);
			CurrentItem = CurrentItem->GetParent();
		}
	}
	return TArray<FNavigationToolItemPtr>();
}

INavigationToolItem::IndexType FNavigationToolItem::GetChildIndex(const FNavigationToolItemPtr& ChildItem) const
{
	return GetChildren().Find(ChildItem);
}

bool FNavigationToolItem::CanAddChild(const FNavigationToolItemPtr& InChild) const
{
	return InChild.IsValid();
}

bool FNavigationToolItem::AddChild(const FNavigationToolAddItemParams& InAddItemParams)
{
	if (CanAddChild(InAddItemParams.Item))
	{
		AddChildChecked(InAddItemParams);
		return true;
	}
	return false;
}

bool FNavigationToolItem::RemoveChild(const FNavigationToolRemoveItemParams& InRemoveItemParams)
{
	if (InRemoveItemParams.Item.IsValid())
	{
		return RemoveChildChecked(InRemoveItemParams);
	}
	return false;
}

void FNavigationToolItem::SetParent(FNavigationToolItemPtr InParent)
{
	//check that one of the parent's children is this
	WeakParent = InParent;
}

ENavigationToolItemViewMode FNavigationToolItem::GetSupportedViewModes(const INavigationToolView& InToolView) const
{
	return InToolView.GetItemDefaultViewMode();
}

FNavigationToolItemId FNavigationToolItem::GetItemId() const
{
	if (ItemId.IsValidId())
	{
		return ItemId;
	}
	const_cast<FNavigationToolItem*>(this)->RecalculateItemId();
	return ItemId;
}

FSlateColor FNavigationToolItem::GetItemLabelColor() const
{
	return FStyleColors::Foreground;
}

const FSlateBrush* FNavigationToolItem::GetIconBrush() const
{
	const FSlateIcon Icon = FNavigationToolExtender::FindOverrideIcon(SharedThis(this));
	if (Icon.IsSet())
	{
		return Icon.GetIcon();
	}

	if (const FSlateBrush* const DefaultIconBrush = GetDefaultIconBrush())
	{
		return DefaultIconBrush;
	}

	return GetIcon().GetIcon();
}

FSlateColor FNavigationToolItem::GetIconColor() const
{
	return FStyleColors::Foreground;
}

TSharedRef<SWidget> FNavigationToolItem::GenerateLabelWidget(const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	return SNew(SNavigationToolLabelItem, SharedThis(this), InRow);
}

bool FNavigationToolItem::Delete()
{
	GetOwnerTool().NotifyToolItemDeleted(SharedThis(this));
	return true;
}

void FNavigationToolItem::AddFlags(ENavigationToolItemFlags Flags)
{
	EnumAddFlags(ItemFlags, Flags);
}

void FNavigationToolItem::RemoveFlags(ENavigationToolItemFlags Flags)
{
	EnumRemoveFlags(ItemFlags, Flags);
}

bool FNavigationToolItem::HasAnyFlags(ENavigationToolItemFlags Flags) const
{
	return EnumHasAnyFlags(ItemFlags, Flags);
}

bool FNavigationToolItem::HasAllFlags(ENavigationToolItemFlags Flags) const
{
	return EnumHasAllFlags(ItemFlags, Flags);
}

TOptional<EItemDropZone> FNavigationToolItem::CanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone)
{
	if (const TSharedPtr<FNavigationToolItemDragDropOp> ItemDragDropOp = InDragDropEvent.GetOperationAs<FNavigationToolItemDragDropOp>())
	{
		const TOptional<EItemDropZone> DropZone = ItemDragDropOp->CanDrop(InDropZone, SharedThis(this));
		if (DropZone.IsSet())
		{
			if (GetItemId() != FNavigationToolItemId::RootId)
			{
				ItemDragDropOp->CurrentIconBrush = GetIconBrush();
			}
			return *DropZone;
		}

		ItemDragDropOp->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
	}

	const TSharedRef<FNavigationToolItem> SharedThisRef = SharedThis(this);

	TOptional<EItemDropZone> OutDropZone;

	GetOwnerTool().ForEachProvider([this, &InDragDropEvent, InDropZone, &SharedThisRef, &OutDropZone]
		(const TSharedRef<FNavigationToolProvider>& InProvider)
		{
			OutDropZone = InProvider->OnToolItemCanAcceptDrop(InDragDropEvent, InDropZone, SharedThisRef);
			if (OutDropZone.IsSet())
			{
				return false;
			}
			return true;
		});

	return OutDropZone;
}

FReply FNavigationToolItem::AcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone)
{
	if (const TSharedPtr<FNavigationToolItemDragDropOp> ItemDragDropOp = InDragDropEvent.GetOperationAs<FNavigationToolItemDragDropOp>())
	{
		const FReply Reply = ItemDragDropOp->Drop(InDropZone, SharedThis(this));
		if (Reply.IsEventHandled())
		{
			return Reply;
		}
	}

	const TSharedRef<FNavigationToolItem> SharedThisRef = SharedThis(this);

	FReply OutReply = FReply::Unhandled();

	GetOwnerTool().ForEachProvider([this, &InDragDropEvent, InDropZone, &SharedThisRef, &OutReply]
		(const TSharedRef<FNavigationToolProvider>& InProvider)
		{
			OutReply = InProvider->OnToolItemAcceptDrop(InDragDropEvent, InDropZone, SharedThisRef);
			if (OutReply.IsEventHandled())
			{
				return false;
			}
			return true;
		});

	return OutReply;
}

FLinearColor FNavigationToolItem::GetItemTintColor() const
{
	return FStyleColors::White.GetSpecifiedColor();
}

bool FNavigationToolItem::IsExpanded() const
{
	if (const TSharedPtr<INavigationToolView> ToolView = Tool.GetMostRecentToolView())
	{
		return ToolView->IsItemExpanded(SharedThis(const_cast<FNavigationToolItem*>(this)));
	}
	return false;
}

void FNavigationToolItem::SetExpansion(const bool bInExpand)
{
	if (const TSharedPtr<INavigationToolView> ToolView = Tool.GetMostRecentToolView())
	{
		return ToolView->SetItemExpansion(SharedThis(this), bInExpand);
	}
}

TOptional<FColor> FNavigationToolItem::GetColor() const
{
	return Tool.FindItemColor(SharedThis(const_cast<FNavigationToolItem*>(this)));
}

void FNavigationToolItem::SetColor(const TOptional<FColor>& InColor)
{
	Tool.SetItemColor(SharedThis(this), InColor.Get(FColor()));
}

void FNavigationToolItem::SetProvider(const TWeakPtr<FNavigationToolProvider>& InWeakProvider)
{
	WeakProvider = InWeakProvider;
}

void FNavigationToolItem::RecalculateItemId()
{
	const FNavigationToolItemId OldItemId = ItemId;
	ItemId = CalculateItemId();

	FNavigationTool& ToolPrivate = static_cast<FNavigationTool&>(Tool);
	ToolPrivate.NotifyItemIdChanged(OldItemId, SharedThis(this));
}

void FNavigationToolItem::AddChildChecked(const FNavigationToolAddItemParams& InAddItemParams)
{
	if (const FNavigationToolItemPtr OldParent = InAddItemParams.Item->GetParent())
	{
		// if we are adding the child and the old parent is this,
		// then it means we're just rearranging, only remove from array
		if (OldParent.Get() == this)
		{
			Children.Remove(InAddItemParams.Item);
		}
		else
		{
			FNavigationToolRemoveItemParams RemoveParams(InAddItemParams.Item);
			OldParent->RemoveChild(RemoveParams);
		}
	}

	if (InAddItemParams.RelativeItem.IsValid()
		&& InAddItemParams.RelativeDropZone.IsSet()
		&& InAddItemParams.RelativeDropZone != EItemDropZone::OntoItem)
	{
		const int32 RelativeItemIndex = Children.Find(InAddItemParams.RelativeItem);
		if (RelativeItemIndex != INDEX_NONE)
		{
			const int32 TargetIndex = InAddItemParams.RelativeDropZone == EItemDropZone::BelowItem
				? RelativeItemIndex + 1
				: RelativeItemIndex;

			Children.EmplaceAt(TargetIndex, InAddItemParams.Item);
		}
		else
		{
			Children.EmplaceAt(0, InAddItemParams.Item);
		}
	}
	else
	{
		Children.EmplaceAt(0, InAddItemParams.Item);
	}

	InAddItemParams.Item->SetParent(SharedThis(this));
}

bool FNavigationToolItem::RemoveChildChecked(const FNavigationToolRemoveItemParams& InRemoveItemParams)
{
	InRemoveItemParams.Item->SetParent(nullptr);
	return Children.Remove(InRemoveItemParams.Item) > 0;
}

void FNavigationToolItem::HandleNewSortableChildren(TArray<FNavigationToolItemPtr> InSortableChildren)
{
	InSortableChildren.Sort([this](const FNavigationToolItemPtr& InItemA, const FNavigationToolItemPtr& InItemB)
		{
			const FNavigationToolSaveState* const SaveStateA = InItemA->GetProviderSaveState();
			if (!SaveStateA)
			{
				return false;
			}

			const FNavigationToolSaveState* const SaveStateB = InItemB->GetProviderSaveState();
			if (!SaveStateB)
			{
				return false;
			}

			const FNavigationToolSerializedTreeNode* const NodeA = SaveStateA->SerializedTree.FindTreeNode(InItemA->MakeSerializedItem());
			const FNavigationToolSerializedTreeNode* const NodeB = SaveStateB->SerializedTree.FindTreeNode(InItemB->MakeSerializedItem());

			return FNavigationToolSerializedTree::CompareTreeItemOrder(NodeA, NodeB);
		});

	FNavigationToolAddItemParams AddItemParams;
	for (const FNavigationToolItemPtr& NewChild : InSortableChildren)
	{
		const FNavigationToolSaveState* const SaveState = NewChild->GetProviderSaveState();
		if (!SaveState)
		{
			continue;
		}

		AddItemParams.Item = NewChild;

		const FNavigationToolSerializedTreeNode* const TreeNode = SaveState->SerializedTree.FindTreeNode(NewChild->MakeSerializedItem());
		if (TreeNode && Children.IsValidIndex(TreeNode->GetLocalIndex()))
		{
			// Add Before the Child at Index, so this Item is at the specific Index
			AddItemParams.RelativeItem = Children[TreeNode->GetLocalIndex()];
			AddItemParams.RelativeDropZone = EItemDropZone::AboveItem;
		}
		else
		{
			// Add After Last, so this Item is the last item in the List
			AddItemParams.RelativeItem = Children.IsEmpty() ? nullptr : Children.Last();
			AddItemParams.RelativeDropZone = EItemDropZone::BelowItem;
		}

		AddChild(AddItemParams);
	}
}

FNavigationToolSaveState* FNavigationToolItem::GetProviderSaveState() const
{
	if (const TSharedPtr<FNavigationToolProvider> Provider = GetProvider())
	{
		return Provider->GetSaveState(Tool);
	}
	return nullptr;
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
