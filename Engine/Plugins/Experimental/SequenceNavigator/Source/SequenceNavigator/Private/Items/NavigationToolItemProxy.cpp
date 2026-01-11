// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/NavigationToolItemProxy.h"
#include "Input/Reply.h"
#include "NavigationTool.h"
#include "NavigationToolView.h"

namespace UE::SequenceNavigator
{

FNavigationToolItemProxy::FNavigationToolItemProxy(INavigationTool& InTool, const FNavigationToolItemPtr& InParentItem)
	: FNavigationToolItem(InTool, InParentItem)
{
	WeakParent = InParentItem;
}

bool FNavigationToolItemProxy::IsItemValid() const
{
	return FNavigationToolItem::IsItemValid()
		&& WeakParent.IsValid()
		&& Tool.FindItem(WeakParent.Pin()->GetItemId()).IsValid();
}

void FNavigationToolItemProxy::FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, bool bRecursive)
{
	const FNavigationToolItemPtr Parent = GetParent();
	if (!Parent.IsValid() || !Parent->IsAllowedInTool())
	{
		return;
	}
	
	Super::FindChildren(OutChildren, bRecursive);
	
	GetProxiedItems(Parent.ToSharedRef(), OutChildren, bRecursive);
}

void FNavigationToolItemProxy::SetParent(FNavigationToolItemPtr InParent)
{
	Super::SetParent(InParent);

	// Recalculate our item Id because we rely on what our parent is for our Id
	RecalculateItemId();
}

ENavigationToolItemViewMode FNavigationToolItemProxy::GetSupportedViewModes(const INavigationToolView& InToolView) const
{
	// Hide proxies if it has no children
	if (Children.IsEmpty())
	{
		return ENavigationToolItemViewMode::None;
	}
	return InToolView.GetItemProxyViewMode();
}

bool FNavigationToolItemProxy::CanAutoExpand() const
{
	return false;
}

FNavigationToolItemId FNavigationToolItemProxy::CalculateItemId() const
{
	if (WeakParent.IsValid())
	{
		return FNavigationToolItemId(WeakParent.Pin(), *this);	
	}
	return FNavigationToolItemId();
}

} // namespace UE::SequenceNavigator
