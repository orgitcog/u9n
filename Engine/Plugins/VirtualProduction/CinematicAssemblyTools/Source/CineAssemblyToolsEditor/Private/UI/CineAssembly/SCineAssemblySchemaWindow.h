// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "CineAssemblySchema.h"
#include "Input/DragAndDrop.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"

struct FPropertyAndParent;

/** Mode that configures the UI based on the intended user interactions with the Cine Assembly Schema asset */
enum class ESchemaConfigMode : uint8
{
	CreateNew,
	Edit
};

/** Row widget for the asset list view */
class SSchemaAssetTableRow : public STableRow<TSharedPtr<FString>>
{
public:
	/** Editable text widget stored in this row to easily trigger edit mode */
	TSharedPtr<SInlineEditableTextBlock> TextBlock;
};

/** An entry in the content folder tree view with knowledge of its path, children, and parent */
struct FSchemaTreeItem
{
	/** The types of items that can be represented in this tree view */
	enum class EItemType : uint8
	{
		Asset,
		Folder
	};

	/** The type of this tree item */
	EItemType Type;

	/** The relative path of this tree item */
	FString Path;

	/** The parent of this item in the tree */
	TSharedPtr<FSchemaTreeItem> Parent;

	/** The children of this item in the tree that are Asset types */
	TArray<TSharedPtr<FSchemaTreeItem>> ChildAssets;

	/** The children of this item in the tree that are Folder types */
	TArray<TSharedPtr<FSchemaTreeItem>> ChildFolders;

	/** The text widget that displays this item's name and supports renaming */
	TSharedPtr<SInlineEditableTextBlock> NameWidget;
};

/** Drag and Drop operation to handle dragging schema assets and folders  */
class FSchemaAssetDragDrop : public FDragDropOperation
{
public:

	DRAG_DROP_OPERATOR_TYPE(FSchemaAssetDragDrop, FDragDropOperation)

	DECLARE_DELEGATE_OneParam(FOnDropNotHanlded, const FString&);

	FSchemaAssetDragDrop() = default;

	static TSharedRef<FSchemaAssetDragDrop> New(const FString& InName);

	// Begin FDragDropOperation overrides
	virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent) override;
	virtual void OnDragged(const class FDragDropEvent& DragDropEvent) override;
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
	// End FDragDropOperation overrides

	/** The source item being dragged. This could be null if the thing being dragged is from the Asset List (not the tree) */
	TSharedPtr<FSchemaTreeItem> SourceTreeItem;

	/** The name of the item being displayed, used by the decorator and to name the new tree item that will get made when dropped */
	FString ItemName;

	/** Callback to handle the case where an item is dropped somewhere not on the tree view */
	FOnDropNotHanlded OnDropNotHandled;
};

/**
 * A window for configuring the properties of a UCineAssemblySchema
 */
class SCineAssemblySchemaWindow : public SCompoundWidget
{
public:
	SCineAssemblySchemaWindow() = default;
	~SCineAssemblySchemaWindow();

	SLATE_BEGIN_ARGS(SCineAssemblySchemaWindow) {}
	SLATE_END_ARGS()

	/** Widget construction, initialized with the path where a new schema asset will be created */
	void Construct(const FArguments& InArgs, const FString& InCreateAssetPath);

	/** Widget construction, initialized with the schema asset being edited */
	void Construct(const FArguments& InArgs, UCineAssemblySchema* InSchema);

	/**
	 * Widget construction, initialized with the GUID of the schema to be edited
	 * The widget will search the asset registry to find the schema asset with the matching GUID,
	 * and then update the widget contents accordingly.
	 */
	void Construct(const FArguments& InArgs, FGuid InSchemaGuid);

	/** Returns the name of the schema asset being edited */
	FString GetSchemaName();

	/** Searches the asset registry for a Cine Assembly Schema matching the input ID and updates the UI */
	void FindSchema(FGuid SchemaID);

private:
	/** Constructs the main UI for the widget */
	TSharedRef<SWidget> BuildUI();

