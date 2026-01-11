// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MetaHumanCharacterEditorStyle.h"
#include "Widgets/Views/STileView.h"

struct FSlateBrush;

/*
* This class is used as a custom TileView for MetaHuman Character purposes
* Requirements for using it are having an Enum for options which the TileView will represent
* and TileView needs to be inside of a DetailsView so that PropertyHandle can be passed
*/
template<typename TEnum>
class SMetaHumanCharacterEditorTileView : public SCompoundWidget
{
public:
	using FEnumType = std::underlying_type_t<TEnum>;

	DECLARE_DELEGATE_RetVal_OneParam(const FSlateBrush*, FOnGetSlateBrush, uint8)
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, uint8)

	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorTileView<TEnum>) 
		{}

		/** The initially selected item of the Tile View. */
		SLATE_ATTRIBUTE(TEnum, InitiallySelectedItem)

		/** The array of items excluded from the Tile View. */
		SLATE_ARGUMENT(TArray<TEnum>, ExcludedItems)

		/** Called to get the brush of an item in the Tile View. */
		SLATE_EVENT(FOnGetSlateBrush, OnGetSlateBrush)

		/** Called when the selection of the Tile View has changed. */
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)

	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const typename SMetaHumanCharacterEditorTileView<TEnum>::FArguments& InArgs)
	{
		OnGetSlateBrush = InArgs._OnGetSlateBrush;
		OnSelectionChanged = InArgs._OnSelectionChanged;

		check(OnGetSlateBrush.IsBound());
		check(OnSelectionChanged.IsBound());

		ExcludedItems = InArgs._ExcludedItems;

		const TEnum InitiallySelectedItem = InArgs._InitiallySelectedItem.IsSet() ? InArgs._InitiallySelectedItem.Get() : static_cast<TEnum>(0);

		TSharedPtr<TEnum> InitialItem = nullptr;
		TileViewItems.Empty();

		// Initialize the possible options
		for (TEnum Option : TEnumRange<TEnum>())
		{
			if (ExcludedItems.Contains(Option))
			{
				continue;
			}

			TSharedPtr<TEnum> Item = TileViewItems.Add_GetRef(MakeShared<TEnum>(Option));
			if (Option == InitiallySelectedItem)
			{
				InitialItem = Item;
			}
		}

		ChildSlot
			[
				SAssignNew(TileView, STileView<TSharedPtr<TEnum>>)
				.ListItemsSource(&TileViewItems)
				.SelectionMode(ESelectionMode::Single)
				.ItemAlignment(EListItemAlignment::EvenlyDistributed)
				.ClearSelectionOnClick(false)
				.OnGenerateTile(this, &SMetaHumanCharacterEditorTileView::OnGenerateTile)
				.OnSelectionChanged(this, &SMetaHumanCharacterEditorTileView::OnTileSelectionChanged)
				.IsEnabled(InArgs._IsEnabled)
			];

		// Set the initial selection based on the current value of the property
		if (InitialItem.IsValid())
		{
			TileView->SetSelection(InitialItem);
		}
	}

	// Sets custom item list
	void SetItemsSource(const TArray<TEnum>& InEnumItems, TEnum SelectedItem)
	{
		TSharedPtr<TEnum> InitialItem = nullptr;
		TileViewItems.Empty();

		for (TEnum Option : InEnumItems)
		{
			if (ExcludedItems.Contains(Option))
			{
				continue;
			}

			TSharedPtr<TEnum> Item = TileViewItems.Add_GetRef(MakeShared<TEnum>(Option));
			if (Option == SelectedItem)
			{
				InitialItem = Item;
			}
		}

		TileView->RequestListRefresh();
		if (InitialItem.IsValid())
		{
			TileView->SetSelection(InitialItem);
		}
	}

private:
	/** Called to generate the Tile View's tile widgets. */
	TSharedRef<ITableRow> OnGenerateTile(TSharedPtr<TEnum>InItem, const TSharedRef<class STableViewBase>& InOwnerTable) const
	{
		check(InItem.IsValid());

		const FSlateBrush* Brush = OnGetSlateBrush.Execute(static_cast<uint8>(*InItem));

		return 
			SNew(STableRow<TSharedPtr<TEnum>>, InOwnerTable)
			.Padding(4.0f)
			.Style(FMetaHumanCharacterEditorStyle::Get(), "MetaHumanCharacterEditorTools.TableViewRow")
			.Content()
			[
				SNew(SImage)
				.Image(Brush)
			];
	}

	/** Called when the selection of the Tile View has changed. */
	void OnTileSelectionChanged(TSharedPtr<TEnum> InItem, ESelectInfo::Type InSelectInfo)
	{
		if (InItem.IsValid() && InSelectInfo != ESelectInfo::Direct)
		{
			// ESelectInfo::Direct means the selection was changed in code, via a call to SetSelection. In this particular
			// instance SetSelection only used to set the initial selection of the widget since STileView has the concept
			// of selected item, so we only pass the value down to the property handle if the selection happen as the
			// result of a user interaction
			const uint8 Item = static_cast<FEnumType>(*InItem);
			OnSelectionChanged.ExecuteIfBound(Item);
		}
	}

	/** The delegate to execute when the selection of the Tile View has changed. */
	FOnSelectionChanged OnSelectionChanged;

	/** The delegate to execute to get the brush to display for a specific Tile View item. */
	FOnGetSlateBrush OnGetSlateBrush;

	/** The array of Tile View items. */
	TArray<TSharedPtr<TEnum>> TileViewItems;

	/** The array of items that should be exclueded from the Tile View. */
	TArray<TEnum> ExcludedItems;
	
	/** Reference to the Tile View widget. */
	TSharedPtr<STileView<TSharedPtr<TEnum>>> TileView;
};
