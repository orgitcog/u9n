// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanAssetReportView.h"

#include "MetaHumanAssetReport.h"
#include "MetaHumanStyleSet.h"

#include "Components/HorizontalBox.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "Misc/FileHelper.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "MetaHumanAssetReportView"

namespace UE::MetaHuman
{
class SReportItemRow;

class SReportDataItem
{
public:
	FText Name;
	TArray<TSharedPtr<SReportDataItem>> Children;
	const FSlateBrush* Icon = nullptr;
	FSlateColor Color = FLinearColor::White;
	const UObject* ItemTarget = nullptr;

	static void GetChildren(TSharedPtr<SReportDataItem> Item, TArray<TSharedPtr<SReportDataItem>>& ToPopulate)
	{
		ToPopulate = Item->Children;
	}

	static TSharedRef<ITableRow> GenerateRow(TSharedPtr<SReportDataItem> Item, const TSharedRef<STableViewBase>& Owner);
};

class SReportItemRow : public STableRow<TSharedPtr<SReportDataItem>>
{
public:
	SLATE_BEGIN_ARGS(SReportItemRow)
		{
		}
		SLATE_ARGUMENT(TSharedPtr<SReportDataItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		Item = InArgs._Item;

		// Construct parent class
		ConstructInternal(
			STableRow::FArguments().Style(&FMetaHumanStyleSet::Get().GetWidgetStyle<FTableRowStyle>("MetaHumanManager.TreeViewItemStyle")),
			OwnerTableView
		);

		// Populate widget
		ChildSlot
			.Padding(FMetaHumanStyleSet::Get().GetMargin(Item->Children.Num() ? "ReportView.SectionPadding" : "ReportView.EntryPadding"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				.Padding(FMetaHumanStyleSet::Get().GetMargin("MetaHumanManager.IconMargin"))
				.AutoWidth()
				[
					SNew(SImage)
					.Image(this, &SReportItemRow::GetIconForRow)
					.ColorAndOpacity(Item->Color)
				]
				+ SHorizontalBox::Slot()
				.FillContentWidth(1)
				[
					SNew(STextBlock)
					.Text(Item->Name)
					.Font(FMetaHumanStyleSet::Get().GetFontStyle("ReportView.EntryFont"))
					.AutoWrapText(true)
				]
			];
	}

	const FSlateBrush* GetIconForRow() const
	{
		if (Item->Children.Num())
		{
			return IsItemExpanded() ? FAppStyle::Get().GetBrush("TreeArrow_Expanded") : FAppStyle::Get().GetBrush("TreeArrow_Collapsed");
		}

		return Item->Icon;
	}

	TSharedPtr<SReportDataItem> Item;
};

TSharedRef<ITableRow> SReportDataItem::GenerateRow(TSharedPtr<SReportDataItem> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SReportItemRow, Owner)
		.Item(Item);
}

void SMetaHumanAssetReportView::Construct(const FArguments& InArgs)
{
	ReportType = InArgs._ReportType;
	ChildSlot
	[
		SAssignNew(ReportItemsTreeView, STreeView<TSharedPtr<SReportDataItem>>)
		.TreeViewStyle(&FMetaHumanStyleSet::Get().GetWidgetStyle<FTableViewStyle>("MetaHumanManager.TreeViewStyle"))
		.TreeItemsSource(&ReportData)
		.OnGetChildren_Static(SReportDataItem::GetChildren)
		.OnGenerateRow_Static(SReportDataItem::GenerateRow)
		.OnMouseButtonClick(this, &SMetaHumanAssetReportView::OnClickItem)
		.HeaderRow(SNew(SHeaderRow)
			.Style(&FMetaHumanStyleSet::Get().GetWidgetStyle<FHeaderRowStyle>("MetaHumanManager.TreeHeaderRowStyle"))
			+ SHeaderRow::Column("")
			.HeaderContentPadding(FMetaHumanStyleSet::Get().GetMargin("ReportView.HeaderPadding"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMetaHumanStyleSet::Get().GetMargin("MetaHumanManager.IconMargin"))
				.AutoWidth()
				[
					SNew(SImage)
					.Image(this, &SMetaHumanAssetReportView::GetIconForHeader)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.FillContentWidth(1)
				[
					SNew(STextBlock)
					.Font(FMetaHumanStyleSet::Get().GetFontStyle("ReportView.HeaderFont"))
					.Text(this, &SMetaHumanAssetReportView::GetTextForHeader)
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility(this, &SMetaHumanAssetReportView::GetSaveButtonVisibility)
					.OnClicked(this, &SMetaHumanAssetReportView::OnSaveButtonClicked)
					.Text(LOCTEXT("SaveButtonText", "Save Report..."))
				]
			]
		)
	];
}

void SMetaHumanAssetReportView::OnClickItem(TSharedPtr<SReportDataItem> Item) const
{
	if (Item->Children.Num())
	{
		ReportItemsTreeView->SetItemExpansion(Item, !ReportItemsTreeView->IsItemExpanded(Item));
	}
	else if (Item->ItemTarget)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		// Temporarily cast away const as SyncBrowserToAssets does not take const UObjects
		ContentBrowserModule.Get().SyncBrowserToAssets(TArray{const_cast<UObject*>(Item->ItemTarget)});
	}
}

EVisibility SMetaHumanAssetReportView::GetSaveButtonVisibility() const
{
	return Report.IsValid() ? EVisibility::Visible : EVisibility::Hidden;
}

FReply SMetaHumanAssetReportView::OnSaveButtonClicked()
{
	TArray<FString> SelectedFilenames;
	if (FDesktopPlatformModule::Get()->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
		TEXT("Save as html file..."),
		TEXT(""),
		TEXT(""),
		TEXT("HTML file (*.html)|*.html"),
		EFileDialogFlags::None,
		SelectedFilenames))
	{
		if (!SelectedFilenames.IsEmpty())
		{
			FFileHelper::SaveStringToFile(Report->GenerateHtmlReport(), *SelectedFilenames[0]);
		}
	}

