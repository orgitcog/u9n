// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolItem.h"

namespace UE::SequenceNavigator
{

/**
 * Item Proxies are Navigation Tool Items that with the sole purpose to group and hold common items together.
 * The description or name of such commonality between these Items should be the name of the Proxy that holds them.
 * 
 * NOTE: Although Item Proxies by default require a parent to be visible in Navigation Tool,
 * they can be created without a parent as a means to override behavior (e.g. DisplayName, Icon, etc)
 */
class SEQUENCENAVIGATOR_API FNavigationToolItemProxy : public FNavigationToolItem
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolItemProxy, FNavigationToolItem);

	FNavigationToolItemProxy(INavigationTool& InTool, const FNavigationToolItemPtr& InParentItem);

	//~ Begin INavigationToolItem
	virtual bool IsItemValid() const override;
	virtual void FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, bool bRecursive) override final;
	virtual void SetParent(FNavigationToolItemPtr InParent) override;
	virtual ENavigationToolItemViewMode GetSupportedViewModes(const INavigationToolView& InToolView) const override;
	virtual bool CanAutoExpand() const override;
	virtual FText GetClassName() const override { return FText::GetEmpty(); }
	//~ End INavigationToolItem

	uint32 GetPriority() const { return Priority; }
	void SetPriority(uint32 InPriority) { Priority = InPriority; }

	/** Gets the Items that this Item Proxy is representing / holding (i.e. children) */
	virtual void GetProxiedItems(const TSharedRef<INavigationToolItem>& InParent, TArray<FNavigationToolItemPtr>& OutChildren, bool bInRecursive) = 0;

protected:
	//~ Begin FNavigationToolItem
	virtual FNavigationToolItemId CalculateItemId() const override;
	//~ End FNavigationToolItem

private:
	/** This Item Proxy's Order Priority (i.e. Highest priority is placed topmost or leftmost (depending on Orientation). Priority 0 is lowest priority */
	uint32 Priority = 0;
};

} // namespace UE::SequenceNavigator
