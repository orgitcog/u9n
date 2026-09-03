// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "CineAssembly.h"
#include "CineAssemblySchema.h"
#include "IDetailsView.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SWindow.h"
#include "Widgets/Views/STreeView.h"

/** An entry in the hierarchy tree view */
struct FHierarchyTreeItem
{
	/** The types of items that can be represented in this tree view */
	enum class EItemType : uint8
	{
		Asset,
		Folder
	};

	/** The type of this tree item */
	EItemType Type;

	/** The relative path of this tree item (possibly containing tokens) */
	FTemplateString Path;

	/** The children of this item in the tree that are Asset types */
	TArray<TSharedPtr<FHierarchyTreeItem>> ChildAssets;

	/** The children of this item in the tree that are Folder types */
	TArray<TSharedPtr<FHierarchyTreeItem>> ChildFolders;
};

/**
 * A window for configuring the properties of a UCineAssembly asset
 */
class SCineAssemblyConfigWindow : public SWindow
{
public:
	SCineAssemblyConfigWindow() = default;
	~SCineAssemblyConfigWindow();

	SLATE_BEGIN_ARGS(SCineAssemblyConfigWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FString& InCreateAssetPath);

private:
	/** Creates the panel that displays the available Cine Assembly templates */
	TSharedRef<SWidget> MakeCineTemplatePanel();

	/** Creates the panel that holds the various tabs with properties of the Cine Assembly */
	TSharedRef<SWidget> MakeInfoPanel();

	/** Creates the buttons on the bottom of the window */
	TSharedRef<SWidget> MakeButtonsPanel();

	/** Creates the widget to display for the Overview tab */
	TSharedRef<SWidget> MakeDetailsWidget();

	/** Creates the widget to display for the Hierarchy tab */
	TSharedRef<SWidget> MakeHierarchyWidget();

	/** Creates the widget to display for the Notes tab */
	TSharedRef<SWidget> MakeNotesWidget();

	/** Returns the text to display on the Create Asset button, based on the selected schema */
	FText GetCreateButtonText() const;

	/** Closes the window and indicates that a new asset should be created by the asset factory */
	FReply OnCreateAssetClicked();

	/** Closes the window and indicates that no assets should be created by the asset factory */
	FReply OnCancelClicked();

	/** Updates the UI and CineAssembly properties based on the selected schema */
	void OnSchemaSelected(const FAssetData& AssetData);

	/** 
	 * Evaluates the input template string with the naming tokens subsystem, and stores the result in the Resolved text.
	 * This function is throttled to only run at a set frequency, to avoid the potential to constantly query the naming tokens subsystem.
	 */
	void EvaluateTokenString(FTemplateString& StringToEvaluate);

	/** Populate the tree view items from the list of folders and assets specified by the selected schema */
	void PopulateHierarchyTree();

	/** Generates the row widget for an entry in the tree view */
	TSharedRef<ITableRow> OnGenerateTreeRow(TSharedPtr<FHierarchyTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Gets the children of the input tree view item to build additional tree rows */
	void OnGetChildren(TSharedPtr<FHierarchyTreeItem> TreeItem, TArray<TSharedPtr<FHierarchyTreeItem>>& OutNodes);

	/** Returns the tree item whose path matches the input path */
	TSharedPtr<FHierarchyTreeItem> FindItemAtPathRecursive(TSharedPtr<FHierarchyTreeItem> TreeItem, const FString& Path) const;

	/** Recursively expands every item in the tree view */
	void ExpandTreeRecursive(TSharedPtr<FHierarchyTreeItem> TreeItem) const;

	/** Recursively evaluates the tokens in the path of each item in the tree view */
	void EvaluateHierarchyTokensRecursive(TSharedPtr<FHierarchyTreeItem> TreeItem);

private:
	/** Switcher that controls which tab widget is currently visible */
	TSharedPtr<SWidgetSwitcher> TabSwitcher;

	/** Details View displaying the reflected properties of the Cine Assembly being configured */ 
	TSharedPtr<IDetailsView> DetailsView;

	/** Transient object used only by this UI to configure the properties of the new asset that will get created by the Factory */
	TStrongObjectPtr<UCineAssembly> CineAssemblyToConfigure = nullptr;

	/** The currently selected schema to use as a base for configuring the new cine assembly */
	UCineAssemblySchema* SelectedSchema = nullptr;

	/** The root path where the configured assembly will be created */
	FString CreateAssetPath;

	/** The last time the naming tokens were updated */
	FDateTime LastTokenUpdateTime;

	/** Items source for the tree view */
	TArray<TSharedPtr<FHierarchyTreeItem>> HierarchyTreeItems;

	/** The root item in the tree view */
	TSharedPtr<FHierarchyTreeItem> RootItem;

	/** A read-only tree view of folders and assets that will be created based on the selected schema */
	TSharedPtr<STreeView<TSharedPtr<FHierarchyTreeItem>>> HierarchyTreeView;

	/** Cached content browser settings, used to restore defaults when closing the window */
	bool bShowEngineContentCached = false;
	bool bShowPluginContentCached = false;
};

/** A panel that displays properties of a Cine Assembly asset */
class SCineAssemblyEditWidget : public SCompoundWidget
{
public:
	SCineAssemblyEditWidget() = default;
	~SCineAssemblyEditWidget();

	SLATE_BEGIN_ARGS(SCineAssemblyEditWidget) {}
	SLATE_END_ARGS()

	/** Widget construction, initialized with the assembly asset being edited */
	void Construct(const FArguments& InArgs, UCineAssembly* InAssembly);

	/**
	 * Widget construction, initialized with the GUID of the assembly to be edited
	 * The widget will search the asset registry to find the assembly asset with the matching GUID,
	 * and then update the widget contents accordingly. 
	 */
	void Construct(const FArguments& InArgs, FGuid InAssemblyGuid);

	/** Returns the name of the assembly asset being edited */
	FString GetAssemblyName();

	/** Searches the asset registry for a Cine Assembly matching the input ID and updates the UI */
	void FindAssembly(FGuid AssemblyID);

	/** Returns true if the Assembly asset has a rendered thumbnail (such as from the Sequencer preview) */
	bool HasRenderedThumbnail();

private:
	/** Constructs the main UI for the widget */ 
	TSharedRef<SWidget> BuildUI();

	/** Creates the widget to display for the Overview tab */
	TSharedRef<SWidget> MakeOverviewWidget();

	/** Filter used by the Details View to determine which custom rows to show */
	bool IsCustomRowVisible(FName RowName, FName ParentName);

private:
	/** Switcher that controls which tab widget is currently visible */
	TSharedPtr<SWidgetSwitcher> TabSwitcher;

	/** Transient object used only by this UI to configure the properties of the new asset that will get created by the Factory */
	UCineAssembly* CineAssembly = nullptr;
};