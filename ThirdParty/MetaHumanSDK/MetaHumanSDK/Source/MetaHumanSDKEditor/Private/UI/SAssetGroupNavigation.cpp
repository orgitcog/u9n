// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAssetGroupNavigation.h"

#include "MetaHumanAssetReport.h"
#include "ProjectUtilities/MetaHumanAssetManager.h"
#include "UI/MetaHumanStyleSet.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "AssetGroupNavigation"

namespace UE::MetaHuman
{
void SNavigationEntry::Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
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
				.Padding({this, &SNavigationEntry::GetMarginForItem})
				.AutoWidth()
				[
					SNew(SImage)
					.Image(this, &SNavigationEntry::GetIconForItem)
				]
				+ SHorizontalBox::Slot()
				.FillContentWidth(1)
				[
					SNew(STextBlock)
					.Font(FMetaHumanStyleSet::Get().GetFontStyle("ItemDetails.ListItemFont"))
					.Text(FText::FromString(RowData->Name.ToString()))
				]
			]
		],
		OwnerTableView
	);
}

const FSlateBrush* SNavigationEntry::GetIconForItem() const
{
	if (RowData.IsValid() && IsValid(RowData->VerificationReport))
	{
		if (RowData->VerificationReport->GetReportResult() == EMetaHumanOperationResult::Failure)
		{
			return FMetaHumanStyleSet::Get().GetBrush("ReportView.ErrorIcon");
		}
		if (RowData->VerificationReport->HasWarnings())
		{
			return FMetaHumanStyleSet::Get().GetBrush("ReportView.WarningIcon");
		}
		return FMetaHumanStyleSet::Get().GetBrush("ReportView.SuccessIcon");
	}
	return nullptr;
}

FMargin SNavigationEntry::GetMarginForItem() const
{
	if (RowData.IsValid() && IsValid(RowData->VerificationReport))
	{
		return FMetaHumanStyleSet::Get().GetMargin("MetaHumanManager.IconMargin");
	}
	return FMetaHumanStyleSet::Get().GetMargin("MetaHumanManager.NoIconMargin");
}

FSectionItem::FSectionItem(const FText& InName):
	Name(InName)
{
}

void FSectionItem::SetItems(const TArray<FMetaHumanAssetDescription>& SourceItems)
{
	Items.Reset();
	for (const FMetaHumanAssetDescription& Source : SourceItems)
	{
		// Need to duplicate the list as the SListItem API requires arrays of TSharedPtr or TSharedRef for data sources
		Items.Emplace(MakeShared<FMetaHumanAssetDescription>(Source));
	}

	Items.Sort([](const TSharedPtr<FMetaHumanAssetDescription>& A, const TSharedPtr<FMetaHumanAssetDescription>& B)
	{
		return A->Name.Compare(B->Name) <= 0;
	});
}

const TArray<TSharedRef<FMetaHumanAssetDescription>>& FSectionItem::GetItems() const
{
	return Items;
}

const FText& FSectionItem::GetName() const
{
	return Name;
}

void SNavigationSection::Construct(const FArguments& InArgs)
{
	SectionItem = InArgs._SectionItem;
	ExpansionCallback = InArgs._OnExpand;
	NavigateCallback = InArgs._OnNavigate;

	ChildSlot
	[
		SAssignNew(ExpandableArea, SExpandableArea)
		.AreaTitle(SectionItem->GetName())
		.AreaTitleFont(FMetaHumanStyleSet::Get().GetFontStyle("ItemNavigation.HeaderFont"))
		.HeaderPadding(FMetaHumanStyleSet::Get().GetFloat("ItemNavigation.HeaderPadding"))
		.OnAreaExpansionChanged(FOnBooleanValueChanged::CreateSP(this, &SNavigationSection::OnExpansionChanged))
		.InitiallyCollapsed(true)
		.Padding(0)
		.BodyContent()
		[
			SAssignNew(ItemsList, SListView<TSharedRef<FMetaHumanAssetDescription>>)
			.ListItemsSource(&SectionItem->GetItems())
			.OnGenerateRow(this, &SNavigationSection::OnGenerateWidgetForItem)
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged(SListView<TSharedRef<FMetaHumanAssetDescription>>::FOnSelectionChanged::CreateSP(this, &SNavigationSection::OnSelectionChanged))
		]
	];
}

