// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorDragTools/EditorViewportChange.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "EditorDragTools/EditorViewportClientProxy.h"
#include "EditorViewportClient.h"
#include "Framework/Commands/InputChord.h"
#include "Settings/EditorStyleSettings.h"
#include "SnappingUtils.h"

FEditorViewportChange::FEditorViewportChange(FEditorViewportClient* InEditorViewportClient)
	: FEditorDragToolBehaviorTarget(InEditorViewportClient)
	, ViewOptionOffset(FVector2D(0.0f, 0.0f))
{
	bUseSnapping = true;
	bConvertDelta = false;
}

void FEditorViewportChange::Render(const FSceneView* View, FCanvas* Canvas)
{
	FCanvasLineItem LineItem(Start, End);
	Canvas->DrawItem(LineItem);

	const FLinearColor ToolColor = GetDefault<UEditorStyleSettings>()->ViewportToolOverlayColor;
	FCanvasTextItem TextItem(
		FVector2D(FMath::FloorToFloat(End.X), FMath::FloorToFloat(End.Y) + 20),
		GetDesiredViewportTypeText(),
		GEngine->GetMediumFont(),
		ToolColor
	);
	TextItem.bCentreX = true;
	Canvas->DrawItem(TextItem);
}

FInputRayHit FEditorViewportChange::CanBeginClickDragSequence(const FInputDeviceRay& InPressPos)
{
	// Todo: this could be retrieved from a command for customization
	FInputChord ActivationChord(EModifierKey::Control, EKeys::MiddleMouseButton);

	return IsActivationChordPressed(ActivationChord)
			 ? FInputRayHit(TNumericLimits<float>::Max()) // bHit is true. Depth is max to lose the standard tiebreaker.
			 : FInputRayHit();
}

void FEditorViewportChange::OnClickPress(const FInputDeviceRay& InPressPos)
{
	FEditorViewportClient* const EditorViewportClient = EditorViewportClientProxy->GetEditorViewportClient();
	if (EditorViewportClient && EditorViewportClient->Viewport)
	{
		OnActivateTool().Broadcast();

		bIsDragging = true;

		FIntPoint MousePos;
		EditorViewportClient->Viewport->GetMousePos(MousePos);

		// Take into account DPI scale for drawing lines properly when scale is not 1.0
		Start = FVector(MousePos.X, MousePos.Y, 0) / EditorViewportClient->GetDPIScale();

		// Snap to constraints.
		if (bUseSnapping)
		{
			const float GridSize = GEditor->GetGridSize();
			const FVector GridBase(GridSize, GridSize, GridSize);
			FSnappingUtils::SnapPointToGrid(Start, GridBase);
		}

		End = Start;
	}
}

void FEditorViewportChange::OnClickDrag(const FInputDeviceRay& InDragPos)
{
	if (FEditorViewportClient* const EditorViewportClient = EditorViewportClientProxy->GetEditorViewportClient())
	{
		FIntPoint MousePos;
		EditorViewportClient->Viewport->GetMousePos(MousePos);

		// Take into account DPI scale for drawing lines properly when scale is not 1.0
		End = FVector(MousePos) / EditorViewportClient->GetDPIScale();

		ViewOptionOffset.X = End.X - Start.X;
		ViewOptionOffset.Y = End.Y - Start.Y;
	}
}

void FEditorViewportChange::OnClickRelease(const FInputDeviceRay& InReleasePos)
{
	if (FEditorViewportClient* const EditorViewportClient = EditorViewportClientProxy->GetEditorViewportClient())
	{
		EditorViewportClient->SetViewportType(GetDesiredViewportType());
	}

	FEditorDragToolBehaviorTarget::OnClickRelease(InReleasePos);
}

void FEditorViewportChange::OnTerminateDragSequence()
{
	FEditorDragToolBehaviorTarget::OnTerminateDragSequence();
}

ELevelViewportType FEditorViewportChange::GetDesiredViewportType() const
{
	ELevelViewportType ViewOption = LVT_Perspective;

	if (ViewOptionOffset.Y == 0)
	{
		if (ViewOptionOffset.X == 0)
		{
			ViewOption = LVT_Perspective;
		}
		else if (ViewOptionOffset.X > 0)
		{
			ViewOption = LVT_OrthoRight; // Right
		}
		else
		{
			ViewOption = LVT_OrthoLeft; // Left
		}
	}
	else
	{
		double OffsetRatio = ViewOptionOffset.X / ViewOptionOffset.Y;
		double DragAngle = FMath::RadiansToDegrees(FMath::Atan(OffsetRatio));

		if (ViewOptionOffset.Y >= 0)
		{
			if (DragAngle >= -15.f && DragAngle <= 15.f)
			{
				ViewOption = LVT_OrthoBottom; // Bottom
			}
			else if (DragAngle > 75.f)
			{
				ViewOption = LVT_OrthoRight; // Right
			}
			else if (DragAngle < -75.f)
			{
				ViewOption = LVT_OrthoLeft; // Left
			}
		}
		else
		{
			if (DragAngle >= -15.f && DragAngle < 15.f)
			{
				ViewOption = LVT_OrthoTop; // Top
			}
			else if (DragAngle >= 15.f && DragAngle < 75.f)
			{
				ViewOption = LVT_OrthoFront; // Front
			}
			else if (DragAngle >= -75.f && DragAngle < -15.f)
			{
				ViewOption = LVT_OrthoBack; // Back
			}
			else if (DragAngle >= 75.f)
			{
				ViewOption = LVT_OrthoLeft; // Left
			}
			else if (DragAngle <= -75.f)
			{
				ViewOption = LVT_OrthoRight; // Right
			}
		}
	}

	return ViewOption;
}

FText FEditorViewportChange::GetDesiredViewportTypeText() const
{
	switch (GetDesiredViewportType())
	{
	case LVT_Perspective:
		return FText::FromString("Perspective");
	case LVT_OrthoFreelook:
		return FText::FromString("Free Look");
	case LVT_OrthoTop:
		return FText::FromString("Top");
	case LVT_OrthoLeft:
		return FText::FromString("Left");
	case LVT_OrthoFront:
		return FText::FromString("Front");
	case LVT_OrthoBack:
		return FText::FromString("Back");
	case LVT_OrthoBottom:
		return FText::FromString("Bottom");
	case LVT_OrthoRight:
		return FText::FromString("Right");
	default:;
	}

	return FText();
}
