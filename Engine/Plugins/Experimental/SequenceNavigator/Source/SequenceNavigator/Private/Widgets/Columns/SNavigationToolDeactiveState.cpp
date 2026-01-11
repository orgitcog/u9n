// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNavigationToolDeactiveState.h"
#include "Columns/NavigationToolDeactiveStateColumn.h"
#include "Editor.h"
#include "Extensions/ISequenceInactivableExtension.h"
#include "Items/INavigationToolItem.h"
#include "NavigationToolView.h"
#include "Widgets/SNavigationToolTreeRow.h"

#define LOCTEXT_NAMESPACE "SNavigationToolDeactiveState"

namespace UE::SequenceNavigator
{

class FEvaluationStateDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FVisibilityDragDropOp, FDragDropOperation)

	/** Flag which defines whether to hide destination items or not */
	EItemSequenceInactiveState InactiveState;

	/** Undo transaction stolen from the gutter which is kept alive for the duration of the drag */
	TUniquePtr<FScopedTransaction> UndoTransaction;

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return SNullWidget::NullWidget;
	}

	/** Create a new drag and drop operation out of the specified flag */
	static TSharedRef<FEvaluationStateDragDropOp> New(const EItemSequenceInactiveState InEvaluationState, TUniquePtr<FScopedTransaction>& ScopedTransaction)
	{
		TSharedRef<FEvaluationStateDragDropOp> Operation = MakeShared<FEvaluationStateDragDropOp>();
		Operation->InactiveState = InEvaluationState;
		Operation->UndoTransaction = MoveTemp(ScopedTransaction);
		Operation->Construct();
		return Operation;
	}
};

EItemSequenceInactiveState GetItemInactiveState(const FNavigationToolItemPtr& InItem)
{
	if (!InItem.IsValid())
	{
		return EItemSequenceInactiveState::None;
	}

	ISequenceInactivableExtension* const InactivatableItem = InItem->CastTo<ISequenceInactivableExtension>();
	if (!InactivatableItem)
	{
		return EItemSequenceInactiveState::None;
	}

	return InactivatableItem->GetInactiveState();
}

void SetItemInactive(const FNavigationToolItemPtr& InItem, const bool bInInactive)
{
	if (!InItem.IsValid())
	{
		return;
	}

	if (ISequenceInactivableExtension* const InactivatableItem = InItem->CastTo<ISequenceInactivableExtension>())
	{
		InactivatableItem->SetIsInactive(bInInactive);
	}
}

void SNavigationToolDeactiveState::Construct(const FArguments& InArgs
	, const TSharedRef<FNavigationToolDeactiveStateColumn>& InColumn
	, const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRowWidget)
{
	WeakColumn = InColumn;
	WeakItem = InItem;
	WeakView = InView;
	WeakRowWidget = InRowWidget;

	SImage::Construct(SImage::FArguments()
		.IsEnabled(this, &SNavigationToolDeactiveState::IsVisibilityWidgetEnabled)
		.ColorAndOpacity(this, &SNavigationToolDeactiveState::GetForegroundColor)
		.Image(this, &SNavigationToolDeactiveState::GetBrush));
}

FReply SNavigationToolDeactiveState::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		const EItemSequenceInactiveState NewState = (GetInactiveState() != EItemSequenceInactiveState::None)
			? EItemSequenceInactiveState::Inactive : EItemSequenceInactiveState::None;
		return FReply::Handled().BeginDragDrop(FEvaluationStateDragDropOp::New(NewState, UndoTransaction));
	}
	return FReply::Unhandled();
}

void SNavigationToolDeactiveState::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FEvaluationStateDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FEvaluationStateDragDropOp>();
	if (DragDropOp.IsValid())
	{
		const bool bItemInactive = (DragDropOp->InactiveState != EItemSequenceInactiveState::None);
		SetIsInactive(bItemInactive);
	}
}

FReply SNavigationToolDeactiveState::HandleClick()
{
	if (!IsVisibilityWidgetEnabled())
	{
		return FReply::Unhandled();
	}

	const TSharedPtr<INavigationToolView> ToolView = WeakView.Pin();
	const FNavigationToolItemPtr Item = WeakItem.Pin();
	const TSharedPtr<FNavigationToolDeactiveStateColumn> Column = WeakColumn.Pin();

	if (!ToolView.IsValid() || !Item.IsValid() || !Column.IsValid())
	{
		return FReply::Unhandled();
	}

	// Open an undo transaction
	UndoTransaction = MakeUnique<FScopedTransaction>(LOCTEXT("SetNavigationToolItemInactiveState", "Set Item Inactive State"));

	const bool bNewIsInactive = (GetInactiveState() != EItemSequenceInactiveState::Inactive);

	// We operate on all the selected items if the specified item is selected
	if (ToolView->IsItemSelected(Item.ToSharedRef()))
	{
		for (FNavigationToolItemPtr& SelectedItem : ToolView->GetSelectedItems())
		{
			SetItemInactive(SelectedItem, bNewIsInactive);
		}
	}
	else
	{
		SetIsInactive(bNewIsInactive);
	}

	return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
}

FReply SNavigationToolDeactiveState::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return HandleClick();
}

FReply SNavigationToolDeactiveState::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	return HandleClick();
}

FReply SNavigationToolDeactiveState::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		UndoTransaction.Reset();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SNavigationToolDeactiveState::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	UndoTransaction.Reset();
}

const FSlateBrush* SNavigationToolDeactiveState::GetBrush() const
{
	return FAppStyle::GetBrush(TEXT("Sequencer.Column.Mute"));
}

FSlateColor SNavigationToolDeactiveState::GetForegroundColor() const
{
	const FNavigationToolItemPtr Item = WeakItem.Pin();
	if (!Item.IsValid())
	{
		return FStyleColors::Transparent;
	}

	const bool bIsItemHovered = WeakRowWidget.IsValid() && WeakRowWidget.Pin()->IsHovered();
	if (bIsItemHovered || IsHovered())
	{
		switch (GetInactiveState())
		{
		case EItemSequenceInactiveState::None:
			return FStyleColors::White25;

		case EItemSequenceInactiveState::PartiallyInactive:
			return FStyleColors::ForegroundHover;

		case EItemSequenceInactiveState::Inactive:
			return FStyleColors::ForegroundHover;
		}
	}
	else
	{
		switch (GetInactiveState())
		{
		case EItemSequenceInactiveState::None:
			return FStyleColors::Transparent;

		case EItemSequenceInactiveState::PartiallyInactive:
			return FStyleColors::White25;

		case EItemSequenceInactiveState::Inactive:
			return FStyleColors::Foreground;
		}
	}

	return FStyleColors::Transparent;
}

EItemSequenceInactiveState SNavigationToolDeactiveState::GetInactiveState() const
{
	return GetItemInactiveState(WeakItem.Pin());
}

void SNavigationToolDeactiveState::SetIsInactive(const bool bInIsInactive)
{
	SetItemInactive(WeakItem.Pin(), bInIsInactive);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
