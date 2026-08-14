// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Items/NavigationToolItemProxy.h"

namespace UE::SequenceNavigator
{

class SEQUENCENAVIGATOR_API FNavigationToolComponentProxy : public FNavigationToolItemProxy
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolComponentProxy, FNavigationToolItemProxy)

	FNavigationToolComponentProxy(INavigationTool& InTool, const FNavigationToolItemPtr& InParentItem);
	
	//~ Begin INavigationToolItem
	virtual FText GetDisplayName() const override;
	virtual FSlateIcon GetIcon() const override;
	virtual FText GetIconTooltipText() const override;
	virtual ENavigationToolItemViewMode GetSupportedViewModes(const INavigationToolView& InToolView) const override;
	//~ End INavigationToolItem
	
	//~ Begin FNavigationToolItemProxy
	virtual void GetProxiedItems(const TSharedRef<INavigationToolItem>& InParent, TArray<FNavigationToolItemPtr>& OutChildren, bool bInRecursive) override;
	//~ End FNavigationToolItemProxy
};

} // namespace UE::SequenceNavigator
