// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "Extensions/IColorExtension.h"
#include "Items/INavigationToolItem.h"
#include "Items/NavigationToolItemId.h"
#include "NavigationToolDefines.h"
#include "NavigationToolItemType.h"
#include "NavigationToolSettings.h"

namespace UE::SequenceNavigator
{

class SNavigationToolTreeRow;

/** Base Implementation of INavigationToolItem */
class SEQUENCENAVIGATOR_API FNavigationToolItem
	: public INavigationToolItem
	, public IColorExtension
{
	friend class FNavigationTool;

public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolItem
		, INavigationToolItem
		, IColorExtension);

	DECLARE_DELEGATE_RetVal_OneParam(FNavigationToolSaveState*, FNavigationToolGetToolState, const FNavigationToolItemPtr /*InItem*/);

	FNavigationToolItem(INavigationTool& InTool, const FNavigationToolItemPtr& InParentItem);

	//~ Begin INavigationToolItem
	virtual INavigationTool& GetOwnerTool() const final override;
	virtual TSharedPtr<FNavigationToolProvider> GetProvider() const final override;
	virtual FNavigationToolSaveState* GetProviderSaveState() const override;
	virtual bool IsItemValid() const override;
	virtual void RefreshChildren() override;
	virtual void ResetChildren() override;
	virtual const TArray<FNavigationToolItemPtr>& GetChildren() const override { return Children; }
	virtual void FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive) override;
	virtual void FindValidChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive) override;
	virtual TArray<FNavigationToolItemPtr> FindPath(const TArray<FNavigationToolItemPtr>& InItems) const override;
	virtual TArray<FNavigationToolItemPtr>& GetChildrenMutable() override { return Children; }
	virtual IndexType GetChildIndex(const FNavigationToolItemPtr& ChildItem) const override;
	virtual bool ShouldSort() const override { return false; }
	virtual bool CanAddChild(const FNavigationToolItemPtr& InChild) const override;
	virtual bool AddChild(const FNavigationToolAddItemParams& InAddItemParams) override;
	virtual bool RemoveChild(const FNavigationToolRemoveItemParams& InRemoveItemParams) override;
	virtual FNavigationToolItemPtr GetParent() const override { return WeakParent.Pin(); }
	virtual void SetParent(FNavigationToolItemPtr InParent) override;
	virtual bool CanBeTopLevel() const override { return false; }
	virtual bool IsAllowedInTool() const override { return true; }
	virtual FText GetDisplayName() const override { return FText::GetEmpty(); }
	virtual FText GetClassName() const override { return FText::GetEmpty(); }
	virtual ENavigationToolItemViewMode GetSupportedViewModes(const INavigationToolView& InToolView) const override;
	virtual FNavigationToolItemId GetItemId() const override final;
	virtual FSlateColor GetItemLabelColor() const override;
	virtual FLinearColor GetItemTintColor() const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual FSlateColor GetIconColor() const override;
	virtual TSharedRef<SWidget> GenerateLabelWidget(const TSharedRef<SNavigationToolTreeRow>& InRow) override;
	virtual bool ShowVisibility() const override { return false; }
	virtual bool CanReceiveParentVisibilityPropagation() const override { return false; }
	virtual bool GetVisibility() const override { return false; }
	virtual bool CanAutoExpand() const override { return true; }
	virtual bool CanDelete() const override { return false; }
	virtual bool Delete() override;
	virtual void AddFlags(ENavigationToolItemFlags Flags) override;
	virtual void RemoveFlags(ENavigationToolItemFlags Flags) override;
	virtual bool HasAnyFlags(ENavigationToolItemFlags Flags) const override;
	virtual bool HasAllFlags(ENavigationToolItemFlags Flags) const override;
	virtual void SetFlags(ENavigationToolItemFlags InFlags) override { ItemFlags = InFlags; }
	virtual ENavigationToolItemFlags GetFlags() const override { return ItemFlags; }
	virtual TArray<FName> GetTags() const override { return TArray<FName>(); };
	virtual TOptional<EItemDropZone> CanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone) override;
	virtual FReply AcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone) override;
	virtual FOnRenameAction& OnRenameAction() override { return OnRenameActionDelegate; }
	virtual FOnExpansionChanged& OnExpansionChanged() override { return OnExpansionChangedDelegate; }
	virtual bool IsExpanded() const override;
	virtual void SetExpansion(const bool bInExpand) override;
	//~ End INavigationToolItem

	//~ Begin IColorExtension
	virtual TOptional<FColor> GetColor() const override;
	virtual void SetColor(const TOptional<FColor>& InColor) override;
	//~ End IColorExtension

protected:
	friend class INavigationTool;

	void SetProvider(const TWeakPtr<FNavigationToolProvider>& InWeakProvider);

	/** Gets the Item Id with the latest information (e.g. parent, object, etc)*/
	virtual FNavigationToolItemId CalculateItemId() const = 0;

	/** Sets the ItemId member var to what CalculateItemId returns */
	void RecalculateItemId();
	
	/** The actual implementation of putting the given Item under the Children array */
	void AddChildChecked(const FNavigationToolAddItemParams& InAddItemParams);

	/** The actual implementation of removing the given item from the Children array */
	bool RemoveChildChecked(const FNavigationToolRemoveItemParams& InRemoveItemParams);
	
	/** Careful handling of multiple children being detected and added to this item children array */
	void HandleNewSortableChildren(TArray<FNavigationToolItemPtr> InSortableChildren);

	/** Reference to the Owning Navigation Tool */
	INavigationTool& Tool;

	/** Tool provider that is responsible for the creation of this item */
	TWeakPtr<FNavigationToolProvider> WeakProvider;

	/** Weak pointer to the Parent Item. Can be null, but if valid, the Parent should have this item in the Children Array */
	FNavigationToolItemWeakPtr WeakParent;

	/** Array of Shared pointers to the Child Items. These Items should have their ParentWeak pointing to this item */
	TArray<FNavigationToolItemPtr> Children;

	/** Delegate for when Expansion changes in the Item */
	FOnExpansionChanged OnExpansionChangedDelegate;
	
	/** The delegate for renaming */
	FOnRenameAction OnRenameActionDelegate;

	/** The current flags set for this item */
	ENavigationToolItemFlags ItemFlags = ENavigationToolItemFlags::None;

	FNavigationToolItemId ItemId;
};

} // namespace UE::SequenceNavigator
