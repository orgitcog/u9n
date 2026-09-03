// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SCharacterPartsView.h"

#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanCharacterPaletteEditorLog.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanCollectionPipeline.h"
#include "MetaHumanWardrobeItem.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetThumbnail.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Logging/StructuredLog.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterPaletteEditor"

static constexpr float TileViewItemSize = 128.0f;

struct FPartsViewTileData
{
	FPartsViewTileData(const TSharedPtr<FMetaHumanCharacterPaletteItem>& InItem)
	: Item(InItem)
	{
	}

	TSharedPtr<FMetaHumanCharacterPaletteItem> Item;
	TSharedPtr<FAssetThumbnail> Thumbnail;
};

SCharacterPartsView::SCharacterPartsView()
: ListItems(MakeShared<UE::Slate::Containers::TObservableArray<TSharedPtr<FPartsViewTileData>>>())
// TODO: Find out what happens if this is too small
, AssetThumbnailPool(MakeShared<FAssetThumbnailPool>(128))
{
}

void SCharacterPartsView::Construct(const FArguments& Args)
{
	check(Args._CharacterPalette);
	CharacterPalette = TStrongObjectPtr(Args._CharacterPalette);
	bIsPaletteEditable = Args._IsPaletteEditable;
	PipelineSlotName = Args._PipelineSlotName;
	OnSelectionChangedDelegate = Args._OnSelectionChanged;
	OnMouseButtonDoubleClickDelegate = Args._OnMouseButtonDoubleClick;
	OnPaletteModifiedDelegate = Args._OnPaletteModified;

	PopulateListItems();
		
	this->ChildSlot
	[
		SAssignNew(TileView, STileView<TSharedPtr<FPartsViewTileData>>)
		.ListItemsSource(ListItems)
		.ItemWidth(TileViewItemSize)
		.ItemHeight(TileViewItemSize)
		.OnGenerateTile(this, &SCharacterPartsView::OnGenerateTile)
		.OnSelectionChanged(this, &SCharacterPartsView::OnTileViewSelectionChanged)
		.OnMouseButtonDoubleClick(this, &SCharacterPartsView::OnTileViewDoubleClick)
	];
}

void SCharacterPartsView::PopulateListItems()
{
	ListItems->Reset(CharacterPalette->GetItems().Num() + 1);

	if (PipelineSlotName != NAME_None)
	{
		// This null item will be the "None" option if the user doesn't want to select something for this slot
		ListItems->Add(MakeShared<FPartsViewTileData>(nullptr));
	}

	for (const FMetaHumanCharacterPaletteItem& Item : CharacterPalette->GetItems())
	{
		if (PipelineSlotName == NAME_None
			|| Item.SlotName == PipelineSlotName)
		{
			ListItems->Add(MakeShared<FPartsViewTileData>(MakeShared<FMetaHumanCharacterPaletteItem>(Item)));
		}
	}
}

TSharedRef<ITableRow> SCharacterPartsView::OnGenerateTile(TSharedPtr<FPartsViewTileData> InItem, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const float PaddingSize = 2.0f;

	auto CreateTableRowFromText = [&OwnerTable, PaddingSize](const FText& Text)
	{
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
			.Content()
			[
				SNew(SBox)
				.WidthOverride(TileViewItemSize + PaddingSize)
				.HeightOverride(TileViewItemSize + PaddingSize)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Text)
				]
			];
	};

	if (!InItem.IsValid())
	{
		// This shouldn't happen. Ensures will fire in the calling code if it does.
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable);
	}

	if (!InItem->Item.IsValid())
	{
		// This is the "None" option
		return CreateTableRowFromText(LOCTEXT("NoneOptionName", "None"));
	}

	if (!InItem->Thumbnail.IsValid())
	{
		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		check(AssetRegistry);

		if (InItem->Item->WardrobeItem)
		{
			const TSoftObjectPtr<UObject> PrincipalAsset = InItem->Item->WardrobeItem->PrincipalAsset;

			FAssetData PrincipalAssetData;
			if (AssetRegistry->TryGetAssetByObjectPath(PrincipalAsset.ToSoftObjectPath(), PrincipalAssetData) == UE::AssetRegistry::EExists::Exists)
			{
				InItem->Thumbnail = MakeShared<FAssetThumbnail>(PrincipalAssetData, TileViewItemSize, TileViewItemSize, AssetThumbnailPool);
			}
		}
	}

	if (!InItem->Thumbnail.IsValid())
	{
		return CreateTableRowFromText(InItem->Item->GetOrGenerateDisplayName());
	}

	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		.Content()
		[
			SNew(SBox)
			.Padding(PaddingSize)
			[
				InItem->Thumbnail->MakeThumbnailWidget()
			]
		];
}

void SCharacterPartsView::OnTileViewSelectionChanged(TSharedPtr<FPartsViewTileData> SelectedTile, ESelectInfo::Type SelectInfo)
{
	TSharedPtr<FMetaHumanCharacterPaletteItem> SelectedItem;

	if (SelectedTile.IsValid())
	{
		SelectedItem = SelectedTile->Item;
	}

	OnSelectionChangedDelegate.ExecuteIfBound(SelectedItem, SelectInfo);
}

