// Copyright Epic Games, Inc. All Rights Reserved.

#include "Columns/NavigationToolInTimeColumn.h"
#include "Extensions/IInTimeExtension.h"
#include "INavigationToolView.h"
#include "Items/INavigationToolItem.h"
#include "Widgets/Columns/SNavigationToolInTime.h"

#define LOCTEXT_NAMESPACE "NavigationToolInTimeColumn"

namespace UE::SequenceNavigator
{

FText FNavigationToolInTimeColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("InTimeColumn", "In");
}

const FSlateBrush* FNavigationToolInTimeColumn::GetIconBrush() const
{
	return FAppStyle::GetBrush(TEXT("Icons.Alignment.Left"));
}

SHeaderRow::FColumn::FArguments FNavigationToolInTimeColumn::ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InView
	, const float InFillSize)
{
	const FName ColumnId = GetColumnId();

	return SHeaderRow::Column(ColumnId)
		.FillWidth(InFillSize)
		.HAlignHeader(HAlign_Center)
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Fill)
		.VAlignCell(VAlign_Center)
		.DefaultLabel(GetColumnDisplayNameText())
		.OnGetMenuContent(InView, &INavigationToolView::GetColumnMenuContent, ColumnId);
}

TSharedRef<SWidget> FNavigationToolInTimeColumn::ConstructRowWidget(const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	if (!InItem->IsA<IInTimeExtension>())
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SNavigationToolInTime, InItem, InView, InRow);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
