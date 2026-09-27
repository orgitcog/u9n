// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterPaletteItem.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STileView.h"

class FAssetThumbnail;
class FAssetThumbnailPool;
struct FMetaHumanPaletteItemKey;
struct FPartsViewTileData;
class UMetaHumanCollection;

/**
 * A Content Browser-like widget for displaying a collection of assets representing character parts.
 * 
 * Accepts assets dragged and dropped into it from a Content Browser.
 */
class METAHUMANCHARACTERPALETTEEDITOR_API SCharacterPartsView : public SCompoundWidget
{
public:
	typedef typename TSlateDelegates<TSharedPtr<FMetaHumanCharacterPaletteItem>>::FOnSelectionChanged FOnSelectionChanged;
	typedef typename TSlateDelegates<TSharedPtr<FMetaHumanCharacterPaletteItem>>::FOnMouseButtonDoubleClick FOnMouseButtonDoubleClick;
	typedef FSimpleDelegate FOnPaletteModified;

	SLATE_BEGIN_ARGS(SCharacterPartsView) {}
		SLATE_ARGUMENT(UMetaHumanCollection*, CharacterPalette)
		/** True if this asset is allowed to edit the palette, otherwise it will only view the palette's contents */
		SLATE_ARGUMENT(bool, IsPaletteEditable)
		/** Optional. If specified, this widget will only edit the given slot, otherwise it will show the contents of all slots. */
		SLATE_ARGUMENT(FName, PipelineSlotName)
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
		SLATE_EVENT(FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick)
		SLATE_EVENT(FOnPaletteModified, OnPaletteModified)
	SLATE_END_ARGS()

	SCharacterPartsView();

	void Construct(const FArguments& Args);

	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** Write the edited item back to the Character Palette asset */
	void WriteItemToCharacterPalette(const FMetaHumanPaletteItemKey& OriginalItemKey, const TSharedRef<FMetaHumanCharacterPaletteItem>& ModifiedItem);
	
	// Begin SWidget interface
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	// End SWidget interface

private:
	void PopulateListItems();
	TSharedRef<ITableRow> OnGenerateTile(TSharedPtr<FPartsViewTileData> InItem, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnTileViewSelectionChanged(TSharedPtr<FPartsViewTileData> SelectedTile, ESelectInfo::Type SelectInfo);
	void OnTileViewDoubleClick(TSharedPtr<FPartsViewTileData> SelectedTile);
	TArray<FName, TInlineAllocator<1>> GetTargetSlotNames() const;

	TStrongObjectPtr<UMetaHumanCollection> CharacterPalette;
	FOnSelectionChanged OnSelectionChangedDelegate;
	FOnMouseButtonDoubleClick OnMouseButtonDoubleClickDelegate;
	FOnPaletteModified OnPaletteModifiedDelegate;
	bool bIsPaletteEditable = false;
	FName PipelineSlotName;

	// Mirrors the list of parts on the Palette asset
	TSharedRef<UE::Slate::Containers::TObservableArray<TSharedPtr<FPartsViewTileData>>> ListItems;
	TSharedRef<FAssetThumbnailPool> AssetThumbnailPool;

	TSharedPtr<STileView<TSharedPtr<FPartsViewTileData>>> TileView;
};

