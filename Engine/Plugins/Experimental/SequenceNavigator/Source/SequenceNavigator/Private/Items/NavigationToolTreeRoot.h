// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Items/NavigationToolItem.h"

namespace UE::SequenceNavigator
{

class INavigationTool;

/**
 * The Item that ensures that every item (except self) has a parent in the hierarchy to make it easier
 * to handle. This is not really a visual item, so it can't appear in the Navigation Tool view.
 */
class FNavigationToolTreeRoot final : public FNavigationToolItem
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolTreeRoot, FNavigationToolItem)

	FNavigationToolTreeRoot(INavigationTool& InTool);
	virtual ~FNavigationToolTreeRoot() override = default;

	//~ Begin INavigationToolItem
	virtual void FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive) override;
	virtual bool CanAddChild(const FNavigationToolItemPtr& InChild) const override;
	virtual bool IsAllowedInTool() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetClassName() const override;
	virtual FText GetIconTooltipText() const override;
	virtual FSlateIcon GetIcon() const override;
	virtual TSharedRef<SWidget> GenerateLabelWidget(const TSharedRef<SNavigationToolTreeRow>& InRow) override;
	virtual TOptional<EItemDropZone> CanAcceptDrop(const FDragDropEvent& InDragDropEvent, const EItemDropZone InDropZone) override;	
	virtual FReply AcceptDrop(const FDragDropEvent& InDragDropEvent, const EItemDropZone InDropZone) override;
	//~ End INavigationToolItem

protected:
	//~ Begin FNavigationToolItem
	virtual FNavigationToolItemId CalculateItemId() const override;
	//~ End FNavigationToolItem
};

} // namespace UE::SequenceNavigator
