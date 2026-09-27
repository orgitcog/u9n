// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "NavigationToolDefines.h"
#include "Templates/SharedPointer.h"

class FDragDropEvent;
class FReply;
class ISequencer;
class SWidget;
enum class EItemDropZone;
struct FGeometry;
struct FPointerEvent;
enum class ENavigationToolItemViewMode : uint8;

namespace UE::SequenceNavigator
{

class INavigationTool;
class INavigationToolColumn;

class INavigationToolView : public TSharedFromThis<INavigationToolView>
{
public:
	virtual ~INavigationToolView() = default;

	virtual TSharedPtr<INavigationTool> GetOwnerTool() const  = 0;

	/** Returns the Navigation Tool Widget. Can be null widget */
	virtual TSharedPtr<SWidget> GetToolWidget() const = 0;

	virtual TSharedPtr<ISequencer> GetSequencer() const = 0;

	/** Marks the Navigation Tool View to be refreshed on Next Tick */
	virtual void RequestRefresh() = 0;

	DECLARE_MULTICAST_DELEGATE(FOnToolViewRefreshed);
	virtual FOnToolViewRefreshed& GetOnToolViewRefreshed() = 0;

	virtual void SetKeyboardFocus() = 0;

	virtual ENavigationToolItemViewMode GetItemDefaultViewMode() const = 0;

	virtual ENavigationToolItemViewMode GetItemProxyViewMode() const = 0;

	virtual bool IsColumnVisible(const TSharedPtr<INavigationToolColumn>& InColumn) const = 0;

	virtual TSharedRef<SWidget> GetColumnMenuContent(const FName InColumnId) = 0;

	virtual void GetChildrenOfItem(const FNavigationToolItemPtr InItem, TArray<FNavigationToolItemPtr>& OutChildren) const = 0;

	/**
	 * Gets the Children of a given Item. Can recurse if the immediate child is hidden (the children of these hidden items should still be given the opportunity to show up)
	 * @param InItem the Item to get the children of
	 * @param OutChildren the visible children in the give view mode.
	 * @param InViewMode the view mode(s) that the children should support to be added to OutChildren
	 * @param InRecursionDisallowedItems the items where recursion should not be performed
	 */
	virtual void GetChildrenOfItem(const FNavigationToolItemPtr& InItem
		, TArray<FNavigationToolItemPtr>& OutChildren
		, ENavigationToolItemViewMode InViewMode
		, const TSet<FNavigationToolItemPtr>& InRecursionDisallowedItems) const = 0;

	/** Whether the given Item is explicitly marked as Read-only in the Navigation Tool */
	virtual bool IsItemReadOnly(const FNavigationToolItemPtr& InItem) const = 0;

	/** Selection State */

	/** Whether the given Item can be selected in this Navigation Tool View / Widget */
	virtual bool CanSelectItem(const FNavigationToolItemPtr& InItem) const = 0;

	/** Selects the Item in this Navigation Tool View */
	virtual void SelectItems(TArray<FNavigationToolItemPtr> InItems, const ENavigationToolItemSelectionFlags InFlags = ENavigationToolItemSelectionFlags::SignalSelectionChange) = 0;

	/** Clears the currently Selected Items in the Navigation Tool View */
	virtual void ClearItemSelection(bool bSignalSelectionChange = true) = 0;

	/** Whether the current item is selected in this Navigation Tool View */
	virtual bool IsItemSelected(const FNavigationToolItemPtr& InItem) const = 0;

	/** Gets the Currently Selected Items in the Tree View */
	virtual TArray<FNavigationToolItemPtr> GetSelectedItems() const = 0;

	/** Drag Drop */

	/** Called a Drag is attempted for the selected items in view */
	virtual FReply OnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FNavigationToolItemPtr InTargetItem) = 0;

	/** Processes the Drag and Drop for the given Item if valid, else it will process it for the Root Item */
	virtual FReply OnDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, const FNavigationToolItemPtr InTargetItem) = 0;

	/** Determines whether the Drag and Drop can be processed by the given item */
	virtual TOptional<EItemDropZone> OnCanDrop(const FDragDropEvent& InDragDropEvent
		, EItemDropZone InDropZone
		, const FNavigationToolItemPtr InTargetItem) const = 0;

	/** Expansion State */

	virtual bool IsItemExpanded(const FNavigationToolItemPtr& InItem, const bool bInUseFilter = true) const = 0;

	/** Expands / Collapses the given Item */
	virtual void SetItemExpansion(const FNavigationToolItemPtr& InItem, const bool bInExpand, const bool bInUseFilter = true) = 0;

	/** Expands / Collapses the given Item and its children recursively */
	virtual void SetItemExpansionRecursive(const FNavigationToolItemPtr InItem, const bool bInExpand) = 0;

	/** Change the expansion state of the parents (recursively) of the given item */
	virtual void SetParentItemExpansions(const FNavigationToolItemPtr& InItem, const bool bInExpand) = 0;

	/** @return True if all items in the tool tree view can be expanded */
	virtual bool CanExpandAll() const = 0;

	/** Expands all items in the tool tree view */
	virtual void ExpandAll() = 0;

	/** @return True if all items in the tool tree view can be collapsed */
	virtual bool CanCollapseAll() const = 0;

	/** Collapses all items in the tool tree view */
	virtual void CollapseAll() = 0;
};

} // namespace UE::SequenceNavigator
