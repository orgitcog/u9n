// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "MetaHumanCharacter.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STileView.h"

struct FAssetData;
class FAssetThumbnailPool;
struct FContentBrowserItem;
struct FMetaHumanObserverChanges;
class FDeferredCleanupSlateBrush;
struct FSlateBrush;
class SSearchBox;
struct FTimerHandle;
class SVerticalBox;
class UMetaHumanCharacter;

/** Struct used to represent an asset view item. */
struct FMetaHumanCharacterAssetViewItem
{
	FMetaHumanCharacterAssetViewItem(const FAssetData& InAssetData, const FName& InSlotName, const FMetaHumanPaletteItemKey& InPaletteItemKey, const TSharedPtr<FAssetThumbnailPool>& InAssetThumbnailPool);
	~FMetaHumanCharacterAssetViewItem();

	FAssetData AssetData;
	FName SlotName;
	FMetaHumanPaletteItemKey PaletteItemKey;
	TSharedPtr<FAssetThumbnail> Thumbnail;
	TSharedPtr<FDeferredCleanupSlateBrush> ThumbnailImageOverride;
};

/** Drag drop action whick allow to hold data about asset view items. **/
class FMetaHumanCharacterAssetViewItemDragDropOp : public FAssetDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FMetaHumanCharacterAssetViewItemDragDropOp, FAssetDragDropOp)

	TSharedPtr<FMetaHumanCharacterAssetViewItem> AssetItem;
	static TSharedRef<FMetaHumanCharacterAssetViewItemDragDropOp> New(FAssetData AssetData, UActorFactory* ActorFactory, TSharedPtr<FMetaHumanCharacterAssetViewItem> AssetItem);
};

DECLARE_DELEGATE_RetVal_OneParam(FName, FMetaHumanCharacterEditorOnOverrideName, const FName&);

DECLARE_DELEGATE_RetVal_OneParam(FText, FMetaHumanCharacterOnOverrideThumbnailName, TSharedPtr<FMetaHumanCharacterAssetViewItem> /*Item*/);

DECLARE_DELEGATE_RetVal_TwoParams(TArray<FMetaHumanCharacterAssetViewItem>, FMetaHumanCharacterEditorOnPopulateItems, const FMetaHumanCharacterAssetsSection&, const FMetaHumanObserverChanges&);

DECLARE_DELEGATE_RetVal_OneParam(UObject*, FMetaHumanCharacterEditorOnProcessAssetData, const FAssetData&);

DECLARE_DELEGATE_TwoParams(FMetaHumanCharacterEditorOnProcessFolders, const TArray<FContentBrowserItem>, const FMetaHumanCharacterAssetsSection&);

DECLARE_DELEGATE_TwoParams(FMetaHumanCharacterEditorOnItemSelectionChanged, TSharedPtr<FMetaHumanCharacterAssetViewItem> /*SelectedItem*/, ESelectInfo::Type /*SelectInfo*/);

DECLARE_DELEGATE_OneParam(FMetaHumanCharacterEditorAssetViewItemDelegate, TSharedPtr<FMetaHumanCharacterAssetViewItem> /*Item*/);

DECLARE_DELEGATE_OneParam(FMetaHumanCharacterEditorAssetViewSectionDelegate, const FMetaHumanCharacterAssetsSection& /*Section*/);

DECLARE_DELEGATE_RetVal_OneParam(bool, FMetaHumanCharacterEditorOnGetItemState, TSharedPtr<FMetaHumanCharacterAssetViewItem> /*Item*/);

DECLARE_DELEGATE_RetVal_TwoParams(bool, FMetaHumanCharacterEditorOnGetSectionState, TSharedPtr<FMetaHumanCharacterAssetViewItem> /*Item*/, const FMetaHumanCharacterAssetsSection& /*Section*/);

