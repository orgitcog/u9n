// Copyright Epic Games, Inc. All Rights Reserved.

#include "SImportSummary.h"

#include "MetaHumanAssetReport.h"
#include "MetaHumanStyleSet.h"
#include "SMetaHumanAssetReportView.h"
#include "Verification/MetaHumanCharacterVerification.h"

#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "Engine/Blueprint.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "ImportSummary"

namespace UE::MetaHuman
{
class SImportItemView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SImportItemView)
		{
		}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FMetaHumanStyleSet::Get().GetBrush("MetaHumanManager.RoundedBorder"))
			.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.Padding"))
			[
				SNew(SVerticalBox)
				// Asset Title
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.DetailsSectionMargin"))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.TitleTextMargin"))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.TitleIconMargin"))
						[
							SNew(SImage)
							.Image(this, &SImportItemView::GetItemAssetTypeIcon)
						]
						+ SHorizontalBox::Slot()
						.FillContentWidth(1)
						[
							SNew(STextBlock)
							.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.TitleFont"))
							.Text(this, &SImportItemView::GetItemName)
							.ColorAndOpacity(FStyleColors::White)
						]
					]
					+ SVerticalBox::Slot()
					.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.TitleTextMargin"))
					[
						SNew(STextBlock)
						.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.DetailsTextFont"))
						.Text(this, &SImportItemView::GetItemAssetType)
					]
				]
				+ SVerticalBox::Slot()
				.FillContentHeight(1)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(ReportView, SMetaHumanAssetReportView)
						.ReportType(SMetaHumanAssetReportView::EReportType::Import)
					]
				]
			]
		];
	}

	void SetImportResult(TSharedPtr<FImportResult> Item)
	{
		if (Item)
		{
			CurrentItem = Item;
			ReportView->SetReport(Item->Report.Get());
		}
	}

	FText GetItemName() const
	{
		if (CurrentItem.IsValid() && CurrentItem->Target.IsValid())
		{
			return FText::FromName(CurrentItem->Target->GetFName());
		}
		return LOCTEXT("NoNameAvailable", "None");
	}

	FText GetItemAssetType() const
	{
		if (CurrentItem.IsValid() && CurrentItem->Target.IsValid())
		{
			if (Cast<UGroomBindingAsset>(CurrentItem->Target.Get()))
			{
				return LOCTEXT("GroomAssetType", "Groom");
			}
			if (Cast<USkeletalMesh>(CurrentItem->Target.Get()))
			{
				return LOCTEXT("ClothingAssetType", "Clothing");
			}
			if (Cast<UBlueprint>(CurrentItem->Target.Get()))
			{
				return LOCTEXT("MetaHumanAssetType", "MetaHuman");
			}
			if (FMetaHumanCharacterVerification::Get().IsCharacterAsset(CurrentItem->Target.Get()))
			{
				return LOCTEXT("MetaHumanAssetType", "MetaHuman");
			}
			if (FMetaHumanCharacterVerification::Get().IsOutfitAsset(CurrentItem->Target.Get()))
			{
				return LOCTEXT("ClothingAssetType", "Clothing");
			}
		}
		return LOCTEXT("UnknownAssetType", "Unknown");
	}

	const FSlateBrush* GetItemAssetTypeIcon() const
	{
		if (CurrentItem.IsValid() && CurrentItem->Target.IsValid())
		{
			if (Cast<UGroomBindingAsset>(CurrentItem->Target.Get()))
			{
				return FMetaHumanStyleSet::Get().GetBrush("ItemDetails.GroomIcon");
			}
			if (Cast<USkeletalMesh>(CurrentItem->Target.Get()))
			{
				return FMetaHumanStyleSet::Get().GetBrush("ItemDetails.ClothingIcon");
			}
			if (Cast<UBlueprint>(CurrentItem->Target.Get()))
			{
				return FMetaHumanStyleSet::Get().GetBrush("ItemDetails.CharacterIcon");
			}
			if (FMetaHumanCharacterVerification::Get().IsCharacterAsset(CurrentItem->Target.Get()))
			{
				return FMetaHumanStyleSet::Get().GetBrush("ItemDetails.CharacterIcon");
			}
			if (FMetaHumanCharacterVerification::Get().IsOutfitAsset(CurrentItem->Target.Get()))
			{
				return FMetaHumanStyleSet::Get().GetBrush("ItemDetails.ClothingIcon");
			}
		}
		return FMetaHumanStyleSet::Get().GetBrush("ItemDetails.DefaultIcon");
	}

private:
	TSharedPtr<SMetaHumanAssetReportView> ReportView;
	TSharedPtr<FImportResult> CurrentItem;
};