void SNavigationSection::Collapse()
{
	// Collapse is called by SAssetGroupNavigation to automatically collapse other sections. We don't want to notify
	// SAssetGroupNavigation again so switch out the callback temporarily
	const FOnExpansionChanged OldExpansionCallback = ExpansionCallback;
	ExpansionCallback = {};
	ExpandableArea->SetExpanded(false);
	ItemsList->ClearSelection();
	ExpansionCallback = OldExpansionCallback;
}

void SNavigationSection::OnSelectionChanged(TSharedPtr<FMetaHumanAssetDescription> Item, ESelectInfo::Type Type) const
{
	if (ExpandableArea->IsExpanded())
	{
		NavigateCallback.Execute(ItemsList->GetSelectedItems());
	}
}
void SNavigationSection::OnExpansionChanged(bool bIsExpanded) const
{
	ExpansionCallback.ExecuteIfBound(SectionItem, bIsExpanded);
	if (bIsExpanded)
	{
		if (SectionItem->GetItems().Num())
		{
			ItemsList->SetSelection(SectionItem->GetItems()[0]);
		}
		else
		{
			NavigateCallback.Execute({});
		}
	}
}

TSharedRef<ITableRow> SNavigationSection::OnGenerateWidgetForItem(TSharedRef<FMetaHumanAssetDescription> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SNavigationEntry, Owner)
		.Item(Item);
}

void AddSection()
{
}

void SAssetGroupNavigation::Construct(const FArguments& InArgs)
{
	NavigateCallback = InArgs._OnNavigate;
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ExpandableArea.Border"))
		.BorderBackgroundColor(FLinearColor::White)
		.Padding(FMetaHumanStyleSet::Get().GetFloat("ItemNavigation.BorderPadding"))
		[
			SAssignNew(SectionsSplitter, SSplitter)
			.PhysicalSplitterHandleSize(2)
			.Orientation(Orient_Vertical)
		]
	];

	// Add and populate the navigation sections
	Sections.Reset();
	AddSection(LOCTEXT("CharacterAssetNavigationSection", "Characters (Editable)"), EMetaHumanAssetType::Character);
	AddSection(LOCTEXT("CharacterAssemblyNavigationSection", "Characters (Assembly)"), EMetaHumanAssetType::CharacterAssembly);
	AddSection(LOCTEXT("SkeletalClothingNavigationSection", "Clothing (Skeletal)"), EMetaHumanAssetType::SkeletalClothing);
	AddSection(LOCTEXT("OutfitClothingNavigationSection", "Clothing (Outfit)"), EMetaHumanAssetType::OutfitClothing);
	AddSection(LOCTEXT("GroomsNavigationSection", "Grooms"), EMetaHumanAssetType::Groom);
}

void SAssetGroupNavigation::AddSection(const FText& Title, EMetaHumanAssetType Type)
{
	Sections.Add(MakeShared<FSectionItem>(Title));
	Sections.Last()->SetItems(UMetaHumanAssetManager::FindAssetsForPackaging(Type));
	SectionsSplitter->AddSlot()
					.SizeRule(SSplitter::SizeToContent)
					.Resizable(false)
	[
		SNew(SNavigationSection)
		.OnNavigate(NavigateCallback)
		.OnExpand(FOnExpansionChanged::CreateSP(this, &SAssetGroupNavigation::OnExpansionChanged))
		.SectionItem(Sections.Last())
	];
}

void SAssetGroupNavigation::OnExpansionChanged(TSharedPtr<FSectionItem> ExpandedSection, bool bIsExpanded)
{
	for (int i = 0; i < SectionsSplitter->GetChildren()->Num(); i++)
	{
		if (Sections[i] != ExpandedSection || !bIsExpanded)
		{
			TSharedRef<SWidget> Item = SectionsSplitter->GetChildren()->GetChildAt(i);
			TSharedRef<SNavigationSection> NavigationSection = StaticCastSharedRef<SNavigationSection>(Item);
			NavigationSection->Collapse();
			SectionsSplitter->SlotAt(i).SetSizingRule(SSplitter::SizeToContent);
		}
		else
		{
			SectionsSplitter->SlotAt(i).SetSizingRule(SSplitter::FractionOfParent);
		}
	}

	if (!bIsExpanded)
	{
		NavigateCallback.ExecuteIfBound({});
	}
}
}

#undef LOCTEXT_NAMESPACE