/** Widget that represents an asset view item in a tile view. */
class SMetaHumanCharacterAssetViewItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterAssetViewItem) {}

		/** Data for the asset this item represents. */
		SLATE_ARGUMENT(TSharedPtr<FMetaHumanCharacterAssetViewItem>, AssetItem)

		/** The handle to the thumbnail this item should render. */
		SLATE_ARGUMENT(TSharedPtr<FAssetThumbnail>, AssetThumbnail)

		/** Called to get wether the item is selected in the view. */
		SLATE_ARGUMENT(FIsSelected, IsSelected)

		/** Called to get the item checked state. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, IsChecked)

		/** Called to get the item supported state. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, IsAvailable)

		/** Called to get the item active state. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, IsActive)

		/** Called to get thumbnail brushes overrides. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnOverrideThumbnail)

		/** Called to get thumbnail name overrides. */
		SLATE_EVENT(FMetaHumanCharacterOnOverrideThumbnailName, OnOverrideThumbnailName)

		/** Called when this item is deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnDeleted)

		/** Called to get wheter this item can be deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, CanDelete)

		/** Called when the owner folder of this item is deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnDeletedFolder)

		/** Called to get wheter the owner folder of this item can be deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, CanDeleteFolder)

	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs);

	//~ Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	//~ End SWidget interface

private:
	/** Generates the thumbnail widget for this item. */
	TSharedRef<SWidget> GenerateThumbnailWidget();

	/** Generates the context menu for this item. */
	TSharedRef<SWidget> GenerateAssetItemContextMenu();

	/** Gets the context menu unique name. */
	FName GetContextMenuName() const { return FName(TEXT("MetaHumanCharacter.AssetViewItem.ContextMenu")); }

	/** Gets the asset area overlay background brush. */
	const FSlateBrush* GetAssetAreaOverlayBackgroundImage() const;

	/** Gets the name area background brush. */
	const FSlateBrush* GetNameAreaBackgroundImage() const;

	/** Gets the color for the name area text. */
	FSlateColor GetNameAreaTextColor() const;

	/** Gets the thumbnail name for current item */
	FText GetThumbnailName() const;

	/** True is the item asset is dirty. */
	bool IsItemDirty() const;

	/** True is the item asset is checked. */
	bool IsItemChecked() const;

	/** True is the item asset is available. */
	bool IsItemAvailable() const;

	/** True is the item asset is active. */
	bool IsItemActive() const;

	/** Gets the visibility of the thumbnail chip border image. */
	EVisibility GetAssetColorVisibility() const;

	/** Gets the visibility of the dirty state icon. */
	EVisibility GetDirtyIconVisibility() const;

	/** Gets the visibility of the checked state icon. */
	EVisibility GetCheckedIconVisibility() const;

	/** Gets the visibility of the available state icon. */
	EVisibility GetAvailableIconVisibility() const;

	/** Gets the visibility of the active state icon. */
	EVisibility GetActiveIconVisibility() const;

	/** Gets the visibility of the states overlay border. */
	EVisibility GetStatesOverlayBorderVisibility() const;

	/** The delegate to execute to get wether the item is selected in the view. */
	FIsSelected IsSelected;

	/** The delegate to execute to get the item checked state. */
	FMetaHumanCharacterEditorOnGetItemState IsChecked;

	/** The delegate to execute to get the item available state. */
	FMetaHumanCharacterEditorOnGetItemState IsAvailable;

	/** The delegate to execute to get the item active state. */
	FMetaHumanCharacterEditorOnGetItemState IsActive;

	/** The delegate to execute to get thumbnail brushes overrides. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnOverrideThumbnailDelegate;

	/** The delegate to execute to get thumbnail name overrides */
	FMetaHumanCharacterOnOverrideThumbnailName OnOverrideThumbnailNameDelegate;

	/** The delegate to execute when this item is deleted. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnDeletedDelegate;

	/** The delegate to execute to get wheter this item can be deleted. */
	FMetaHumanCharacterEditorOnGetItemState CanDeleteDelegate;

	/** The delegate to execute when the owner folder of this item is deleted. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnDeletedFolderDelegate;

	/** The delegate to execute to get wheter the owner folder of this item can be deleted. */
	FMetaHumanCharacterEditorOnGetItemState CanDeleteFolderDelegate;

	/** Reference to the thumbnail widget. */
	TSharedPtr<SWidget> Thumbnail;

	/** Reference to the thumbnail that this item is rendering */
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	/** The asset view item this widget is based on */
	TSharedPtr<FMetaHumanCharacterAssetViewItem> AssetItem;

	/** True when a drag is over this item with a drag operation that we know how to handle. The operation itself may not be valid to drop. */
	bool bDraggedOver = false;
};

