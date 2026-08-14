// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/Widgets/SMetaHumanCharacterEditorAssetViewsPanel.h"
#include "Widgets/SCompoundWidget.h"

struct FAssetData;
class FAssetThumbnail;
class FAssetThumbnailPool;
struct FContentBrowserItem;
struct FMetaHumanCharacterAssetsSection;
struct FMetaHumanCharacterAssetViewItem;
struct FMetaHumanObserverChanges;
struct FSlateBrush;
class SBox;
class UMetaHumanCharacter;

DECLARE_DELEGATE_ThreeParams(FMetaHumanCharacterOnItemDropped, const FGeometry&, const FDragDropEvent&, int32);
DECLARE_DELEGATE_OneParam(FMetaHumanCharacterOnItemDeleted, int32);
DECLARE_DELEGATE_RetVal_OneParam(bool, FMetaHumanCharacterFilterAssetData, const FAssetData&);

/** Widget used to display asset thumbnails for the Blend Tool. */
class SMetaHumanCharacterEditorBlendToolThumbnail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorBlendToolThumbnail)
		{}

		/** Called when an item is dropped in this widget. */
		SLATE_EVENT(FMetaHumanCharacterOnItemDropped, OnItemDropped)
		
		/** Called when an item is deleted */
		SLATE_EVENT(FMetaHumanCharacterOnItemDeleted, OnItemDeleted)

	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs, int32 InItemIndex);

	/** Gets the asset data displayed by this widget. */
	FAssetData GetThumbnailAssetData() const;

protected:
	//~ Begin SWidget interface
	virtual void OnDragEnter(const FGeometry & MyGeometry, const FDragDropEvent & DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	//~ End SWidget interface

private:
	/** Generates the thumbnail widget for this item. */
	TSharedRef<SWidget> GenerateThumbnailWidget(TSharedPtr<FMetaHumanCharacterAssetViewItem> AssetItem);

	/** Called when the delete button is clicked. */
	FReply OnDeleteButtonClicked();

	/** Gets the visibility of the delete button. */
	EVisibility GetDeleteButtonVisibility() const;

	/** Gets the border brush of this widget. */
	const FSlateBrush* GetBorderBrush() const;

	/** Gets the thumbnail name label as a text. */
	FText GetThumbnailNameAsText() const;

	/** The delegate to execute when an item is dropped in this widget. */
	FMetaHumanCharacterOnItemDropped OnItemDroppedDelegate;

	/** The delegate to execute when an item is deleted in this widget. */
	FMetaHumanCharacterOnItemDeleted OnItemDeletedDelegate;

	/** The default brush used by this widget. */
	const FSlateBrush* DefaultBrush;

	/** The brush used when this widget is selected. */
	const FSlateBrush* SelectedBrush;

	/** True if a drag and drop operation is on. */
	bool bIsDragging = false;

	/** Reference to the thumbnail pool used by this widget thumbnail. */
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	/** Reference to the asset thumbnail displayed by this widget. */
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	/** Reference to the thumbnail container box widget. */
	TSharedPtr<SBox> ThumbnailContainerBox;

	/** Index of the thumbnail within the thumbnail panel */
	int32 ItemIndex;
};

/** Widget used to display the Blend Tool and its properties widgets in the MetaHumanCharacter editor. */
class SMetaHumanCharacterEditorBlendToolPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorBlendToolPanel)
		: _VirtualFolderSlotName(NAME_None)
		{}

		/** Name identifier used for handling operations with virtual assets. */
		SLATE_ARGUMENT(FName, VirtualFolderSlotName)

		/** Called when an item is dropped in one of this panel's thumbnails. */
		SLATE_EVENT(FMetaHumanCharacterOnItemDropped, OnItemDropped)

		/** Called when panel's thumbnails is deleted. */
		SLATE_EVENT(FMetaHumanCharacterOnItemDeleted, OnItemDeleted)

		/** Called when an items is double-clicked */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnItemActivated)

		/** Called to override the item thumbnail brush. */
		SLATE_EVENT(FMetaHumanCharacterEditorAssetViewItemDelegate, OnOverrideItemThumbnail)

		/* Called to determine if asset data should be added asset views. */
		SLATE_EVENT(FMetaHumanCharacterFilterAssetData, OnFilterAssetData)

	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs, UMetaHumanCharacter* InCharacter);
	
	/** Gets the array of current blendable items displayed by this panel. */
	TArray<FAssetData> GetBlendableItems() const;

	/** The delegate to execute when an item is double clicked in this widget. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnItemActivatedDelegate;

	/** The delegate to execute to override the item thumbnail brush. */
	FMetaHumanCharacterEditorAssetViewItemDelegate OnOverrideItemThumbnailDelegate;

	FMetaHumanCharacterFilterAssetData OnFilterAssetDataDelegate;

private:
	/** Gets and array of items containing the stored Character individual assets. */
	TArray<FMetaHumanCharacterAssetViewItem> GetCharacterIndividualAssets() const;

	/** Gets the sections array for the wardrobe asset views panel. */
	TArray<FMetaHumanCharacterAssetsSection> GetAssetViewsSections() const;

	/** Called when to populate asset views with items. */
	TArray<FMetaHumanCharacterAssetViewItem> OnPopulateAssetViewsItems(const FMetaHumanCharacterAssetsSection& InSection, const FMetaHumanObserverChanges& InChanges);

	/** Called to process an array of dropped folders in the asset views panel. */
	void OnProcessDroppedFolders(const TArray<FContentBrowserItem> Items, const FMetaHumanCharacterAssetsSection& InSection) const;

	/** Called when the given item has been deleted. */
	void OnBlendToolVirtualItemDeleted(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

	/** True if the given item can be deleted. */
	bool CanDeleteBlendToolVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const;

	/** Called when the folder has been deleted. */
	void OnPresetsPathsFolderDeleted(const FMetaHumanCharacterAssetsSection& InSection);

	/** True if the given folder can be deleted. */
	bool CanDeletePresetsPathsFolder(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item, const FMetaHumanCharacterAssetsSection& InSection) const;

	/** Called when the given item has been moved in a virtual folder. */
	void OnHandleBlendVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

	/** Called when the Project Settings selected directory paths have been changed. */
	void OnPresetsDirectoriesChanged();

	/** The array of available thumbnails in this panel. */
	TArray<TSharedPtr<SMetaHumanCharacterEditorBlendToolThumbnail>> BlendToolThumbnails;

	/** Reference to this Asset Views panel. */
	TSharedPtr<SMetaHumanCharacterEditorAssetViewsPanel> AssetViewsPanel;

	/** Reference to the Character this panel is based on. */
	TWeakObjectPtr<UMetaHumanCharacter> CharacterWeakPtr;

	/** Slate arguments. */
	FName VirtualFolderSlotName;
};
