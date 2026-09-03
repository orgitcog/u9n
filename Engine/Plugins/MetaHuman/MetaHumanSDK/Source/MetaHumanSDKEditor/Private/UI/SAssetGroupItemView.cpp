// Copyright Epic Games, Inc. All Rights Reserved.
#include "SAssetGroupItemView.h"

#include "MetaHumanAssetReport.h"
#include "ProjectUtilities/MetaHumanAssetManager.h"
#include "SMetaHumanAssetReportView.h"
#include "UI/MetaHumanStyleSet.h"

#include "AssetDefinition.h"
#include "AssetDefinitionRegistry.h"
#include "Application/SlateApplicationBase.h"
#include "AssetThumbnail.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/VerticalBox.h"
#include "HAL/FileManager.h"
#include "Styling/StyleColors.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "AssetGroupItemView"

namespace UE::MetaHuman
{
// Customized container for asset thumbnail widgets. Adds border and minimize / maximize button.
class SAssetGroupItemPreview : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SAssetGroupItemPreview)
		{
		}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_EVENT(FSimpleDelegate, OnChangeMaximized)
		SLATE_ARGUMENT(bool, IsMaximized)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		const TSharedRef<SWidget>& AssetThumbnail = InArgs._Content.Widget;
		ChildSlot
		[
			// The actual light border
			SNew(SBorder)
			.BorderImage(FMetaHumanStyleSet::Get().GetBrush("ItemDetails.ThumbnailBorder"))
			[
				// AssetWidgets don't have a background, so use another border to set the background color
				SNew(SBorder)
				.BorderImage(FMetaHumanStyleSet::Get().GetBrush("ItemDetails.ThumbnailInnerBorder"))
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						AssetThumbnail
					]
					+ SOverlay::Slot()
					[
						SNew(SBox)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Bottom)
						.Padding(FMetaHumanStyleSet::Get().GetFloat("ItemDetails.ResizeButtonMargin"))
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleRoundButton")
							.ButtonColorAndOpacity(FStyleColors::Panel)
							.ContentPadding(FMetaHumanStyleSet::Get().GetFloat("ItemDetails.ResizeButtonPadding"))
							.OnPressed(InArgs._OnChangeMaximized)
							[
								SNew(SImage)
								.Image(FMetaHumanStyleSet::Get().GetBrush(InArgs._IsMaximized ? "ItemDetails.MinimizeIcon" : "ItemDetails.MaximizeIcon"))
							]
						]
					]
				]
			]
		];
	}
};

// The display information about an asset that is part of the AssetGroup
class FAssetDetails
{
public:
	explicit FAssetDetails(const FName& PackageName)
	{
		Name = FPaths::GetBaseFilename(PackageName.ToString());

		// We want the size on disk for the asset size
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName.ToString(), FPackageName::GetAssetPackageExtension());
		Size = IFileManager::Get().FileSize(*Filename);

		TArray<FAssetData> PackagedAssets;
		IAssetRegistry::GetChecked().GetAssetsByPackageName(PackageName, PackagedAssets);
		if (PackagedAssets.Num()) // If somehow we have an empty package in the list, then it will show as type "Unknown"
		{
			if (const UAssetDefinitionRegistry* AssetDefinitionRegistry = UAssetDefinitionRegistry::Get())
			{
				if (const UAssetDefinition* AssetDefinition = AssetDefinitionRegistry->GetAssetDefinitionForAsset(PackagedAssets[0]))
				{
					Type = AssetDefinition->GetAssetDisplayName();
					TypeColor = AssetDefinition->GetAssetColor();
				}
			}
		}
	}

	FString Name;
	FText Type = LOCTEXT("UnknownType", "Unknown");
	FLinearColor TypeColor = FLinearColor::White;
	int Size = 0;
};

// Represents a row in the details table
class SAssetDetailsRow : public SMultiColumnTableRow<TSharedPtr<FAssetDetails>>
{
public:
	SLATE_BEGIN_ARGS(SAssetDetailsRow)
		{
		}
		SLATE_ARGUMENT(TSharedPtr<FAssetDetails>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		RowData = Args._Item;

		FSuperRowType::Construct(
			FSuperRowType::FArguments()
			.Padding(FMetaHumanStyleSet::Get().GetFloat("ItemDetails.DetailRowPadding")),
			OwnerTableView
		);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == NameColumn)
		{
			return SNew(SBox)
				.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.DetailColumnMargin"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(FMetaHumanStyleSet::Get().GetMargin("MetaHumanManager.IconMargin"))
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FMetaHumanStyleSet::Get().GetBrush("ItemDetails.DetailFileIcon"))
						.ColorAndOpacity(RowData->TypeColor)
					]
					+ SHorizontalBox::Slot()
					.FillContentWidth(1)
					[
						SNew(STextBlock)
						.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.DetailEntryFont"))
						.Text(FText::FromString(RowData->Name))
					]
				];
		}
		if (ColumnName == TypeColumn)
		{
			return SNew(STextBlock)
				.Margin(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.DetailColumnMargin"))
				.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.DetailEntryFont"))
				.Text(RowData->Type);
		}
		if (ColumnName == SizeColumn)
		{
			return SNew(STextBlock)
				.Margin(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.DetailColumnMargin"))
				.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.DetailEntryFont"))
				.Text(FText::AsMemory(RowData->Size));
		}

		return SNullWidget::NullWidget;
	}

	static const FName NameColumn;
	static const FName TypeColumn;
	static const FName SizeColumn;

private:
	TSharedPtr<FAssetDetails> RowData;
};