/** Displays assets in the observed asset section as tile view. */
class SMetaHumanCharacterEditorAssetsView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorAssetsView)
		: _SelectionMode(ESelectionMode::Single)
		, _SlotName(NAME_None)
		, _MaxHeight(320.f)
		, _AutoHeight(false)
		, _AllowDragging(true)
		, _AllowDropping(false)
		, _HasVirtualFolder(false)
		{}

		/** The assets sections this view is based on. */
		SLATE_ARGUMENT(TArray<FMetaHumanCharacterAssetsSection>, Sections)

		/** List of assets to exclude from the view. */
		SLATE_ARGUMENT(TArray<TWeakObjectPtr<UObject>>, ExcludedObjects)

		/** The selection mode of the items in the view. */
		SLATE_ARGUMENT(ESelectionMode::Type, SelectionMode)

		/** The slot name associated to the view sections, or NAME_None if slots are not allowed. */
		SLATE_ARGUMENT(FName, SlotName)

		/** The name label of the view. */
		SLATE_ARGUMENT(FString, Label)

		/** The asset view maximum height. */
		SLATE_ARGUMENT(float, MaxHeight)

		/** True if the asset view should occupy all the available space. */
		SLATE_ARGUMENT(bool, AutoHeight)

		/** True if asset dragging is allowed. */
		SLATE_ARGUMENT(bool, AllowDragging)

		/** True if asset dropping is allowed. */
		SLATE_ARGUMENT(bool, AllowDropping)

		/** True if this view should be treated as a virtual folder. */
		SLATE_ARGUMENT(bool, HasVirtualFolder)

		/** Called to get thumbnail brushes overrides. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnOverrideThumbnail)

		/** Called to get thumbnail brushes overrides. */
		SLATE_EVENT(FMetaHumanCharacterOnOverrideThumbnailName, OnOverrideThumbnailName)

		/** Called to process a dropped item, before checking if it matches class requirements. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnProcessAssetData, OnProcessDroppedItem)

		/** Called to process an array of dropped folders. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnProcessFolders, OnProcessDroppedFolders)

		/** Called when asset views are populated. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnPopulateItems, OnPopulateItems)

		/** Called when asset views selection has changed. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnItemSelectionChanged, OnSelectionChanged)

		/** Called when an item is activated. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnItemActivated)

		/** Called when an item is deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnItemDeleted)

		/** Called to get whether an item can be deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, CanDeleteItem)

		/** Called when a folder is deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewSectionDelegate, OnFolderDeleted)

		/** Called to get whether a folder can be deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetSectionState, CanDeleteFolder)

		/** Called when an item is added to the virtual assets folder. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnHadleVirtualItem);

		/** Called to get whether the item is compatible with a section. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetSectionState, IsItemCompatible)

		/** Called to get the item checked state. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, IsItemChecked)

		/** Called to get the item available state. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, IsItemAvailable)

		/** Called to get the item active state. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, IsItemActive)

	SLATE_END_ARGS()

	/** Constructor. */
	SMetaHumanCharacterEditorAssetsView();

	/** Destructor. */
	~SMetaHumanCharacterEditorAssetsView();

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs);

	/** Gets the array of selected items of the view. */
	TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> GetSelectedItems() const;

	/** Gets the array of items of the view. */
	TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> GetItems() const;

	/** Gets the name label of the view. */
	FString GetLabel() const { return Label; }

	/** Gets the current scroll offset of the view. */
	float GetScrollOffset() const;

	/** Populates the list items array. */
	void PopulateListItems(const FMetaHumanObserverChanges& InChanges);

	/** Sets the scroll offset of the view. */
	void SetScrollOffset(float InScrollOffset);

	/** Sets the text filter for the view. */
	void SetFilter(const FText& InNewSearchText);

	/** Sets the given item as selected, if valid. */
	void SetItemSelection(TSharedPtr<FMetaHumanCharacterAssetViewItem> InItem, bool bSelected, ESelectInfo::Type SelectInfo);

	/** Clears the selection. */
	void ClearSelection();

	//~ Begin SWidget interface
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	//~ End SWidget interface

