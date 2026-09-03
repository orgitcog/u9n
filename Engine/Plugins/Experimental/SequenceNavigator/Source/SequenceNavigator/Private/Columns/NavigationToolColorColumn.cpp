// Copyright Epic Games, Inc. All Rights Reserved.

#include "Columns/NavigationToolColorColumn.h"
#include "INavigationToolView.h"
#include "Widgets/Columns/SNavigationToolColor.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "NavigationToolColorColumn"

namespace UE::SequenceNavigator
{

FText FNavigationToolColorColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("ColorColumn", "Color");
}

SHeaderRow::FColumn::FArguments FNavigationToolColorColumn::ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InView
	, const float InFillSize)
{
	const FName ColumnId = GetColumnId();

	return SHeaderRow::Column(ColumnId)
	    .FixedWidth(12.f)
		.HAlignHeader(HAlign_Center)
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		.DefaultLabel(GetColumnDisplayNameText())
	    .DefaultTooltip(GetColumnDisplayNameText())
		[
			// Display nothing in the column header
			SNew(SBox)
		];
}

TSharedRef<SWidget> FNavigationToolColorColumn::ConstructRowWidget(const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	return SNew(SNavigationToolColor, InItem, InView, InRow);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
