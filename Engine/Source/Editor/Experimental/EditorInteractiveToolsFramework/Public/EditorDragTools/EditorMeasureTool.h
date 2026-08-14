// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorDragToolBehaviorTarget.h"

class FCanvas;

class FEditorMeasureTool : public FEditorDragToolBehaviorTarget
{
public:
	explicit FEditorMeasureTool(FEditorViewportClient* InViewportClient);

	virtual void Render(const FSceneView* InView,FCanvas* InCanvas) override;

	//~ Begin IClickDragBehaviorTarget
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& InPressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& InPressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& InDragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& InReleasePos) override;
	virtual void OnTerminateDragSequence() override;
	//~ End IClickDragBehaviorTarget

private:
	/**
	 * Gets the world-space snapped position of the specified pixel position
	 */
	FVector2D GetSnappedPixelPos(FVector2D InPixelPos) const;

	/** Pixel-space positions for start and end */
	FVector2D PixelStart, PixelEnd;
};
