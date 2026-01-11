// Copyright Epic Games, Inc. All Rights Reserved.

#include "Columns/NavigationToolRevisionControlColumn.h"
#include "INavigationToolView.h"
#include "Items/INavigationToolItem.h"
#include "Items/NavigationToolSequence.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Widgets/Columns/SNavigationToolRevisionControl.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "NavigationToolRevisionControlColumn"

namespace UE::SequenceNavigator
{

FText FNavigationToolRevisionControlColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("RevisionControl", "Revision Control");
}

const FSlateBrush* FNavigationToolRevisionControlColumn::GetIconBrush() const
{
	return FRevisionControlStyleManager::Get().GetBrush(TEXT("RevisionControl.Icon"));
}

SHeaderRow::FColumn::FArguments FNavigationToolRevisionControlColumn::ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InView
	, const float InFillSize)
{
	const FName ColumnId = GetColumnId();

	return SHeaderRow::Column(ColumnId)
		.FixedWidth(24.f)
		.HAlignHeader(HAlign_Center)
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnDisplayNameText())
		.OnGetMenuContent(InView, &INavigationToolView::GetColumnMenuContent, ColumnId)
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(this, &FNavigationToolRevisionControlColumn::GetIconBrush)
		];
}

TSharedRef<SWidget> FNavigationToolRevisionControlColumn::ConstructRowWidget(const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	if (!InItem->IsA<FNavigationToolSequence>())
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SNavigationToolRevisionControl, SharedThis(this), InItem, InView, InRow);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
