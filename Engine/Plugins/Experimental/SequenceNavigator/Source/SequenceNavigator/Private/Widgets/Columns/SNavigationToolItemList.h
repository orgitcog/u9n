// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;

namespace UE::SequenceNavigator
{

class INavigationToolView;
class SNavigationToolTreeRow;

/** Widget that visualizes the list of children an Item has when collapsed */
class SNavigationToolItemList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolItemList) {}
	SLATE_END_ARGS()

	virtual ~SNavigationToolItemList() override;

	void Construct(const FArguments& InArgs
		, const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

	void OnItemExpansionChanged(const TSharedPtr<INavigationToolView>& InToolView, bool bInIsExpanded);

	void Refresh();

	FReply OnItemChipSelected(const FNavigationToolItemPtr& InItem, const FPointerEvent& InMouseEvent);

	FReply OnItemChipValidDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent);

private:
	FNavigationToolItemWeakPtr WeakItem;

	TWeakPtr<INavigationToolView> WeakView;

	TWeakPtr<SNavigationToolTreeRow> WeakRowWidget;

	TArray<FNavigationToolItemWeakPtr> WeakChildItems;

	TSharedPtr<SScrollBox> ItemListBox;
};

} // namespace UE::SequenceNavigator