private:
	/** True if the given asset data needs to be filtered. */
	bool IsAssetFiltered(const FAssetData& InAssetData) const;

	/** Called each time an asset view tile is generated. */
	TSharedRef<ITableRow> OnGenerateTile(TSharedPtr<FMetaHumanCharacterAssetViewItem> InItem, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Called when a folder has been deleted. */
	void OnDeletedFolder(TSharedPtr<FMetaHumanCharacterAssetViewItem> InItem) const;

	/** Called to get whether a folder can be deleted or not. */
	bool CanDeleteFolder(TSharedPtr<FMetaHumanCharacterAssetViewItem> InItem) const;

	/** Called when an asset view item is being dragged. */
	FReply OnDraggingAssetItem(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;

	/** Gets the visibility of the dropping area widgets. */
	EVisibility GetDroppingAreaVisibility() const;

	/** Handles the assets in the VirtualFolderItems array. */
	void HandleVirtualFolderAsset(const FAssetData& AssetData);

	/** Saves an asset in the given folder path, if valid. */
	UObject* SaveAssetToSectionFolder(const FAssetData& AssetData, const FString& FolderPath, bool bAllowMoving = false);

	/** The delegate to execute to get thumbnail brushes overrides. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnOverrideThumbnailDelegate;

	/** The delegate to execute to get thumbnail name overrides */
	FMetaHumanCharacterOnOverrideThumbnailName OnOverrideThumbnailNameDelegate;

	/** The delegate to execute to process a dropped item, before checking if it matches class requirements. */
	FMetaHumanCharacterEditorOnProcessAssetData OnProcessDroppedItemDelegate;

	/** The delegate to execute to process an array of dropped folder. */
	FMetaHumanCharacterEditorOnProcessFolders OnProcessDroppedFoldersDelegate;

	/** The delegate to execute when asset views are populated. */
	FMetaHumanCharacterEditorOnPopulateItems OnPopulateItemsDelegate;

	/** The delegate to execute when asset views selection has changed. */
	FMetaHumanCharacterEditorOnItemSelectionChanged OnSelectionChangedDelegate;

	/** The delegate to execute when an item is activated. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnItemActivatedDelegate;

	/** The delegate to execute when an item is deleted. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnItemDeletedDelegate;

	/** The delegate to execute to get whether an item can be deleted. */
	FMetaHumanCharacterEditorOnGetItemState CanDeleteItemDelegate;

	/** The delegate to execute when a folder is deleted. */
	FMetaHumanCharacterEditorAssetViewSectionDelegate OnFolderDeletedDelegate;

	/** The delegate to execute to get whether a folder can be deleted. */
	FMetaHumanCharacterEditorOnGetSectionState CanDeleteFolderDelegate;

	/** The delegate to execute when an item is added to the virtual assets folder. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnHandleVirtualItemDelegate;

	/** The delegate to execute to get whether the item is compatible with a section. */
	FMetaHumanCharacterEditorOnGetSectionState IsItemCompatible;

	/** The delegate to execute to get the item checked state. */
	FMetaHumanCharacterEditorOnGetItemState IsItemChecked;

	/** The delegate to execute to get the item available state. */
	FMetaHumanCharacterEditorOnGetItemState IsItemAvailable;

	/** The delegate to execute to get the item active state. */
	FMetaHumanCharacterEditorOnGetItemState IsItemActive;

	/** Keeps track of the subscriber handle to the directory observer. */
	FDelegateHandle SubscriberHandle;

	/** Reference to the asset thumbnail pool used for creating item thumbnails. */
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;

	/** The array of items displayed in the asset view. */
	TSharedRef<UE::Slate::Containers::TObservableArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>>> ListItems;

	/** Reference to the displayed tile view. */
	TSharedPtr<STileView<TSharedPtr<FMetaHumanCharacterAssetViewItem>>> TileView;

	/** The text filter currently applied. */
	FString SearchText;

	/** True when a drag is over this item with a drag operation that we know how to handle. The operation itself may not be valid to drop. */
	bool bDraggedOver = false;

	/** Slate Arguments. */
	TArray<FMetaHumanCharacterAssetsSection> Sections;
	TArray<TWeakObjectPtr<UObject>> ExcludedObjects;
	ESelectionMode::Type SelectionMode = ESelectionMode::Type::Single;
	FName SlotName;
	FString Label;
	float MaxHeight;
	bool bAutoHeight;
	bool bAllowDragging;
	bool bAllowDropping;
	bool bHasVirtualFolder;
};

/** Displays a selection of assets from specified folders into sorted into editor asset views. */
class SMetaHumanCharacterEditorAssetViewsPanel 
	: public SCompoundWidget
	, public FSelfRegisteringEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorAssetViewsPanel)
		: _AutoHeight(false)
		, _AllowDragging(true)
		, _AllowSlots(true)
		, _AllowMultiSelection(false)
		, _AllowSlotMultiSelection(true)
		{}

		/** The array of sections used for creating asset views. */
		SLATE_ATTRIBUTE(TArray<FMetaHumanCharacterAssetsSection>, AssetViewSections)

		/** List of objects to exclude from the asset views. */
		SLATE_ARGUMENT(TArray<TWeakObjectPtr<UObject>>, ExcludedObjects)

		/** List of classes to look for when creating virtual folders. */
		SLATE_ARGUMENT(TArray<TSubclassOf<UObject>>, VirtualFolderClassesToFilter)

		/** True if the asset views should occupy all the available space. */
		SLATE_ARGUMENT(bool, AutoHeight)

		/** True if asset item dragging is allowed. */
		SLATE_ARGUMENT(bool, AllowDragging)

		/** True if slot multiselection is allowed. */
		SLATE_ARGUMENT(bool, AllowSlots)

		/** True if multiselection is allowed inside a single asset view. */
		SLATE_ARGUMENT(bool, AllowMultiSelection)

		/** True if multiselection is allowed in the different views of the same slot. */
		SLATE_ARGUMENT(bool, AllowSlotMultiSelection)

		/** Called to get slot names overrides. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnOverrideName, OnOverrideSlotName)

		/** Called to get slot names overrides. */
		SLATE_EVENT(FMetaHumanCharacterOnOverrideThumbnailName, OnOverrideThumbnailName)

		/** Called to get thumbnail brushes overrides. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnOverrideThumbnail)

		/** Called to process a dropped item, before checking if it matches class requirements. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnProcessAssetData, OnProcessDroppedItem)

		/** Called to process an array of dropped folders. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnProcessFolders, OnProcessDroppedFolders)

		/** Called when asset views are populated. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnPopulateItems, OnPopulateAssetViewsItems)

		/** Called when asset views selection has changed. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnItemSelectionChanged, OnSelectionChanged)

		/** Called when an item is activated. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnItemActivated)

		/** Called when an item is deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnItemDeleted)

		/** Called to get whether an item can be deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, CanDeleteItem)

		/** Called when a folder is deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewSectionDelegate, OnFolderDeleted)

		/** Called to get whether a folder can be deleted. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetSectionState, CanDeleteFolder)

		/** Called when an item is added to the virtual assets folder. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnHadleVirtualItem);

		/** Called to get whether the item is compatible with a section. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetSectionState, IsItemCompatible)

		/** Called to get the item checked state. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, IsItemChecked)

		/** Called to get the item availabel state. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, IsItemAvailable)

		/** Called to get the item active state. */
		SLATE_EVENT(FMetaHumanCharacterEditorOnGetItemState, IsItemActive)

	SLATE_END_ARGS()

	/** Destructor. */
	~SMetaHumanCharacterEditorAssetViewsPanel();

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs);

	/** Gets the array of selected asset view items of this panel. */
	TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> GetSelectedItems() const;

	/** Gets the owner asset view of the given item, if valid. */
	TSharedPtr<SMetaHumanCharacterEditorAssetsView> GetOwnerAssetView(TSharedPtr<FMetaHumanCharacterAssetViewItem> InSelectedItem) const;

	/** Requests the refreshing of the panel. */
	void RequestRefresh();

	//~ Begin SWidget interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget interface

