// Copyright Epic Games, Inc. All Rights Reserved.

#include "SControlRigTweenWidget.h"

#include "EditMode/ControlRigEditModeToolkit.h"
#include "ILevelSequenceEditorToolkit.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Tools/ControlRigPose.h"
#include "TweenLogic.h"
#include "Viewports/InViewportUIDragOperation.h"

#define LOCTEXT_NAMESPACE "ControlRigTweenWidget"

namespace UE::ControlRigEditor
{
void SControlRigTweenWidget::Construct(const FArguments& InArgs)
{
	OwningToolkit = InArgs._InOwningToolkit;
	TweenControllers = MakeUnique<FTweenLogic>(
		TAttribute<TWeakPtr<ISequencer>>::CreateLambda([]
		{
			ULevelSequence* LevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence();
			IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(LevelSequence, false);
			ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);
			return LevelSequenceEditor ? LevelSequenceEditor->GetSequencer() : nullptr;
		}),
		InArgs._InOwningEditMode.ToSharedRef()
	);
	

	ChildSlot
	[
		TweenControllers->MakeWidget()
	];
}

FReply SControlRigTweenWidget::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Need to remember where within a tab we grabbed
	const FVector2D TabGrabScreenSpaceOffset = MouseEvent.GetScreenSpacePosition() - MyGeometry.GetAbsolutePosition();

	FOnInViewportUIDropped OnUIDropped = FOnInViewportUIDropped::CreateSP(this, &SControlRigTweenWidget::FinishDraggingWidget);
	// Start dragging.
	TSharedRef<FInViewportUIDragOperation> DragDropOperation =
		FInViewportUIDragOperation::New(
			SharedThis(this),
			TabGrabScreenSpaceOffset,
			GetDesiredSize(),
			OnUIDropped
		);

	if (OwningToolkit.IsValid())
	{
		OwningToolkit.Pin()->TryRemoveTweenOverlay();
	}
	return FReply::Handled().BeginDragDrop(DragDropOperation);
}

void SControlRigTweenWidget::FinishDraggingWidget(const FVector2D InLocation) const
{
	if (OwningToolkit.IsValid())
	{
		OwningToolkit.Pin()->UpdateTweenWidgetLocation(InLocation);
		OwningToolkit.Pin()->TryShowTweenOverlay();
	}
}
}
#undef LOCTEXT_NAMESPACE