const FName SAssetDetailsRow::NameColumn = "Name";
const FName SAssetDetailsRow::TypeColumn = "Type";
const FName SAssetDetailsRow::SizeColumn = "Size";

// Handles the display of the asset preview and details
class SAssetGroupItemDetails : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SAssetGroupItemView)
		{
		}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		const float SmallThumbnailSize = FMetaHumanStyleSet::Get().GetFloat("ItemDetails.SmallThumbnailSize");
		const float LargeThumbnailSize = FMetaHumanStyleSet::Get().GetFloat("ItemDetails.LargeThumbnailSize");

		// Create the thumbnails for the asset preview pane
		FAssetThumbnailConfig Config;
		Config.ShowAssetColor = false;
		ThumbnailPool = MakeShared<FAssetThumbnailPool>(256, true);
		AssetThumbnail = MakeShared<FAssetThumbnail>(FAssetData(), SmallThumbnailSize, SmallThumbnailSize, ThumbnailPool);
		LargeAssetThumbnail = MakeShared<FAssetThumbnail>(FAssetData(), LargeThumbnailSize, LargeThumbnailSize, ThumbnailPool);

		ChildSlot
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex(this, &SAssetGroupItemDetails::GetPreviewSwitcherIndex)
			+ SWidgetSwitcher::Slot()
			[
				SNew(SAssetGroupItemPreview)
				.OnChangeMaximized(this, &SAssetGroupItemDetails::ToggleMaximizePreview)
				.IsMaximized(true)
				[
					LargeAssetThumbnail->MakeThumbnailWidget(Config)
				]
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SVerticalBox)
				// The Asset Preview
				+ SVerticalBox::Slot()
				.MinHeight(SmallThumbnailSize)
				.MaxHeight(SmallThumbnailSize)
				.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.DetailsSectionMargin"))
				[
					SNew(SAssetGroupItemPreview)
					.OnChangeMaximized(this, &SAssetGroupItemDetails::ToggleMaximizePreview)
					.IsMaximized(false)
					[
						AssetThumbnail->MakeThumbnailWidget(Config)
					]
				]
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
							.Image(this, &SAssetGroupItemDetails::GetItemAssetTypeIcon)
						]
						+ SHorizontalBox::Slot()
						.FillContentWidth(1)
						[
							SNew(STextBlock)
							.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.TitleFont"))
							.Text(this, &SAssetGroupItemDetails::GetItemName)
							.ColorAndOpacity(FStyleColors::White)
						]
					]
					+ SVerticalBox::Slot()
					.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.TitleTextMargin"))
					[
						SNew(STextBlock)
						.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.DetailsTextFont"))
						.Text(this, &SAssetGroupItemDetails::GetItemAssetTypeName)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(200.f)
				.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.DetailsSectionMargin"))
				[
					SAssignNew(ReportView, SMetaHumanAssetReportView)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.DetailsSectionMargin"))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.DetailsTextMargin"))
					[
						SNew(STextBlock)
						.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.DetailsEmphasisFont"))
						.Text(LOCTEXT("AssetDetailsTitle", "Asset Details"))
						.ColorAndOpacity(FStyleColors::White)
					]
					+ SVerticalBox::Slot()
					.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.DetailsTextMargin"))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.DetailsTextFont"))
							.Text(LOCTEXT("TotalSizeHeading", "Total Size: "))
						]
						+ SHorizontalBox::Slot()
						.FillContentWidth(1)
						[
							SNew(STextBlock)
							.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.DetailsEmphasisFont"))
							.Text(this, &SAssetGroupItemDetails::GetItemTotalSize)
							.ColorAndOpacity(FStyleColors::White)
						]
					]
					+ SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.DetailsTextFont"))
							.Text(LOCTEXT("NumAssetsHeading", "Number of referenced assets: "))
						]
						+ SHorizontalBox::Slot()
						.FillContentWidth(1)
						[
							SNew(STextBlock)
							.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.DetailsEmphasisFont"))
							.Text(this, &SAssetGroupItemDetails::GetItemNumAssets)
							.ColorAndOpacity(FStyleColors::White)
						]
					]
				]
				+ SVerticalBox::Slot()
				.FillContentHeight(1)
				[
					SAssignNew(ListView, SListView<TSharedPtr<FAssetDetails>>)
					.ListViewStyle(&FMetaHumanStyleSet::Get().GetWidgetStyle<FTableViewStyle>("MetaHumanManager.ListViewStyle"))
					.HeaderRow(SNew(SHeaderRow)
						.Style(&FMetaHumanStyleSet::Get().GetWidgetStyle<FHeaderRowStyle>("MetaHumanManager.ListHeaderRowStyle"))
						+ SHeaderRow::Column(SAssetDetailsRow::NameColumn).DefaultLabel(LOCTEXT("AssetNameHeader", "Name")).FillWidth(1.0f)
						+ SHeaderRow::Column(SAssetDetailsRow::TypeColumn).DefaultLabel(LOCTEXT("AssetTypeHeader", "Type")).FillWidth(0.6f)
						+ SHeaderRow::Column(SAssetDetailsRow::SizeColumn).DefaultLabel(LOCTEXT("AssetSizeHeader", "Disk Size")).FillWidth(0.6f)
					)
					.ListItemsSource(&AssetDetails)
					.OnGenerateRow(this, &SAssetGroupItemDetails::GetDetailsRowForItem)
				]
			]

		];
	}

	void SetItem(const TSharedPtr<FMetaHumanAssetDescription> AssetDescription)
	{
		CurrentAssetGroup = AssetDescription;
		bIsPreviewMaximized = false;
		AssetDetails.Reset();
		if (CurrentAssetGroup.IsValid())
		{
			AssetThumbnail->SetAsset(CurrentAssetGroup->AssetData);
			LargeAssetThumbnail->SetAsset(CurrentAssetGroup->AssetData);
			ReportView->SetReport(CurrentAssetGroup->VerificationReport);

			for (const FName& Package : AssetDescription->DependentPackages)
			{
				AssetDetails.Emplace(MakeShared<FAssetDetails>(Package));
			}

			// Sort, first by type, then by asset name
			AssetDetails.Sort([](const TSharedPtr<FAssetDetails>& A, const TSharedPtr<FAssetDetails>& B)
			{
				// We can assume types have consistent capitalisation
				const int Comp = A->Type.ToString().Compare(B->Type.ToString());
				if (Comp == 0)
				{
					return A->Name.ToLower().Compare(B->Name.ToLower()) <= 0;
				}
				return Comp < 0;
			});
		}
		else
		{
			ReportView->SetReport(nullptr);
		}
		ListView->RebuildList();
	}

