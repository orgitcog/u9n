// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorDragTools/EditorDragToolBehaviorTarget.h"
#include "Editor.h"
#include "EditorDragTools/EditorViewportClientProxy.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "InputCoreTypes.h"

namespace UE::Editor::DragTools
{

// CVar initializer
static int32 UseITFTools = 0;
static FAutoConsoleVariableRef CVarEnableITFTools(
	TEXT("DragTools.EnableITFTools"),
	UseITFTools,
	TEXT("Is the ITF version of Drag Tools enabled?"),
	FConsoleVariableDelegate::CreateLambda(
		[](const IConsoleVariable* InVariable)
		{
			if (UseITFTools)
			{
				OnEditorDragToolsActivated().Broadcast();
			}
			else
			{
				OnEditorDragToolsDeactivated().Broadcast();
			}
		}
	)
);

bool UseEditorDragTools()
{
	return UseITFTools == 1;
}

FOnEditorDragToolsToggleDelegate& OnEditorDragToolsActivated()
{
	static FOnEditorDragToolsToggleDelegate OnDragToolsActivated;
	return OnDragToolsActivated;
}

FOnEditorDragToolsToggleDelegate& OnEditorDragToolsDeactivated()
{
	static FOnEditorDragToolsToggleDelegate OnDragToolsDeactivated;
	return OnDragToolsDeactivated;
}

} // namespace UE::Editor::DragTools

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FEditorDragTools
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FEditorDragToolBehaviorTarget::FEditorDragToolBehaviorTarget(FEditorViewportClient* InEditorViewportClient)
{
	if (InEditorViewportClient)
	{
		ModeTools = InEditorViewportClient->GetModeTools();
	}

	EditorViewportClientProxy = IEditorViewportClientProxy::CreateViewportClientProxy(InEditorViewportClient);
}

void FEditorDragToolBehaviorTarget::OnUpdateModifierState(int InModifierID, bool bInIsOn)
{
	switch (InModifierID)
	{
	case UE::Editor::DragTools::ShiftKeyMod:
		InputState.bShiftKeyDown = bInIsOn;
		break;
	case UE::Editor::DragTools::CtrlKeyMod:
		InputState.bCtrlKeyDown = bInIsOn;
		break;
	case UE::Editor::DragTools::AltKeyMod:
		InputState.bAltKeyDown = bInIsOn;
		break;
	default:;
	}
}

void FEditorDragToolBehaviorTarget::OnClickRelease(const FInputDeviceRay& InReleasePos)
{
	Start = End = FVector::ZeroVector;
	bIsDragging = false;

	// Signal that this tool is no longer active
	OnDeactivateTool().Broadcast();
}

void FEditorDragToolBehaviorTarget::OnForceEndCapture()
{
	OnTerminateDragSequence();
}

void FEditorDragToolBehaviorTarget::OnTerminateDragSequence()
{
	Start = End = FVector::ZeroVector;
	bIsDragging = false;

	InputState.bShiftKeyDown = false;
	InputState.bCtrlKeyDown = false;
	InputState.bAltKeyDown = false;

	// Signal that this tool is no longer active
	OnDeactivateTool().Broadcast();

	EditorViewportClientProxy->GetEditorViewportClient()->Invalidate(true, false);
}

bool FEditorDragToolBehaviorTarget::IsActivationChordPressed(const FInputChord& InChord) const
{
	bool bIsChordPressed = true;

	if (InChord.NeedsControl())
	{
		bIsChordPressed = bIsChordPressed && FInputDeviceState::IsCtrlKeyDown(InputState);
	}

	if (InChord.NeedsAlt())
	{
		bIsChordPressed = bIsChordPressed && FInputDeviceState::IsAltKeyDown(InputState);
	}

	if (InChord.NeedsShift())
	{
		bIsChordPressed = bIsChordPressed && FInputDeviceState::IsShiftKeyDown(InputState);
	}

	return bIsChordPressed;
}

bool FEditorDragToolBehaviorTarget::IsCurrentModeSupported()
{
	if (ModeTools)
	{
		for (const FEditorModeID UnsupportedMode : GetUnsupportedModes())
		{
			if (!ModeTools->EnsureNotInMode(UnsupportedMode))
			{
				return false;
			}
		}
	}

	return true;
}
