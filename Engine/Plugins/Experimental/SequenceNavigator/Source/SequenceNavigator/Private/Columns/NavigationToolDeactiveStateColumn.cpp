// Copyright Epic Games, Inc. All Rights Reserved.

#include "Columns/NavigationToolDeactiveStateColumn.h"
#include "Extensions/ISequenceInactivableExtension.h"
#include "INavigationToolView.h"
#include "Items/INavigationToolItem.h"
#include "Widgets/Columns/SNavigationToolDeactiveState.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "NavigationToolDeactiveStateColumn"

namespace UE::SequenceNavigator
{

FText FNavigationToolDeactiveStateColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("NavigationToolDeactiveStateColumn", "Sequence Evaluation");
}

const FSlateBrush* FNavigationToolDeactiveStateColumn::GetIconBrush() const
{
	return FAppStyle::GetBrush(TEXT("Sequencer.Column.Mute"));
}

SHeaderRow::FColumn::FArguments FNavigationToolDeactiveStateColumn::ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InView
	, const float InFillSize)
{
	const FName ColumnId = GetColumnId();

	return SHeaderRow::Column(ColumnId)
		.FixedWidth(24.f)
		.HAlignHeader(HAlign_Left)
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnDisplayNameText())
		.OnGetMenuContent(InView, &INavigationToolView::GetColumnMenuContent, ColumnId)
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(this, &FNavigationToolDeactiveStateColumn::GetIconBrush)
		];
}

TSharedRef<SWidget> FNavigationToolDeactiveStateColumn::ConstructRowWidget(const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	if (!InItem->IsA<ISequenceInactivableExtension>())
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SNavigationToolDeactiveState, SharedThis(this), InItem, InView, InRow)
		];
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
