// Copyright Epic Games, Inc. All Rights Reserved.

#include "Columns/NavigationToolTakeColumn.h"
#include "Items/NavigationToolSequence.h"
#include "INavigationToolView.h"
#include "Items/INavigationToolItem.h"
#include "Widgets/Columns/SNavigationToolTake.h"

#define LOCTEXT_NAMESPACE "NavigationToolTakeColumn"

namespace UE::SequenceNavigator
{

FText FNavigationToolTakeColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("TakeColumn", "Take");
}

const FSlateBrush* FNavigationToolTakeColumn::GetIconBrush() const
{
	return FAppStyle::GetBrush(TEXT("ClassIcon.LevelSequence"));
}

SHeaderRow::FColumn::FArguments FNavigationToolTakeColumn::ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InView
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

TSharedRef<SWidget> FNavigationToolTakeColumn::ConstructRowWidget(const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	if (!InItem->IsA<FNavigationToolSequence>())
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SNavigationToolTake, InItem, InView, InRow);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