	/** Creates the panel that displays the tab menu */
	TSharedRef<SWidget> MakeMenuPanel();

	/** Creates the panel that displays the content for each tab */
	TSharedRef<SWidget> MakeContentPanel();

	/** Creates the buttons on the bottom of the window */
	TSharedRef<SWidget> MakeButtonsPanel();

	/** Creates the content for the Details tab */
	TSharedRef<SWidget> MakeDetailsTabContent();

	/** Creates the content for the Metadata tab */
	TSharedRef<SWidget> MakeMetadataTabContent();

	/** Creates the content for the Hierarchy tab */
	TSharedRef<SWidget> MakeHierarchyTabContent();

	/** Creates the content for the Details and Data tabs (metadata properties are only shown in the Data tab) */
	TSharedRef<SWidget> MakeDetailsWidget(bool bShowMetadata);

	/** Returns true if a schema already exists with the input name */
	bool DoesSchemaExistWithName(const FString& SchemaName) const;

	/** Validates the user input text for the schema name */
	bool ValidateSchemaName(const FText& InText, FText& OutErrorMessage) const;

	/** Filter used by the Details Views to determine which schema properties to display */
	bool IsPropertyVisible(const FPropertyAndParent& PropertyAndParent, bool bShowMetadata);

	/** Closes the window and indicates that a new asset should be created by the asset factory */
	FReply OnCreateAssetClicked();

	/** Closes the window and indicates that no assets should be created by the asset factory */
	FReply OnCancelClicked();

	/** Adds a new entry to the asset list view */
	FReply OnAddAssetClicked();

	/** Adds a new folder item to the content tree view */
	FReply OnAddFolderClicked();

