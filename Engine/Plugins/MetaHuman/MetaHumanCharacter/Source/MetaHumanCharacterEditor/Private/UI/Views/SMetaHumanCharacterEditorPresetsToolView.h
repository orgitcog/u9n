// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "SMetaHumanCharacterEditorToolView.h"

struct FContentBrowserItem;
struct FMetaHumanCharacterAssetsSection;
struct FMetaHumanCharacterAssetViewItem;
struct FMetaHumanObserverChanges;
struct FSlateBrush;
class FUICommandList;
class SMetaHumanCharacterEditorAssetViewsPanel;
class SWindow;
class UMetaHumanCharacterEditorPresetsTool;

/** View for displaying the Presets Tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorPresetsToolView
	: public SMetaHumanCharacterEditorToolView
	, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorPresetsToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorPresetsTool* InTool);

protected:
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual UInteractiveToolPropertySet* GetToolProperties() const override;
	virtual void MakeToolView() override;
	//~ End of SMetaHumanCharacterEditorToolView interface

	//~ Begin FNotifyHook interface
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	//~ End of FNotifyHook interface

private:
	/** Creates the section widget for showing the Presets View. */
	TSharedRef<SWidget> CreatePresetsToolViewPresetsViewSection();

	/** Creates the section widget for showing the Management properties. */
	TSharedRef<SWidget> CreatePresetsToolViewManagementSection();

	/** Creates the section widget for showing the Library properties. */
	TSharedRef<SWidget> CreatePresetsToolViewLibrarySection();

	/** Creates the Preset Properties window content. */
	TSharedRef<SWidget> MakePresetPropertiesWindow();

	/** Called when the Accept button in the Preset Properties window is clicked. */
	FReply OnAcceptPresetPropertiesClicked();

	/** Called when the Cancel button in the Preset Properties window is clicked. */
	FReply OnCancelPresetPropertiesClicked();

	/** True if properties editing is enabled. */
	bool IsPropertiesEditingEnabled() const;

	/** True when the Inspect button is enabled. */
	bool IsInspectButtonEnabled() const;

	/** Called when the Inspect button is clicked. */
	FReply OnInspectButtonClicked();

	/** Opens the preset properties window. */
	void OpenPresetPropertiesWindow();

	/** Applies the preset properties values. */
	void ApplyPreset();

	/** Gets and array of items containing the stored Character individual assets. */
	TArray<FMetaHumanCharacterAssetViewItem> GetCharacterIndividualAssets() const;

	/** Gets the sections array for the wardrobe asset views panel. */
	TArray<FMetaHumanCharacterAssetsSection> GetAssetViewsSections() const;

	/** Called when to populate asset views with items. */
	TArray<FMetaHumanCharacterAssetViewItem> OnPopulateAssetViewsItems(const FMetaHumanCharacterAssetsSection& InSection, const FMetaHumanObserverChanges& InChanges);

	/** Called to process an array of dropped folders in the asset views panel. */
	void OnProcessDroppedFolders(const TArray<FContentBrowserItem> Items, const FMetaHumanCharacterAssetsSection& InSection) const;

	/** Called when the given item has been activated. */
	void OnPresetsToolItemActivated(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

	/** Called when the given item has been deleted. */
	void OnPresetsToolVirtualItemDeleted(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

	/** True if the given item can be deleted. */
	bool CanDeletePresetsToolVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const;

	/** Called when the folder has been deleted. */
	void OnPresetsPathsFolderDeleted(const FMetaHumanCharacterAssetsSection& InSection);

	/** True if the given folder can be deleted. */
	bool CanDeletePresetsPathsFolder(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item, const FMetaHumanCharacterAssetsSection& InSection) const;

	/** Called when the given item has been moved in a virtual folder. */
	void OnHandlePresetsVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

	/** Called when the Project Settings selected directory paths have been changed. */
	void OnPresetsDirectoriesChanged();

	/** Reference to this view command list. */
	TSharedPtr<FUICommandList> CommandList;

	/** Reference to this Asset Views panel. */
	TSharedPtr<SMetaHumanCharacterEditorAssetViewsPanel> AssetViewsPanel;

	/** Reference to the window which displays the Presets properties. */
	TSharedPtr<SWindow> PresetsPropertiesWindow;

	/** Name identifier for the slot where virtual assets from the presets library are stored. */
	static FName PresetsLibraryAssetsSlotName;
};
