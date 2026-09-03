// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNavigationToolLock.h"
#include "Items/INavigationToolItem.h"
#include "Items/NavigationToolItemUtils.h"
#include "NavigationToolSettings.h"
#include "NavigationToolView.h"
#include "Widgets/SNavigationToolTreeRow.h"

#define LOCTEXT_NAMESPACE "SNavigationToolLock"

namespace UE::SequenceNavigator
{

class FLockDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FLockDragDropOp, FDragDropOperation)

	/** Flag which defines whether to lock destination items or not */
	bool bShouldLock;

	/** Undo transaction stolen from the gutter which is kept alive for the duration of the drag */
	TUniquePtr<FScopedTransaction> UndoTransaction;

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return SNullWidget::NullWidget;
	}

	/** Create a new drag and drop operation out of the specified flag */
	static TSharedRef<FLockDragDropOp> New(const bool bShouldLock, TUniquePtr<FScopedTransaction>& ScopedTransaction)
	{
		const TSharedRef<FLockDragDropOp> Operation = MakeShared<FLockDragDropOp>();
		Operation->bShouldLock     = bShouldLock;
		Operation->UndoTransaction = MoveTemp(ScopedTransaction);
		Operation->Construct();
		return Operation;
	}
};

EItemSequenceLockState GetItemLockState(const FNavigationToolItemPtr& InItem)
{
	if (!InItem.IsValid())
	{
		return EItemSequenceLockState::None;
	}

	ISequenceLockableExtension* const LockableItem = InItem->CastTo<ISequenceLockableExtension>();
	if (!LockableItem)
	{
		return EItemSequenceLockState::None;
	}

	return LockableItem->GetLockState();
}

void SetItemLocked(const FNavigationToolItemPtr& InItem, const bool bInIsLocked)
{
	if (!InItem.IsValid())
	{
		return;
	}

	if (ISequenceLockableExtension* const LockableItem = InItem->CastTo<ISequenceLockableExtension>())
	{
		LockableItem->SetIsLocked(bInIsLocked);
	}
}

void SNavigationToolLock::Construct(const FArguments& InArgs
	, const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRowWidget)
{
	WeakItem = InItem;
	WeakView = InView;
	WeakRowWidget = InRowWidget;

	SImage::Construct(SImage::FArguments()
		.ColorAndOpacity(this, &SNavigationToolLock::GetForegroundColor)
		.Image(this, &SNavigationToolLock::GetBrush));
}

FSlateColor SNavigationToolLock::GetForegroundColor() const
{
	const FNavigationToolItemPtr Item = WeakItem.Pin();
	if (!Item.IsValid())
	{
		return FSlateColor::UseForeground();
	}

	const bool bIsItemSelected = WeakView.IsValid() && WeakView.Pin()->IsItemSelected(Item.ToSharedRef());
	const bool bIsItemHovered = WeakRowWidget.IsValid() && WeakRowWidget.Pin()->IsHovered();
	const bool bAlwaysShowLock = GetDefault<UNavigationToolSettings>()->ShouldAlwaysShowLockState();

	// We can hide the brush if settings for "Always Showing Lock State" is OFF
	// and item is not locked while also not being selected or hovered
	if (!bAlwaysShowLock
		&& GetLockState() == EItemSequenceLockState::None
		&& !bIsItemSelected
		&& !bIsItemHovered)
	{
		return FLinearColor::Transparent;
	}

	if (bIsItemHovered || IsHovered())
	{
		switch (GetLockState())
		{
		case EItemSequenceLockState::None:
			return FStyleColors::White25;

		case EItemSequenceLockState::PartiallyLocked:
			return FStyleColors::ForegroundHover;

		case EItemSequenceLockState::Locked:
			return FStyleColors::ForegroundHover;
		}
	}
	else
	{
		switch (GetLockState())
		{
		case EItemSequenceLockState::None:
			return FStyleColors::Transparent;

		case EItemSequenceLockState::PartiallyLocked:
			return FStyleColors::White25;

		case EItemSequenceLockState::Locked:
			return FStyleColors::Foreground;
		}
	}

	return FSlateColor::UseForeground();
}

const FSlateBrush* SNavigationToolLock::GetBrush() const
{
	return (GetLockState() == EItemSequenceLockState::None)
		? FAppStyle::GetBrush(TEXT("Icons.Unlock"))
		: FAppStyle::GetBrush(TEXT("Icons.Lock"));
}

FReply SNavigationToolLock::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		const bool bShouldLock = GetLockState() == EItemSequenceLockState::Locked;
		return FReply::Handled().BeginDragDrop(FLockDragDropOp::New(bShouldLock, UndoTransaction));
	}
	return FReply::Unhandled();
}

void SNavigationToolLock::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (const TSharedPtr<FLockDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FLockDragDropOp>())
	{
		SetIsLocked(DragDropOp->bShouldLock);
	}
}

FReply SNavigationToolLock::HandleClick()
{
	const TSharedPtr<INavigationToolView> ToolView = WeakView.Pin();
	const FNavigationToolItemPtr Item = WeakItem.Pin();

	if (!ToolView.IsValid() || !Item.IsValid())
	{
		return FReply::Unhandled();
	}

	UndoTransaction = MakeUnique<FScopedTransaction>(LOCTEXT("SetNavigationToolItemLock", "Set Item Lock"));

	const bool bNewIsLocked = (GetLockState() != EItemSequenceLockState::Locked);

	// We operate on all the selected items if the specified item is selected
	if (ToolView->IsItemSelected(Item.ToSharedRef()))
	{
		for (FNavigationToolItemPtr& SelectedItem : ToolView->GetSelectedItems())
		{
			SetItemLocked(SelectedItem, bNewIsLocked);
		}
	}
	else
	{
		SetIsLocked(bNewIsLocked);
	}

	return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
}

FReply SNavigationToolLock::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return HandleClick();
}

FReply SNavigationToolLock::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	return HandleClick();
}

FReply SNavigationToolLock::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		UndoTransaction.Reset();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SNavigationToolLock::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	UndoTransaction.Reset();
}

EItemSequenceLockState SNavigationToolLock::GetLockState() const
{
	return GetItemLockState(WeakItem.Pin());
}

void SNavigationToolLock::SetIsLocked(const bool bInIsLocked)
{
	SetItemLocked(WeakItem.Pin(), bInIsLocked);
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