	/** Generates a row in the asset list view */
	TSharedRef<ITableRow> OnGenerateAssetRow(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Callback when the asset list view rebuilds itself, used to make the textbox in the most recently added row editable */
	void OnAssetListRebuilt();

	/** Create the context menu when the asset list view is right-clicked */
	TSharedPtr<SWidget> MakeAssetListContextMenu();

	/** Handles key presses on the asset list */
	FReply OnAssetListKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/** Generates the row widget for an entry in the tree view */
	TSharedRef<ITableRow> OnGenerateTreeRow(TSharedPtr<FSchemaTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Gets the children of the input tree view item to build additional tree rows */
	void OnGetChildren(TSharedPtr<FSchemaTreeItem> TreeItem, TArray<TSharedPtr<FSchemaTreeItem>>& OutNodes);

	/** Callback when the content tree view rebuilds itself, used to make the textbox in the most recently added row editable */
	void OnTreeItemsRebuilt();

	/** Create the context menu when the content tree view is right-clicked */
	TSharedPtr<SWidget> MakeContentTreeContextMenu();

	/** Handles key presses on the tree view */
	FReply OnTreeViewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/** Build the content tree from the list of folders and assets saved in the schema properties */
	void InitializeContentTree();

	/** Recursively expands every item in the tree view */
	void ExpandTreeRecursive(TSharedPtr<FSchemaTreeItem> TreeItem) const;

	/** Recursively get the path of every folder item in the tree view */
	void GetFolderListRecursive(TSharedPtr<FSchemaTreeItem> TreeItem, TArray<FString>& FolderList) const;

	/** Recursively get the path of every asset item in the tree view */
	void GetAssetListRecursive(TSharedPtr<FSchemaTreeItem> TreeItem, TArray<FString>& AssetPathList) const;

	/** Returns the tree item whose path matches the input path */
	TSharedPtr<FSchemaTreeItem> FindItemAtPathRecursive(TSharedPtr<FSchemaTreeItem> TreeItem, const FString& Path) const;

	/** Modifies the path of the input tree item and all of its children (recursively) with the input new path */
	void SetChildrenPathRecursive(TSharedPtr<FSchemaTreeItem> TreeItem, const FString& NewPath);

	/** Finds the row for the input item and puts its textblock into edit mode */
	void EnterEditMode(TSharedPtr<FString> ItemToRename);

	/** Renames the tree item, and updates the paths of all of its children */
	void OnTreeItemTextCommitted(const FText& InText, ETextCommit::Type InCommitType, TSharedPtr<FSchemaTreeItem> TreeItem);

	/** Callback when one of the properties of the schema being configured changes */
	void OnSchemaPropertiesChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	/** Validate the text entered by the user to ensure it will be a valid asset name */
	bool ValidateAssetName(const FText& InText, FText& OutErrorMessage) const;

	/** Validate the text entered by the user to ensure it will be a valid folder name */
	bool ValidateFolderName(const FText& InText, FText& OutErrorMessage, TSharedPtr<FSchemaTreeItem> TreeItem) const;

	/** Remove the input item from the asset list */
	void DeleteAssetItem(TSharedPtr<FString> InItem);

	/** Remove the input item and its children from the tree view */
	void DeleteTreeItem(TSharedPtr<FSchemaTreeItem> TreeItem);

	/** Begins a drag and drop event to drag an item out of the asset list view */
	FReply OnAssetRowDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Begins a drag and drop event to drag an item out of the content tree view */
	FReply OnTreeRowDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Callback when an item is dropped onto a folder in the content tree view to add it to the children of that row */
	FReply OnTreeRowAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, TSharedPtr<FSchemaTreeItem> InItem);

	/** Generates a unique folder path name, assuming the input item will be the parent */
	FString MakeUniqueFolderPath(TSharedPtr<FSchemaTreeItem> InItem);

	/** Returns true if the input parent, or any of its child folders, contains in the input item */
	bool ContainsRecursive(TSharedPtr<FSchemaTreeItem> InParent, TSharedPtr<FSchemaTreeItem> InItem);

	/** Update the list of folders to create from the current set in the tree view */
	void UpdateFolderList();

	/** Update the list of assets to create from the current set in the tree view */
	void UpdateAssetList();

	/** Cached content browser settings, used to restore defaults when closing the window */
	bool bShowEngineContentCached = false;
	bool bShowPluginContentCached = false;

private:
	/** Switcher that controls which menu tab is currently visible */
	TSharedPtr<SWidgetSwitcher> MenuTabSwitcher;

	/** Switcher that controls which tab widget in the Asset Tab is currently visible */
	TSharedPtr<SWidgetSwitcher> AssetTabSwitcher;

	/** Transient object used only by this UI to configure the properties of the new asset that will get created by the factory */
	TStrongObjectPtr<UCineAssemblySchema> SchemaToConfigure = nullptr;

	/** Mode that configures the UI based on the intended user interactions with the Cine Assembly schema asset */
	ESchemaConfigMode Mode = ESchemaConfigMode::CreateNew;

	/** The root path where the configured schema will be created */
	FString CreateAssetPath;

	/** List items sources for the asset list view */
	TArray<TSharedPtr<FString>> AssetListItems;

	/** List view of assets that will be associate with the schema */
	TSharedPtr<SListView<TSharedPtr<FString>>> AssetListView;

	/** Items source for the tree view */
	TArray<TSharedPtr<FSchemaTreeItem>> TreeItems;

	/** The root item in the tree view */
	TSharedPtr<FSchemaTreeItem> RootItem;

	/** An item representing where the top-level assembly should be created */
	TSharedPtr<FSchemaTreeItem> TopLevelAssemblyItem;

	/** The tree view of content folders and assets for this schema */
	TSharedPtr<STreeView<TSharedPtr<FSchemaTreeItem>>> TreeView;

	/** The mostly recently added tree item, used to allow the user to immediately rename the item after the tree view updates to show it */
	TSharedPtr<FSchemaTreeItem> MostRecentlyAddedItem;
};
