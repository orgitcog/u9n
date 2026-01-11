// Copyright Epic Games, Inc. All Rights Reserved.

#include "Columns/NavigationToolLabelColumn.h"
#include "INavigationToolView.h"
#include "Items/INavigationToolItem.h"

#define LOCTEXT_NAMESPACE "NavigationToolLabelColumn"

namespace UE::SequenceNavigator
{

FText FNavigationToolLabelColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("LabelColumn", "Label");
}

const FSlateBrush* FNavigationToolLabelColumn::GetIconBrush() const
{
	return FAppStyle::GetBrush(TEXT("ClassIcon.FontFace"));
}

SHeaderRow::FColumn::FArguments FNavigationToolLabelColumn::ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InToolView
	, const float InFillSize)
{
	const FName ColumnId = GetColumnId();

	return SHeaderRow::Column(ColumnId)
		.FillWidth(InFillSize)
		.DefaultLabel(GetColumnDisplayNameText())
		.ShouldGenerateWidget(true)
		.OnGetMenuContent(InToolView, &INavigationToolView::GetColumnMenuContent, ColumnId);
}

TSharedRef<SWidget> FNavigationToolLabelColumn::ConstructRowWidget(const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	return InItem->GenerateLabelWidget(InRow);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
