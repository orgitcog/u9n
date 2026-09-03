// Copyright Epic Games, Inc. All Rights Reserved.

#include "Columns/NavigationToolCommentColumn.h"
#include "INavigationToolView.h"
#include "Widgets/Columns/SNavigationToolComment.h"

#define LOCTEXT_NAMESPACE "NavigationToolCommentColumn"

namespace UE::SequenceNavigator
{

FText FNavigationToolCommentColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("CommentColumn", "Comment");
}

const FSlateBrush* FNavigationToolCommentColumn::GetIconBrush() const
{
	return FAppStyle::GetBrush(TEXT("FoliageEditMode.BubbleBorder"));
}

SHeaderRow::FColumn::FArguments FNavigationToolCommentColumn::ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InView
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

TSharedRef<SWidget> FNavigationToolCommentColumn::ConstructRowWidget(const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	return SNew(SNavigationToolComment, InItem, InView, InRow);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