private:
	TSharedRef<ITableRow> GetDetailsRowForItem(TSharedPtr<FAssetDetails> DetailsItem, const TSharedRef<STableViewBase>& Owner)
	{
		return SNew(SAssetDetailsRow, Owner)
			.Item(DetailsItem);
	}

	int32 GetPreviewSwitcherIndex() const
	{
		return bIsPreviewMaximized ? 0 : 1;
	}

	FText GetItemName() const
	{
		if (CurrentAssetGroup.IsValid())
		{
			return FText::FromName(CurrentAssetGroup->Name);
		}
		return LOCTEXT("NoNameAvailable", "None");
	}

	FText GetItemAssetTypeName() const
	{
		if (CurrentAssetGroup.IsValid())
		{
			if (CurrentAssetGroup->AssetType == EMetaHumanAssetType::Groom)
			{
				return LOCTEXT("GroomAssetType", "Groom");
			}
			if (CurrentAssetGroup->AssetType == EMetaHumanAssetType::SkeletalClothing)
			{
				return LOCTEXT("SkeletalClothingAssetType", "Skeletal Clothing");
			}
			if (CurrentAssetGroup->AssetType == EMetaHumanAssetType::OutfitClothing)
			{
				return LOCTEXT("OutfitClothingAssetType", "Outfit");
			}
			if (CurrentAssetGroup->AssetType == EMetaHumanAssetType::CharacterAssembly)
			{
				return LOCTEXT("CharacterAssemblyAssetType", "MetaHuman Assembly");
			}
			if (CurrentAssetGroup->AssetType == EMetaHumanAssetType::Character)
			{
				return LOCTEXT("CharacterAssetType", "MetaHuman Character");
			}
		}
		return LOCTEXT("UnknownAssetType", "Unknown");
	}

	FText GetItemTotalSize() const
	{
		const int Size = CurrentAssetGroup.IsValid() ? CurrentAssetGroup->TotalSize : 0;
		return FText::AsMemory(Size);
	}

	FText GetItemNumAssets() const
	{
		const int NumAssets = CurrentAssetGroup.IsValid() ? CurrentAssetGroup->DependentPackages.Num() : 0;
		return FText::AsNumber(NumAssets);
	}

	const FSlateBrush* GetItemAssetTypeIcon() const
	{
		if (CurrentAssetGroup.IsValid())
		{
			if (CurrentAssetGroup->AssetType == EMetaHumanAssetType::Groom)
			{
				return FMetaHumanStyleSet::Get().GetBrush("ItemDetails.GroomIcon");
			}
			if (CurrentAssetGroup->AssetType == EMetaHumanAssetType::SkeletalClothing || CurrentAssetGroup->AssetType == EMetaHumanAssetType::OutfitClothing)
			{
				return FMetaHumanStyleSet::Get().GetBrush("ItemDetails.ClothingIcon");
			}
			if (CurrentAssetGroup->AssetType == EMetaHumanAssetType::CharacterAssembly || CurrentAssetGroup->AssetType == EMetaHumanAssetType::Character)
			{
				return FMetaHumanStyleSet::Get().GetBrush("ItemDetails.CharacterIcon");
			}
		}
		return FMetaHumanStyleSet::Get().GetBrush("ItemDetails.DefaultIcon");
	}

	void ToggleMaximizePreview()
	{
		bIsPreviewMaximized = !bIsPreviewMaximized;
	}

	// UI Elements
	TSharedPtr<SListView<TSharedPtr<FAssetDetails>>> ListView;

	// Thumbnail handling
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<FAssetThumbnail> AssetThumbnail;
	TSharedPtr<FAssetThumbnail> LargeAssetThumbnail;
	TSharedPtr<SMetaHumanAssetReportView> ReportView;

	// Data
	TSharedPtr<FMetaHumanAssetDescription> CurrentAssetGroup;
	TArray<TSharedPtr<FAssetDetails>> AssetDetails;
	bool bIsPreviewMaximized = false;
};


