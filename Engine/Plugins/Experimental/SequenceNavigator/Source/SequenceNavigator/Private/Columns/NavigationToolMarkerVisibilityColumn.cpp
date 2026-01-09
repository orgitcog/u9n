// Copyright Epic Games, Inc. All Rights Reserved.

#include "Columns/NavigationToolMarkerVisibilityColumn.h"
#include "Extensions/IMarkerVisibilityExtension.h"
#include "Items/INavigationToolItem.h"
#include "INavigationToolView.h"
#include "Widgets/Columns/SNavigationToolMarkerVisibility.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NavigationToolMarkerVisibilityColumn"

namespace UE::SequenceNavigator
{

FText FNavigationToolMarkerVisibilityColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("NavigationToolMarkerVisibilityColumn", "Marker Visibility");
}

const FSlateBrush* FNavigationToolMarkerVisibilityColumn::GetIconBrush() const
{
	return FAppStyle::GetBrush(TEXT("AnimTimeline.SectionMarker"));
}

SHeaderRow::FColumn::FArguments FNavigationToolMarkerVisibilityColumn::ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InView
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
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(FAppStyle::GetBrush(TEXT("AnimTimeline.SectionMarker")))
		];
}

TSharedRef<SWidget> FNavigationToolMarkerVisibilityColumn::ConstructRowWidget(const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	if (!InItem->IsA<IMarkerVisibilityExtension>())
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SNavigationToolMarkerVisibility, SharedThis(this), InItem, InView, InRow);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
