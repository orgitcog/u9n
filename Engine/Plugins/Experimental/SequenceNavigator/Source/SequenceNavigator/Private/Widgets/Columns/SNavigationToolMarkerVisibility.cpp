// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNavigationToolMarkerVisibility.h"
#include "Columns/NavigationToolMarkerVisibilityColumn.h"
#include "Items/INavigationToolItem.h"
#include "Items/NavigationToolItemUtils.h"
#include "NavigationToolView.h"
#include "Styling/StyleColors.h"
#include "Widgets/SNavigationToolTreeRow.h"

#define LOCTEXT_NAMESPACE "SNavigationToolMarkerVisibility"

namespace UE::SequenceNavigator
{

class FMarkerVisibilityDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FMarkerVisibilityDragDropOp, FDragDropOperation)

	/** Flag which defines whether to hide destination items or not */
	EItemMarkerVisibility MarkerVisibility;

	/** Undo transaction stolen from the gutter which is kept alive for the duration of the drag */
	TUniquePtr<FScopedTransaction> UndoTransaction;

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return SNullWidget::NullWidget;
	}

	/** Create a new drag and drop operation out of the specified flag */
	static TSharedRef<FMarkerVisibilityDragDropOp> New(const EItemMarkerVisibility InMarkerVisibility, TUniquePtr<FScopedTransaction>& ScopedTransaction)
	{
		TSharedRef<FMarkerVisibilityDragDropOp> Operation = MakeShared<FMarkerVisibilityDragDropOp>();
		Operation->MarkerVisibility = InMarkerVisibility;
		Operation->UndoTransaction = MoveTemp(ScopedTransaction);
		Operation->Construct();
		return Operation;
	}
};

EItemMarkerVisibility GetItemMarkerVisibility(const FNavigationToolItemPtr& InItem)
{
	if (!InItem.IsValid())
	{
		return EItemMarkerVisibility::None;
	}

	IMarkerVisibilityExtension* const MarkerVisibilityItem = InItem->CastTo<IMarkerVisibilityExtension>();
	if (!MarkerVisibilityItem)
	{
		return EItemMarkerVisibility::None;
	}

	return MarkerVisibilityItem->GetMarkerVisibility();
}

void SetItemMarkerVisibility(const FNavigationToolItemPtr& InItem, const bool bInMarkersVisible)
{
	if (!InItem.IsValid())
	{
		return;
	}

	if (IMarkerVisibilityExtension* const MarkerVisibilityItem = InItem->CastTo<IMarkerVisibilityExtension>())
	{
		MarkerVisibilityItem->SetMarkerVisibility(bInMarkersVisible);
	}
}

void SNavigationToolMarkerVisibility::Construct(const FArguments& InArgs
	, const TSharedRef<FNavigationToolMarkerVisibilityColumn>& InColumn
	, const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRowWidget)
{
	WeakColumn = InColumn;
	WeakItem = InItem;
	WeakView = InView;
	WeakRowWidget = InRowWidget;

	SImage::Construct(SImage::FArguments()
		.IsEnabled(this, &SNavigationToolMarkerVisibility::IsVisibilityWidgetEnabled)
		.ColorAndOpacity(this, &SNavigationToolMarkerVisibility::GetForegroundColor)
		.Image(this, &SNavigationToolMarkerVisibility::GetBrush));
}

FReply SNavigationToolMarkerVisibility::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FReply::Handled().BeginDragDrop(FMarkerVisibilityDragDropOp::New(GetMarkerVisibility(), UndoTransaction));
	}
	return FReply::Unhandled();
}

void SNavigationToolMarkerVisibility::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FMarkerVisibilityDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FMarkerVisibilityDragDropOp>();
	if (DragDropOp.IsValid())
	{
		const bool bNewVisible = (DragDropOp->MarkerVisibility != EItemMarkerVisibility::Visible);
		SetMarkersVisible(bNewVisible);
	}
}

FReply SNavigationToolMarkerVisibility::HandleClick()
{
	if (!IsVisibilityWidgetEnabled())
	{
		return FReply::Unhandled();
	}

	const TSharedPtr<INavigationToolView> ToolView = WeakView.Pin();
	const FNavigationToolItemPtr Item = WeakItem.Pin();
	const TSharedPtr<FNavigationToolMarkerVisibilityColumn> Column = WeakColumn.Pin();

	if (!ToolView.IsValid() || !Item.IsValid() || !Column.IsValid())
	{
		return FReply::Unhandled();
	}

	// Open an undo transaction
	UndoTransaction = MakeUnique<FScopedTransaction>(LOCTEXT("SetNavigationToolMarkerVisibility", "Set Marker Visibility"));

	const bool bNewVisible = (GetMarkerVisibility() != EItemMarkerVisibility::Visible);

	// We operate on all the selected items if the specified item is selected
	if (ToolView->IsItemSelected(Item.ToSharedRef()))
	{
		for (const FNavigationToolItemPtr& SelectedItem : ToolView->GetSelectedItems())
		{
			if (IMarkerVisibilityExtension* const MarkerVisibilityItem = SelectedItem->CastTo<IMarkerVisibilityExtension>())
			{
				MarkerVisibilityItem->SetMarkerVisibility(bNewVisible);
			}
		}
	}
	else
	{
		SetMarkersVisible(bNewVisible);
	}

	return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
}

FReply SNavigationToolMarkerVisibility::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return HandleClick();
}

FReply SNavigationToolMarkerVisibility::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	return HandleClick();
}

FReply SNavigationToolMarkerVisibility::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		UndoTransaction.Reset();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SNavigationToolMarkerVisibility::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	UndoTransaction.Reset();
}

const FSlateBrush* SNavigationToolMarkerVisibility::GetBrush() const
{
	return FAppStyle::GetBrush(TEXT("AnimTimeline.SectionMarker"));
}

FSlateColor SNavigationToolMarkerVisibility::GetForegroundColor() const
{
	const IMarkerVisibilityExtension* const MarkerVisibilityExt = WeakItem.Pin()->CastTo<IMarkerVisibilityExtension>();
	if (!MarkerVisibilityExt)
	{
		return FLinearColor::Transparent;
	}

	const bool bIsItemHovered = WeakRowWidget.IsValid() && WeakRowWidget.Pin()->IsHovered();

	if (IsHovered() || bIsItemHovered)
	{
		switch (GetMarkerVisibility())
		{
		case EItemMarkerVisibility::None:
			return FStyleColors::White25;

		case EItemMarkerVisibility::PartiallyVisible:
			return FStyleColors::ForegroundHover;

		case EItemMarkerVisibility::Visible:
			return FStyleColors::ForegroundHover;
		}
	}
	else
	{
		switch (GetMarkerVisibility())
		{
		case EItemMarkerVisibility::None:
			return FStyleColors::Transparent;

		case EItemMarkerVisibility::PartiallyVisible:
			return FStyleColors::White25;

		case EItemMarkerVisibility::Visible:
			return FStyleColors::Foreground;
		}
	}

	return FStyleColors::Transparent;
}

EItemMarkerVisibility SNavigationToolMarkerVisibility::GetMarkerVisibility() const
{
	return GetItemMarkerVisibility(WeakItem.Pin());
}

void SNavigationToolMarkerVisibility::SetMarkersVisible(const bool bInVisible)
{
	SetItemMarkerVisibility(WeakItem.Pin(), bInVisible);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
