// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "SMetaHumanCharacterEditorToolView.h"

struct FContentBrowserItem;
struct FMetaHumanCharacterAssetsSection;
struct FMetaHumanCharacterAssetViewItem;
struct FMetaHumanObserverChanges;
class FReply;
class FUICommandList;
class IDetailsView;
class SMetaHumanCharacterEditorAssetViewsPanel;
class UMetaHumanCharacter;
class UMetaHumanCharacterEditorWardrobeTool;
class UMetaHumanCharacterPipelineSpecification;

/** View for displaying the Wardrobe Tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorWardrobeToolView
	: public SMetaHumanCharacterEditorToolView
	, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorWardrobeToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorWardrobeTool* InTool);

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
	/** Creates the section widget for showing the Asset Views panel. */
	TSharedRef<SWidget> CreateWardrobeToolViewAssetViewsPanelSection();

	/** Creates the section widget for showing the main toolbar. */
	TSharedRef<SWidget> CreateWardrobeToolViewToolbarSection();

	/** Creates Wardrobe items for all compatible assets in the given section. */
	void CreateWardrobeItemsForCompatibleAssets(const FMetaHumanCharacterAssetsSection& Section);

	/** Registers the toolbar command actions. */
	void RegisterToolbarCommands();

	/** Gets and array of items containing the stored Wardrobe individual assets. */
	TArray<FMetaHumanCharacterAssetViewItem> GetWardrobeIndividualAssets(const FName& SlotName) const;

	/** Gets the sections array for the wardrobe asset views panel. */
	TArray<FMetaHumanCharacterAssetsSection> GetWardrobeAssetViewsSections(const UMetaHumanCharacter* InCharacter, const UMetaHumanCharacterPipelineSpecification* InSpec) const;

	/** True is the item asset is compatible to the given section. */
	bool IsItemCompatible(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item, const FMetaHumanCharacterAssetsSection& Section) const;

	/** True is the item asset is checked. */
	bool IsItemChecked(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const;

	/** True is the item asset is active. */
	bool IsItemActive(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const;

	/** Called to override the given slot name. */
	FName OnOverrideSlotName(const FName& SlotName);

	/** Called to override the name of the thumbnail */
	FText OnOverrideItemThumbnailName(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const;

	/** Called to override the item thumbnail brush. */
	void OnOverrideItemThumbnailBrush(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const;

	/** Called to process a dropped item in the asset views panel. */
	UObject* OnProcessDroppedItem(const FAssetData& AssetData);

	/** Called to process an array of dropped folders in the asset views panel. */
	void OnProcessDroppedFolders(const TArray<FContentBrowserItem> Items, const FMetaHumanCharacterAssetsSection& InSection) const;

	/** Called when to populate asset views with items. */
	TArray<FMetaHumanCharacterAssetViewItem> OnPopulateAssetViewsItems(const FMetaHumanCharacterAssetsSection& InSection, const FMetaHumanObserverChanges& InChanges);

	/** Called when the given item has been activated. */
	void OnWardrobeToolItemActivated(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

	/** Called when the given item has been deleted. */
	void OnWardrobeToolVirtualItemDeleted(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

	/** True if the given item can be deleted. */
	bool CanDeleteWardrobeToolVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const;

	/** Called when the folder has been deleted. */
	void OnWardrobePathsFolderDeleted(const FMetaHumanCharacterAssetsSection& InSection);

	/** True if the given folder can be deleted. */
	bool CanDeleteWardrobePathsFolder(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item, const FMetaHumanCharacterAssetsSection& InSection) const;

	/** Called when the given item has been moved in a virtual folder. */
	void OnHandleWardrobeVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

	enum class EWearRequest
	{
		Wear,
		Unwear,
		Toggle
	};

	/** Tries to apply wear state to the given items. Returns true if collection was updated, false otherwise */
	bool ApplyWearRequest(const TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>>& Items, EWearRequest WearRequest);

	/** Builds and assembles the collection */
	void BuildCollection();

	/** Prepares the asset in the given asset item, if valid. */
	void PrepareAsset(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

	/** Unprepares the asset in the given asset item, if valid. */
	void UnprepareAsset(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

	/** Called when the wardrobe paths array in the project settings has changed. */
	void OnWardrobePathsChanged();

	/** Called when an accessory is prepared. */
	void OnPrepareAccessory();

	/** Called when an accessory is unprepared. */
	void OnUnprepareAccessory();

	/** Called to wear an accessory. */
	void OnWearAccessory();

	/** Called when an accessory is removed. */
	void OnRemoveAccessory();

	/** Called to open accessory properties. */
	void OnOpenAccessoryProperties();

	/** Reference to this view command list. */
	TSharedPtr<FUICommandList> CommandList;

	/** Reference to this Asset Views panel. */
	TSharedPtr<SMetaHumanCharacterEditorAssetViewsPanel> AssetViewsPanel;
};
