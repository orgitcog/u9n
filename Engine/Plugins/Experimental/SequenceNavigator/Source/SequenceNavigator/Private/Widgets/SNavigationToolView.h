// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/SBasicFilterBar.h"
#include "Items/INavigationToolItem.h"
#include "NavigationToolDefines.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class SBorder;
class SHeaderRow;
class STableViewBase;

namespace UE::SequenceNavigator
{

class FNavigationToolFilter;
class FNavigationToolFilterBar;
class FNavigationToolItemDragDropOp;
class FNavigationToolToolbarMenu;
class FNavigationToolView;
class SNavigationToolFilterBar;
class SNavigationToolTreeView;
enum class ENavigationToolFilterChange : uint8;

class SNavigationToolView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FNavigationToolView>& InToolView);

	virtual ~SNavigationToolView() override;

	TSharedPtr<SNavigationToolTreeView> GetTreeView() const { return TreeView; }

	/** Rebuilds the entire widget. Called when the filter bar layout is changed. */
	void RebuildWidget();

	void ReconstructColumns();

	bool IsColumnVisible(const FName InColumnId) const;
	void ShowHideColumn(const FName InColumnId, const bool bInVisible);

	void SetColumnWidth(const FName InColumnId, const float InWidth);

	void RequestTreeRefresh();

	void SetItemSelection(const TArray<FNavigationToolItemPtr>& InItems, bool bSignalSelectionChange);

	void OnItemSelectionChanged(const FNavigationToolItemPtr InItem, ESelectInfo::Type InSelectionType);

	void ScrollItemIntoView(const FNavigationToolItemPtr& InItem) const;

	bool IsItemExpanded(const FNavigationToolItemPtr& InItem) const;
	void SetItemExpansion(const FNavigationToolItemPtr& Item, const bool bInExpand) const;

	void UpdateItemExpansions(const FNavigationToolItemPtr& Item) const;

	TSharedRef<ITableRow> OnItemGenerateRow(const FNavigationToolItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);

	void SetKeyboardFocus() const;

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;

	void SetTreeBorderVisibility(bool bVisible);

	void GenerateColumnState(const FName InColumnId, FNavigationToolViewColumnSaveState& OutColumnState);
	void GenerateColumnStateMap(TMap<FName, FNavigationToolViewColumnSaveState>& OutStateMap);

	EFilterBarLayout GetFilterBarLayout() const;
	void SetFilterBarLayout(const EFilterBarLayout InLayout);

private:
	TSharedRef<SWidget> ConstructSplitterContent();
	TSharedRef<SWidget> ConstructMainContent();

	void RebuildSearchAndFilterRow();

	void OnFilterBarStateChanged(const bool InIsVisible, const EFilterBarLayout InNewLayout);
	void OnFiltersChanged(const ENavigationToolFilterChange InChangeType, const TSharedRef<FNavigationToolFilter>& InFilter);

	TSharedPtr<FNavigationToolFilterBar> GetFilterBar() const;

	TWeakPtr<FNavigationToolView> WeakToolView;

	TSharedPtr<FNavigationToolToolbarMenu> ToolbarMenu;

	TSharedPtr<SVerticalBox> SearchAndFilterRow;
	TSharedPtr<SHeaderRow> HeaderRowWidget;
	TSharedPtr<SBorder> TreeBorder;
	TSharedPtr<SNavigationToolTreeView> TreeView;
	TSharedPtr<SNavigationToolFilterBar> FilterBarWidget;

	/** If true the SuggestionList shouldn't appear since we selected what we wanted */
	bool bIsEnterLastKeyPressed = false;

	bool bSelectingItems = false;

	TSet<TWeakPtr<FNavigationToolItemDragDropOp>> ItemDragDropOps;
};

} // namespace UE::SequenceNavigator
