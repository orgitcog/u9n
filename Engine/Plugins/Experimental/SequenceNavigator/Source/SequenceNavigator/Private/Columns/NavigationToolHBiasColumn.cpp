// Copyright Epic Games, Inc. All Rights Reserved.

#include "Columns/NavigationToolHBiasColumn.h"
#include "INavigationToolView.h"
#include "Items/INavigationToolItem.h"
#include "Items/NavigationToolSequence.h"
#include "Widgets/Columns/SNavigationToolHBias.h"

#define LOCTEXT_NAMESPACE "NavigationToolHBiasColumn"

namespace UE::SequenceNavigator
{

FText FNavigationToolHBiasColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("HBiasColumn", "HBias");
}

const FSlateBrush* FNavigationToolHBiasColumn::GetIconBrush() const
{
	return FAppStyle::GetBrush(TEXT("ClassIcon.TimelineComponent"));
}

SHeaderRow::FColumn::FArguments FNavigationToolHBiasColumn::ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InView
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

TSharedRef<SWidget> FNavigationToolHBiasColumn::ConstructRowWidget(const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	if (!InItem->IsA<FNavigationToolSequence>())
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SNavigationToolHBias, InItem, InView, InRow);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