void SCharacterPartsView::OnTileViewDoubleClick(TSharedPtr<FPartsViewTileData> SelectedTile)
{
	TSharedPtr<FMetaHumanCharacterPaletteItem> SelectedItem;

	if (SelectedTile.IsValid())
	{
		SelectedItem = SelectedTile->Item;
	}

	OnMouseButtonDoubleClickDelegate.ExecuteIfBound(SelectedItem);
}

void SCharacterPartsView::WriteItemToCharacterPalette(const FMetaHumanPaletteItemKey& OriginalItemKey, const TSharedRef<FMetaHumanCharacterPaletteItem>& ModifiedItem)
{
	if (!bIsPaletteEditable)
	{
		return;
	}

	const int32 ItemIndex = ListItems->IndexOfByPredicate(
		[&ModifiedItem](const TSharedPtr<FPartsViewTileData>& Element)
		{
			// Note that this compares pointers rather than values
			return Element->Item == ModifiedItem;
		});

	if (ItemIndex == INDEX_NONE)
	{
		UE_LOGFMT(LogMetaHumanCharacterPaletteEditor, Warning, "WriteItemToCharacterPalette couldn't find item in local array");
		return;
	}

	if (!CharacterPalette->TryReplaceItem(OriginalItemKey, ModifiedItem.Get()))
	{
		UE_LOGFMT(LogMetaHumanCharacterPaletteEditor, Error, "Failed to update item {Key} in palette {Palette}", OriginalItemKey.ToDebugString(), CharacterPalette->GetPathName());
		return;
	}

	OnPaletteModifiedDelegate.ExecuteIfBound();
}

FReply SCharacterPartsView::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (!bIsPaletteEditable
		|| !CharacterPalette->GetEditorPipeline())
	{
		return FReply::Unhandled();
	}

	// Is this an asset drop?
	if (TSharedPtr<FAssetDragDropOp> AssetDragDrop = DragDropEvent.GetOperationAs<FAssetDragDropOp>())
	{
		const TArray<FName, TInlineAllocator<1>> TargetSlotNames = GetTargetSlotNames();

		for (const FAssetData& Asset : AssetDragDrop->GetAssets())
		{
			for (const FName& SlotName : TargetSlotNames)
			{
				const UClass* AssetClass = Asset.GetClass(EResolveClass::Yes);
				if (!AssetClass)
				{
					continue;
				}

				if (CharacterPalette->GetEditorPipeline()->IsPrincipalAssetClassCompatibleWithSlot(SlotName, AssetClass))
				{
					return FReply::Handled();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SCharacterPartsView::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (!bIsPaletteEditable
		|| !CharacterPalette->GetEditorPipeline())
	{
		return FReply::Unhandled();
	}

	TSharedPtr<FAssetDragDropOp> AssetDragDrop = DragDropEvent.GetOperationAs<FAssetDragDropOp>();

	if (!AssetDragDrop)
	{
		return FReply::Unhandled();
	}

	bool bWereAnyAssetsModified = false;
	for (const FAssetData& Asset : AssetDragDrop->GetAssets())
	{
		const TArray<FName, TInlineAllocator<1>> TargetSlotNames = GetTargetSlotNames();

		for (const FName& SlotName : TargetSlotNames)
		{
			FMetaHumanPaletteItemKey NewItemKey;
			if (CharacterPalette->TryAddItemFromPrincipalAsset(SlotName, Asset.ToSoftObjectPath(), NewItemKey))
			{
				if (!CharacterPalette->GetPipeline()->GetSpecification()->Slots[SlotName].bAllowsMultipleSelection)
				{
					CharacterPalette->GetMutableDefaultInstance()->SetSingleSlotSelection(SlotName, NewItemKey);
				}

				FMetaHumanCharacterPaletteItem NewItem;
				verify(CharacterPalette->TryFindItem(NewItemKey, NewItem));
				
				ListItems->Add(MakeShared<FPartsViewTileData>(MakeShared<FMetaHumanCharacterPaletteItem>(NewItem)));
				bWereAnyAssetsModified = true;
			}
		}
	}
		
	if (bWereAnyAssetsModified)
	{
		OnPaletteModifiedDelegate.ExecuteIfBound();
	}

	return FReply::Handled();
}

TArray<FName, TInlineAllocator<1>> SCharacterPartsView::GetTargetSlotNames() const
{
	check(CharacterPalette->GetPipeline());

	TArray<FName, TInlineAllocator<1>> SlotNames;

	if (PipelineSlotName != NAME_None)
	{
		SlotNames.Add(PipelineSlotName);
	}
	else
	{
		CharacterPalette->GetPipeline()->GetSpecification()->Slots.GetKeys(SlotNames);
	}

	return SlotNames;
}

#undef LOCTEXT_NAMESPACE
