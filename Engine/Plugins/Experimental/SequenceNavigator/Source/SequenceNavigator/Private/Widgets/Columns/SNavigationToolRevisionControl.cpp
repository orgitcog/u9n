// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNavigationToolRevisionControl.h"
#include "Columns/NavigationToolRevisionControlColumn.h"
#include "Extensions/IRevisionControlExtension.h"
#include "Items/INavigationToolItem.h"
#include "NavigationToolView.h"
#include "Styling/StyleColors.h"
#include "Widgets/SNavigationToolTreeRow.h"

#define LOCTEXT_NAMESPACE "SNavigationToolRevisionControl"

namespace UE::SequenceNavigator
{

EItemRevisionControlState GetRevisionControlState(const FNavigationToolItemPtr& InItem)
{
	if (!InItem.IsValid())
	{
		return EItemRevisionControlState::None;
	}

	const IRevisionControlExtension* const RevisionControlItem = InItem->CastTo<IRevisionControlExtension>();
	if (!RevisionControlItem)
	{
		return EItemRevisionControlState::None;
	}

	return RevisionControlItem->GetRevisionControlState();
}

const FSlateBrush* GetRevisionControlStatusIcon(const FNavigationToolItemPtr& InItem)
{
	if (!InItem.IsValid())
	{
		return nullptr;
	}

	const IRevisionControlExtension* const RevisionControlItem = InItem->CastTo<IRevisionControlExtension>();
	if (!RevisionControlItem)
	{
		return nullptr;
	}

	return RevisionControlItem->GetRevisionControlStatusIcon();
}

FText GetRevisionControlStatusText(const FNavigationToolItemPtr& InItem)
{
	if (!InItem.IsValid())
	{
		return FText::GetEmpty();
	}

	const IRevisionControlExtension* const RevisionControlItem = InItem->CastTo<IRevisionControlExtension>();
	if (!RevisionControlItem)
	{
		return FText::GetEmpty();
	}

	return RevisionControlItem->GetRevisionControlStatusText();
}

void SNavigationToolRevisionControl::Construct(const FArguments& InArgs
	, const TSharedRef<FNavigationToolRevisionControlColumn>& InColumn
	, const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRowWidget)
{
	WeakColumn = InColumn;
	WeakItem = InItem;
	WeakView = InView;
	WeakRowWidget = InRowWidget;

	SetToolTipText(TAttribute<FText>::CreateSP(this, &SNavigationToolRevisionControl::GetToolTipText));

	SImage::Construct(SImage::FArguments()
		.Image(this, &SNavigationToolRevisionControl::GetBrush)
		.ColorAndOpacity(this, &SNavigationToolRevisionControl::GetForegroundColor));
}

FSlateColor SNavigationToolRevisionControl::GetForegroundColor() const
{
	switch (GetRevisionControlState(WeakItem.Pin()))
	{
	case EItemRevisionControlState::None:
		return FStyleColors::Transparent;
	case EItemRevisionControlState::PartiallySourceControlled:
		return FStyleColors::White25;
	case EItemRevisionControlState::SourceControlled:
		return FStyleColors::Foreground;
	}
	return FStyleColors::Transparent;
}

const FSlateBrush* SNavigationToolRevisionControl::GetBrush() const
{
	return GetRevisionControlStatusIcon(WeakItem.Pin());
}

FText SNavigationToolRevisionControl::GetToolTipText() const
{
	return GetRevisionControlStatusText(WeakItem.Pin());
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