class SImportItemEntry : public STableRow<TSharedPtr<FImportResult>>
{
public:
	SLATE_BEGIN_ARGS(SImportItemEntry)
		{
		}
		SLATE_ARGUMENT(TSharedPtr<FImportResult>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		RowData = Args._Item;

		STableRow::Construct(
			STableRow::FArguments()
			.Content()
			[
				SNew(SBox)
				.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemNavigation.ListItemMargin"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(FMetaHumanStyleSet::Get().GetMargin("MetaHumanManager.IconMargin"))
					.AutoWidth()
					[
						SNew(SImage)
						.Image(this, &SImportItemEntry::GetIconForReport)
					]
					+ SHorizontalBox::Slot()
					.FillContentWidth(1)
					[
						SNew(STextBlock)
						.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.ListItemFont"))
						.Text(this, &SImportItemEntry::GetTextForItem)
					]
				]
			],
			OwnerTableView
		);
	}

	const FSlateBrush* GetIconForReport() const
	{
		if (RowData.IsValid() && RowData->Report.IsValid())
		{
			if (RowData->Report->GetReportResult() == EMetaHumanOperationResult::Failure)
			{
				return FMetaHumanStyleSet::Get().GetBrush("ReportView.ErrorIcon");
			}
			if (RowData->Report->HasWarnings())
			{
				return FMetaHumanStyleSet::Get().GetBrush("ReportView.WarningIcon");
			}
			return FMetaHumanStyleSet::Get().GetBrush("ReportView.SuccessIcon");
		}
		return FMetaHumanStyleSet::Get().GetBrush("ReportView.NoReportIcon");
	}

	FText GetTextForItem() const
	{
		if (RowData.IsValid() && RowData->Target.IsValid())
		{
			return FText::FromString(RowData->Target->GetName());
		}
		return LOCTEXT("ImportFailedEntry", "Import Failed");
	}

private:
	TSharedPtr<FImportResult> RowData = nullptr;
};

DECLARE_DELEGATE_OneParam(FOnNavigateImportedItem, TSharedPtr<FImportResult>);

class SImportedItemsList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SImportedItemsList)
		{
		}
		SLATE_EVENT(FOnNavigateImportedItem, OnNavigate)
		SLATE_ARGUMENT(TArray<TSharedPtr<FImportResult>>, Items)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		NavigateCallback = InArgs._OnNavigate;
		Items = InArgs._Items;
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ExpandableArea.Border"))
			.BorderBackgroundColor(FLinearColor::White)
			.Padding(FMetaHumanStyleSet::Get().GetFloat("ItemNavigation.BorderPadding"))
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("ImportedAssetsTitle", "Imported Assets"))
				.AreaTitleFont(FMetaHumanStyleSet::Get().GetFontStyle("ItemNavigation.HeaderFont"))
				.HeaderPadding(FMetaHumanStyleSet::Get().GetFloat("ItemNavigation.HeaderPadding"))
				.InitiallyCollapsed(false)
				.Padding(0)
				.BodyContent()
				[
					SNew(SListView<TSharedPtr<FImportResult>>)
					.ListItemsSource(&Items)
					.OnGenerateRow(this, &SImportedItemsList::OnGenerateWidgetForItem)
					.OnSelectionChanged(this, &SImportedItemsList::OnSelectionChanged)
				]
			]
		];
	}

	void OnSelectionChanged(TSharedPtr<FImportResult> SelectedItem, ESelectInfo::Type Type) const
	{
		NavigateCallback.ExecuteIfBound(SelectedItem);
	}

	TSharedRef<ITableRow> OnGenerateWidgetForItem(TSharedPtr<FImportResult> Item, const TSharedRef<STableViewBase>& Owner)
	{
		return SNew(SImportItemEntry, Owner)
			.Item(Item);
	}

private:
	TArray<TSharedPtr<FImportResult>> Items;
	FOnNavigateImportedItem NavigateCallback;
};

void SImportSummary::Construct(const FArguments& InArgs)
{
	ImportResults = InArgs._ImportResults;

	SWindow::Construct(
		SWindow::FArguments()
		.Title(LOCTEXT("ImportSummaryTitle", "Import Summary"))
		.SupportsMinimize(true)
		.SupportsMaximize(true)
		.ClientSize(FMetaHumanStyleSet::Get().GetVector("MetaHumanManager.WindowSize"))
		.MinWidth(FMetaHumanStyleSet::Get().GetFloat("MetaHumanManager.WindowMinWidth"))
		.MinHeight(FMetaHumanStyleSet::Get().GetFloat("MetaHumanManager.WindowMinHeight"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.MinWidth(FMetaHumanStyleSet::Get().GetFloat("MetaHumanManager.NavigationWidth"))
			.MaxWidth(FMetaHumanStyleSet::Get().GetFloat("MetaHumanManager.NavigationWidth"))
			.FillContentWidth(0)
			.VAlign(VAlign_Fill)
			[
				SNew(SImportedItemsList)
				.Items(ImportResults)
				.OnNavigate(this, &SImportSummary::ChangeSelection)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.FillContentWidth(1)
			.VAlign(VAlign_Fill)
			.Padding(FMetaHumanStyleSet::Get().GetMargin("MetaHumanManager.ItemViewPadding"))
			[
				SAssignNew(ItemView, SImportItemView)
			]
		]);

	ItemView->SetImportResult(ImportResults[0]);
}

void SImportSummary::ChangeSelection(TSharedPtr<FImportResult> Item)
{
	ItemView->SetImportResult(Item);
}
}

#undef LOCTEXT_NAMESPACE
