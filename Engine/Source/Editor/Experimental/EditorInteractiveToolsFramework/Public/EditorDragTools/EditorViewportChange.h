// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/UnrealEdTypes.h"
#include "EditorDragToolBehaviorTarget.h"

class FCanvas;
class FEditorViewportClient;
class FLevelEditorViewportClient;
class FSceneView;

class FEditorViewportChange : public FEditorDragToolBehaviorTarget
{
public:
	explicit FEditorViewportChange(FEditorViewportClient* InEditorViewportClient);
	virtual void Render(const FSceneView* View, FCanvas* Canvas) override;

	//~ Begin IClickDragBehaviorTarget
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& InPressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& InPressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& InDragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& InReleasePos) override;
	virtual void OnTerminateDragSequence() override;
	//~ End IClickDragBehaviorTarget

private:
	ELevelViewportType GetDesiredViewportType() const;
	FText GetDesiredViewportTypeText() const;

	FVector2D ViewOptionOffset;
};