	return FReply::Handled();
}

const FSlateBrush* SMetaHumanAssetReportView::GetIconForHeader() const
{
	if (Report.IsValid())
	{
		if (Report->GetReportResult() == EMetaHumanOperationResult::Failure)
		{
			return FMetaHumanStyleSet::Get().GetBrush("ReportView.ErrorIcon");
		}
		if (Report->HasWarnings())
		{
			return FMetaHumanStyleSet::Get().GetBrush("ReportView.WarningIcon");
		}
		return FMetaHumanStyleSet::Get().GetBrush("ReportView.SuccessIcon");
	}
	return FMetaHumanStyleSet::Get().GetBrush("ReportView.NoReportIcon");
}

FText SMetaHumanAssetReportView::GetTextForHeader() const
{
	if (Report.IsValid())
	{
		if (Report->GetReportResult() == EMetaHumanOperationResult::Success)
		{
			if (Report->HasWarnings())
			{
				if (ReportType == EReportType::Verification)
				{
					return FText::Format(LOCTEXT("ReportTitleWarnings", "Asset has been verified with {0} {0}|plural(one=warning,other=warnings)"), Report->Warnings.Num());
				}
				else
				{
					return FText::Format(LOCTEXT("ReportTitleImportWarnings", "Asset has been imported with {0} {0}|plural(one=warning,other=warnings)"), Report->Warnings.Num());
				}
			}
			if (Report->Infos.Num())
			{
				if (ReportType == EReportType::Verification)
				{
					return FText::Format(LOCTEXT("ReportTitleInfo", "Asset has been verified with {0} {0}|plural(one=message,other=messages)"), Report->Warnings.Num());
				}
				else
				{
					return FText::Format(LOCTEXT("ReportTitleImportInfo", "Asset has been imported with {0} {0}|plural(one=message,other=messages)"), Report->Warnings.Num());
				}
			}
			if (ReportType == EReportType::Verification)
			{
				return LOCTEXT("ReportTitleClean", "Asset has been verified with no messages");
			}
			else
			{
				return LOCTEXT("ReportTitleImportClean", "Asset has been imported with no messages");
			}
		}
		if (ReportType == EReportType::Verification)
		{
			return FText::Format(LOCTEXT("ReportTitleErrors", "Asset has failed verification with {0} {0}|plural(one=error,other=errors)"), Report->Errors.Num());
		}
		else
		{
			return FText::Format(LOCTEXT("ReportTitleImportErrors", "Asset has not been imported due to {0} {0}|plural(one=error,other=errors)"), Report->Errors.Num());
		}
	}

	return LOCTEXT("VerificationTileNoVerification", "Asset has not been verified");
}

TSharedPtr<SReportDataItem> CreateSection(FText Title, const TArray<FMetaHumanAssetReportItem>& Source, FSlateColor Color)
{
	TArray<TSharedPtr<SReportDataItem>> Items;
	for (const FMetaHumanAssetReportItem& Item : Source)
	{
		Items.Add(MakeShared<SReportDataItem>(
			Item.Message,
			TArray<TSharedPtr<SReportDataItem>>{},
			FMetaHumanStyleSet::Get().GetBrush("ReportView.BulletIcon"),
			Color,
			Item.ProjectItem));
	}
	return MakeShared<SReportDataItem>(Title, Items);
}

void SMetaHumanAssetReportView::SetReport(UMetaHumanAssetReport* ReportToDisplay)
{
	Report.Reset(ReportToDisplay);
	ReportData.Reset();
	if (Report.IsValid())
	{
		if (Report->Errors.Num())
		{
			ReportData.Add(CreateSection(LOCTEXT("ErrorsSectionTitle", "Errors"), Report->Errors, FStyleColors::Error));
		}
		if (Report->Warnings.Num())
		{
			ReportData.Add(CreateSection(LOCTEXT("WarningsSectionTitle", "Warnings"), Report->Warnings, FStyleColors::Warning));
		}
		if (Report->Infos.Num())
		{
			ReportData.Add(CreateSection(LOCTEXT("InfoSectionTitle", "Info"), Report->Infos, FStyleColors::AccentBlue));
		}
	}
	ReportItemsTreeView->RebuildList();

	if (Report.IsValid() && Report->Errors.Num())
	{
		ReportItemsTreeView->SetItemExpansion(ReportItemsTreeView->GetRootItems()[0], true);
	}
}
}

#undef LOCTEXT_NAMESPACE
