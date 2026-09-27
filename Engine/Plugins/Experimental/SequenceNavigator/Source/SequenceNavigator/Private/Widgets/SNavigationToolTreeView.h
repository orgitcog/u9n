// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "Widgets/Views/STreeView.h"

namespace UE::SequenceNavigator
{

class FNavigationToolView;

class SNavigationToolTreeView : public STreeView<FNavigationToolItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolTreeView) {}
		SLATE_ARGUMENT(STreeView<FNavigationToolItemPtr>::FArguments, TreeViewArgs)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<FNavigationToolView>& InToolView);

	int32 GetItemIndex(const FNavigationToolItemPtr& Item) const;

	void FocusOnItem(const FNavigationToolItemPtr& InItem);

	void ScrollItemIntoView(const FNavigationToolItemPtr& InItem);

	void UpdateItemExpansions(const FNavigationToolItemPtr& InItem);

	//~ Begin STreeView
	virtual void Private_UpdateParentHighlights() override;
	//~ End STreeView

	//~ Begin ITypedTableView
	virtual void Private_SetItemSelection(const FNavigationToolItemPtr InItem, bool bShouldBeSelected, bool bWasUserDirected = false) override;
	virtual void Private_SignalSelectionChanged(ESelectInfo::Type SelectInfo) override;
	virtual void Private_ClearSelection() override;
	//~ End SListView

	//~ Begin SWidget
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget

private:
	TWeakPtr<FNavigationToolView> WeakToolView;
	
	SNavigationToolTreeView::TItemSet PreviousSelectedItems;
};

} // namespace UE::SequenceNavigator