protected:
	//~ Begin FSelfRegisteringEditorUndoClient interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FSelfRegisteringEditorUndoClient interface

private:
	/** Makes the asset views panel, using the stored parameters. */
	void MakeAssetViewsPanel();

	/** Generates an asset view slot with the given slot name. */
	TSharedRef<SWidget> GenerateAssetViewsSlot(const FName& SlotName);

	/** Generates an asset view slot with the given slot name. */
	TSharedRef<SWidget> GenerateAssetView(const TArray<FMetaHumanCharacterAssetsSection>& Sections, const FName& SlotName, bool bHasVirtualFolder = false);

	/** Generates the toolbar for an asset view section. */
	TSharedRef<SWidget> GenerateSectionToolbar() const;

	/** Generates the settings menu widget for this panel. */
	TSharedRef<SWidget> GenerateSettingsMenuWidget();

	/** Generates an asset view name label using the given parameters. */
	FString GenerateAssetViewNameLabel(const TArray<FMetaHumanCharacterAssetsSection>& Sections, const FName& SlotName, bool bHasVirtualFolder = false) const;

	/** Populates the slot names array. */
	void PopulateSlotNames();

	/** Gets the owner section of the slot with the given slot name. */
	TArray<FMetaHumanCharacterAssetsSection> GetSectionsBySlotName(const FName& SlotName) const;

	/** Gets the settings menu unique name. */
	FName GetSettingsMenuName() const { return FName(TEXT("MetaHumanCharacter.AssetViewsPanel.Menu")); }

	/** Gets a map of all the asset view labels with the matching scroll offsets. */
	TMap<FString, float> GetAssetViewLabelToScrollOffsetMap() const;

	/** Called when the search box text has changed. */
	void OnSearchBoxTextChanged(const FText& InText);

	/** Called when the item selection of and asset view has changed. */
	void OnItemSelectionChanged(TSharedPtr<FMetaHumanCharacterAssetViewItem> InSelectedItem, ESelectInfo::Type InSelectInfo);

	/** Opens the project settings menu. */
	void OpenProjectSettings();

	/** Toggles the show folders option state. */
	void ToggleShowFolders();

	/** Refreshes the panel asset views widgets. */
	void Refresh();

	/** The delegate to execute to get slot names overrides. */
	FMetaHumanCharacterEditorOnOverrideName OnOverrideSlotNameDelegate;

	/** The delegate to execute to get thumbnail brushes overrides. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnOverrideThumbnailDelegate;

	/** The delegate to execute to get thumbnail name overrides */
	FMetaHumanCharacterOnOverrideThumbnailName OnOverrideThumbnailNameDelegate;

	/** The delegate to execute to process a dropped item, before checking if it matches class requirements. */
	FMetaHumanCharacterEditorOnProcessAssetData OnProcessDroppedItemDelegate;

	/** The delegate to execute to process an array of dropped folder. */
	FMetaHumanCharacterEditorOnProcessFolders OnProcessDroppedFoldersDelegate;

	/** The delegate to execute when asset views are populated. */
	FMetaHumanCharacterEditorOnPopulateItems OnPopulateAssetViewsItemsDelegate;

	/** The delegate to execute when asset views selection has changed. */
	FMetaHumanCharacterEditorOnItemSelectionChanged OnSelectionChangedDelegate;

	/** The delegate to execute when an item is activated. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnItemActivatedDelegate;

	/** The delegate to execute when an item is deleted. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnItemDeletedDelegate;

	/** The delegate to execute to get whether an item can be deleted. */
	FMetaHumanCharacterEditorOnGetItemState CanDeleteItemDelegate;

	/** The delegate to execute when a folder is deleted. */
	FMetaHumanCharacterEditorAssetViewSectionDelegate OnFolderDeletedDelegate;

	/** The delegate to execute to get whether a folder can be deleted. */
	FMetaHumanCharacterEditorOnGetSectionState CanDeleteFolderDelegate;

	/** The delegate to execute when an item is added to the virtual assets folder. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnHandleVirtualItemDelegate;

	/** The delegate to execute to get whether the item is compatible with a section. */
	FMetaHumanCharacterEditorOnGetSectionState IsItemCompatible;

	/** The delegate to execute to get the item checked state. */
	FMetaHumanCharacterEditorOnGetItemState IsItemChecked;

	/** The delegate to execute to get the item available state. */
	FMetaHumanCharacterEditorOnGetItemState IsItemAvailable;

	/** The delegate to execute to get the item active state. */
	FMetaHumanCharacterEditorOnGetItemState IsItemActive;

	/** Timer handle used when asset views refresh is requested. */
	FTimerHandle RefreshAssetViewsTimerHandle;

	/** Reference to the search box used for filtering assets. */
	TSharedPtr<SSearchBox> SearchBox;

	/** Reference to the box container widget for the asset views slots. */
	TSharedPtr<SVerticalBox> AssetViewSlotsBox;

	/** The array of asset views displayed in the panel. */
	TArray<TSharedPtr<SMetaHumanCharacterEditorAssetsView>> AssetViews;

	/** The array of slot names used for creating slots. */
	TArray<FName> AssetViewsSlotNames;

	/** True to show sections as single folders. */
	bool bShowFolders = false;

	/** Slate arguments. */
	TAttribute<TArray<FMetaHumanCharacterAssetsSection>> AssetViewSections;
	TArray<TWeakObjectPtr<UObject>> ExcludedObjects;
	TArray<TSubclassOf<UObject>> VirtualFolderClassesToFilter;
	bool bAutoHeight;
	bool bAllowDragging;
	bool bAllowSlots;
	bool bAllowMultiSelection;
	bool bAllowSlotMultiSelection;
};
