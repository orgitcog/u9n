// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TweenLogic.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UControlRig;
class ISequencer;
class FControlRigEditModeToolkit;

namespace UE::ControlRigEditor
{
/** Manages the tween widget in the viewport, which the user can reposition by dragging. */
class SControlRigTweenWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SControlRigTweenWidget) {}
		SLATE_ARGUMENT(TSharedPtr<FControlRigEditModeToolkit>, InOwningToolkit)
		SLATE_ARGUMENT(TSharedPtr<FControlRigEditMode>, InOwningEditMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	/** Used to update tell the toolkit when this widget has finished being dragged somewhere else. */
	TWeakPtr<FControlRigEditModeToolkit> OwningToolkit;

	/** Manages logic for the tweening widget. */
	TUniquePtr<FTweenLogic> TweenControllers;

	//~ Begin SWidget Interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	};
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End SWidget Interface
	
	void FinishDraggingWidget(const FVector2D InLocation) const;
};
}

