// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Internationalization/Text.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

enum class EMetaHumanAssetType : uint8;
class SExpandableArea;
struct FMetaHumanAssetDescription;

namespace UE::MetaHuman
{
// A Navigation entry in the list - represents a selectable MetaHuman Asset Group
class SNavigationEntry : public STableRow<TSharedRef<FMetaHumanAssetDescription>>
{
public:
	SLATE_BEGIN_ARGS(SNavigationEntry)
		{
		}
		SLATE_ARGUMENT(TSharedPtr<FMetaHumanAssetDescription>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView);

private:
	TSharedPtr<FMetaHumanAssetDescription> RowData;
	const FSlateBrush* GetIconForItem() const;
	FMargin GetMarginForItem() const;
};

// Data class for each section in the navigation list. Represents a collapsible section of the navigation pane
class FSectionItem final
{
public:
	FSectionItem() = default;
	FSectionItem(const FText& InName);
	void SetItems(const TArray<FMetaHumanAssetDescription>& SourceItems);
	const TArray<TSharedRef<FMetaHumanAssetDescription>>& GetItems() const;
	const FText& GetName() const;

private:
	FText Name;
	TArray<TSharedRef<FMetaHumanAssetDescription>> Items;
};

DECLARE_DELEGATE_OneParam(FOnNavigate, const TArray<TSharedRef<FMetaHumanAssetDescription>>&);
DECLARE_DELEGATE_TwoParams(FOnExpansionChanged, TSharedPtr<FSectionItem>, bool);

// Collapsible navigation section expanding to show a list of items
class SNavigationSection final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNavigationSection)
		{
		}
		SLATE_ARGUMENT(TSharedPtr<FSectionItem>, SectionItem)
		SLATE_EVENT(FOnNavigate, OnNavigate)
		SLATE_EVENT(FOnExpansionChanged, OnExpand)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	TSharedRef<ITableRow> OnGenerateWidgetForItem(TSharedRef<FMetaHumanAssetDescription> Item, const TSharedRef<STableViewBase>& Owner);
	void Collapse();
	void OnSelectionChanged(const TSharedPtr<FMetaHumanAssetDescription> Item, ESelectInfo::Type Type) const;
	void OnExpansionChanged(bool bIsExpanded) const;

private:
	TSharedPtr<FSectionItem> SectionItem;
	TSharedPtr<SExpandableArea> ExpandableArea;
	TSharedPtr<SListView<TSharedRef<FMetaHumanAssetDescription>>> ItemsList;
	FOnNavigate NavigateCallback;
	FOnExpansionChanged ExpansionCallback;
};

// Top-level navigation UI presenting a list of collapsible sections each with a tree underneath
class SAssetGroupNavigation final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetGroupNavigation)
		{
		}
		SLATE_EVENT(FOnNavigate, OnNavigate)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void OnExpansionChanged(TSharedPtr<FSectionItem> Section, bool bIsExpanded);

private:
	void AddSection(const FText& Title, EMetaHumanAssetType Type);
	TArray<TSharedRef<FSectionItem>> Sections;
	TSharedPtr<SSplitter> SectionsSplitter;
	FOnNavigate NavigateCallback;
};
}