void SAssetGroupItemView::Construct(const FArguments& InArgs)
{
	OnVerifyCallback = InArgs._OnVerify;
	OnPackageCallback = InArgs._OnPackage;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FMetaHumanStyleSet::Get().GetBrush("MetaHumanManager.RoundedBorder"))
		.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.Padding"))
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex(this, &SAssetGroupItemView::GetMainSwitcherIndex)
			+ SWidgetSwitcher::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoAssetSelected", "Please select an asset to package."))
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillContentHeight(1)
				[
					SAssignNew(ItemDetails, SAssetGroupItemDetails)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.PackageButtonPadding"))
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
						.Text(LOCTEXT("PackageButtonText", "Package..."))
						.IsEnabled(this, &SAssetGroupItemView::IsPackageButtonEnabled)
						.OnClicked(FOnClicked::CreateSP(this, &SAssetGroupItemView::OnPackage))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(FMetaHumanStyleSet::Get().GetMargin("ItemDetails.VerifyButtonPadding"))
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("VerifyButtonText", "Verify"))
						.OnClicked(FOnClicked::CreateSP(this, &SAssetGroupItemView::OnVerify))
					]
				]
			]
		]
	];
}

int32 SAssetGroupItemView::GetMainSwitcherIndex() const
{
	return CurrentAssetGroups.Num() ? 1 : 0;
}

bool SAssetGroupItemView::IsPackageButtonEnabled() const
{
	bool bShouldBeEnabled = CurrentAssetGroups.Num() != 0;
	for(const TSharedRef<FMetaHumanAssetDescription>& Item : CurrentAssetGroups)
	{
		bShouldBeEnabled = bShouldBeEnabled && IsValid(Item->VerificationReport) && Item->VerificationReport->GetReportResult() == EMetaHumanOperationResult::Success;
	}

	return bShouldBeEnabled;
}

void SAssetGroupItemView::SetItems(const TArray<TSharedRef<FMetaHumanAssetDescription>>& AssetDescriptions)
{
	CurrentAssetGroups = AssetDescriptions;
	ItemDetails->SetItem(CurrentAssetGroups.Num() ? CurrentAssetGroups[0].ToSharedPtr() : nullptr);
}

FReply SAssetGroupItemView::OnVerify() const
{
	OnVerifyCallback.ExecuteIfBound(CurrentAssetGroups);
	return FReply::Handled();
}

FReply SAssetGroupItemView::OnPackage() const
{
	OnPackageCallback.ExecuteIfBound(CurrentAssetGroups);
	return FReply::Handled();
}
}

#undef LOCTEXT_NAMESPACE
