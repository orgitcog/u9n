// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SNavigationToolTreeRow.h"
#include "Columns/INavigationToolColumn.h"
#include "Items/INavigationToolItem.h"
#include "NavigationToolStyle.h"
#include "NavigationToolView.h"
#include "Widgets/SNavigationToolTreeView.h"

#define LOCTEXT_NAMESPACE "SNavigationToolTreeRow"

namespace UE::SequenceNavigator
{

void SNavigationToolTreeRow::Construct(const FArguments& InArgs
	, const TSharedRef<FNavigationToolView>& InToolView
	, const TSharedPtr<SNavigationToolTreeView>& InTreeView
	, const FNavigationToolItemPtr& InItem)
{
	WeakToolView = InToolView;
	WeakTreeView = InTreeView;
	Item = InItem;
	HighlightText = InArgs._HighlightText;

	SetColorAndOpacity(TAttribute<FLinearColor>::CreateSP(&*InToolView, &FNavigationToolView::GetItemBrushColor, Item));

	SMultiColumnTableRow::Construct(FSuperRowType::FArguments()
			.Style(&FNavigationToolStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("TableViewRow")))
			.OnCanAcceptDrop(InToolView, &FNavigationToolView::OnCanDrop)
			.OnDragDetected(InToolView, &FNavigationToolView::OnDragDetected, Item)
			.OnDragEnter(InToolView, &FNavigationToolView::OnDragEnter, Item)
			.OnDragLeave(InToolView, &FNavigationToolView::OnDragLeave, Item)
			.OnAcceptDrop(InToolView, &FNavigationToolView::OnDrop)
			.OnDrop(this, &SNavigationToolTreeRow::OnDefaultDrop)
		, InTreeView.ToSharedRef());
}

TSharedRef<SWidget> SNavigationToolTreeRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	if (Item.IsValid())
	{
		const TSharedPtr<FNavigationToolView> ToolView = WeakToolView.Pin();
		check(ToolView.IsValid());
		
		if (const TSharedPtr<INavigationToolColumn> Column = ToolView->GetColumns().FindRef(InColumnName))
		{
			return Column->ConstructRowWidget(Item.ToSharedRef(), ToolView.ToSharedRef(), SharedThis(this));
		}
	}
	return SNullWidget::NullWidget;
}

FReply SNavigationToolTreeRow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const TSharedPtr<FNavigationToolView> ToolView = WeakToolView.Pin();
	if (!ToolView.IsValid())
	{
		return FReply::Unhandled();
	}

	//Select Item and the Tree of Children it contains
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		ENavigationToolItemSelectionFlags Flags = ENavigationToolItemSelectionFlags::IncludeChildren
			| ENavigationToolItemSelectionFlags::SignalSelectionChange
			| ENavigationToolItemSelectionFlags::ScrollIntoView;

		if (MouseEvent.IsControlDown())
		{
			Flags |= ENavigationToolItemSelectionFlags::AppendToCurrentSelection;
		}

		ToolView->SelectItems({Item}, Flags);

		Item->OnSelect();

		return FReply::Handled();
	}
	
	return SMultiColumnTableRow::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SNavigationToolTreeRow::OnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Select Item and the Tree of Children it contains
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (const TSharedPtr<FNavigationToolView> ToolView = WeakToolView.Pin())
		{
			Item->OnDoubleClick();

			return FReply::Handled();
		}
	}

	return SMultiColumnTableRow<FNavigationToolItemPtr>::OnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

TSharedPtr<FNavigationToolView> SNavigationToolTreeRow::GetToolView() const
{
	return WeakToolView.Pin();
}

FReply SNavigationToolTreeRow::OnDefaultDrop(const FDragDropEvent& InDragDropEvent) const
{
	if (const TSharedPtr<FNavigationToolView> ToolView = WeakToolView.Pin())
	{
		ToolView->SetDragIntoTreeRoot(false);
	}

	// Always return handled as no action should take place if the Drop wasn't accepted
	return FReply::Handled();
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
