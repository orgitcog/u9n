// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "Editor.h"

class IEditorViewportClientProxy;
struct FWorldSelectionElementArgs;
class FCanvas;
class FEditorModeTools;
class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FUICommandInfo;

/**
 * ITF related helper functions
 */
namespace UE::Editor::DragTools
{

DECLARE_MULTICAST_DELEGATE(FOnEditorDragToolsToggleDelegate)

/**
 * Returns true if ITF-based drag tools should be used.
 * If an ITF version of a drag tool is not available yet, legacy will be used.
 */
EDITORINTERACTIVETOOLSFRAMEWORK_API bool UseEditorDragTools();

EDITORINTERACTIVETOOLSFRAMEWORK_API FOnEditorDragToolsToggleDelegate& OnEditorDragToolsActivated();
EDITORINTERACTIVETOOLSFRAMEWORK_API FOnEditorDragToolsToggleDelegate& OnEditorDragToolsDeactivated();

constexpr int ShiftKeyMod = 1;
constexpr int AltKeyMod = 2;
constexpr int CtrlKeyMod = 3;

} // namespace UE::Editor::DragTools

/**
 * The base class that all drag tools inherit from.
 * The drag tools implement special behaviors for the user clicking and dragging in a viewport.
 *
 * Inheriting from IClickDragBehaviorTarget so that Drag Tools logic can be shared between legacy and ITF usage of the same tools
 */
class FEditorDragToolBehaviorTarget : public IClickDragBehaviorTarget
{
public:
	FEditorDragToolBehaviorTarget(FEditorViewportClient* InEditorViewportClient);

	virtual void Render(const FSceneView* InView, FCanvas* InCanvas)
	{
	}

	/**
	 * Rendering stub for 2D viewport drag tools.
	 */
	virtual void Render(FCanvas* Canvas)
	{
	}

	/** @return true if we are dragging */
	bool IsDragging() const
	{
		return bIsDragging;
	}

	/** Does this drag tool need to have the mouse movement converted to the viewport orientation? */
	bool bConvertDelta;

	//~ Begin IModifierToggleBehaviorTarget
	virtual void OnUpdateModifierState(int InModifierID, bool bInIsOn) override;
	//~ End IModifierToggleBehaviorTarget

	//~ Begin IClickDragBehaviorTarget
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& InPressPos) override
	{
		return FInputRayHit();
	}

	virtual void OnClickPress(const FInputDeviceRay& InPressPos) override
	{
	}

	virtual void OnClickDrag(const FInputDeviceRay& InDragPos) override
	{
	}

	virtual void OnClickRelease(const FInputDeviceRay& InReleasePos) override;

	virtual void OnForceEndCapture() override;

	virtual void OnTerminateDragSequence() override;
	//~ End IClickDragBehaviorTarget

	bool IsActivationChordPressed(const FInputChord& InChord) const;

	DECLARE_MULTICAST_DELEGATE(FOnToolStateChange);
	FOnToolStateChange& OnActivateTool()
	{
		return OnToolActivatedDelegate;
	}
	FOnToolStateChange& OnDeactivateTool()
	{
		return OnToolDeactivatedDelegate;
	}

protected:
	virtual TArray<FEditorModeID> GetUnsupportedModes() { return {}; }

	bool IsCurrentModeSupported();

	FEditorModeTools* ModeTools;

	/** The start/end location of the current drag. */
	FVector Start, End;

	/** If true, the drag tool wants to be passed grid snapped values. */
	bool bUseSnapping;

	FInputDeviceState InputState;
	bool bIsDragging;

	FOnToolStateChange OnToolActivatedDelegate;
	FOnToolStateChange OnToolDeactivatedDelegate;

	IEditorViewportClientProxy* EditorViewportClientProxy;
};
