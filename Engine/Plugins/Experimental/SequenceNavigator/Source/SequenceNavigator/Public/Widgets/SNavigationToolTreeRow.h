// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"

namespace UE::SequenceNavigator
{

class FNavigationToolItem;
class FNavigationToolView;
class SNavigationToolTreeView;

class SNavigationToolTreeRow : public SMultiColumnTableRow<FNavigationToolItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolTreeRow) {}
		SLATE_ATTRIBUTE(FText, HighlightText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
		, const TSharedRef<FNavigationToolView>& InToolView
		, const TSharedPtr<SNavigationToolTreeView>& InTreeView
		, const FNavigationToolItemPtr& InItem);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;

	TAttribute<FText> GetHighlightText() const { return HighlightText; }

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	TSharedPtr<FNavigationToolView> GetToolView() const;

	const FTableRowStyle* GetStyle() const { return Style; }

	/** The default reply if a row did not handle AcceptDrop */
	FReply OnDefaultDrop(const FDragDropEvent& InDragDropEvent) const;

private:
	FNavigationToolItemPtr Item;
	
	TWeakPtr<SNavigationToolTreeView> WeakTreeView;
	
	TWeakPtr<FNavigationToolView> WeakToolView;
	
	TAttribute<FText> HighlightText;
};

} // namespace UE::SequenceNavigator
