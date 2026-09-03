// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorDragTools/DragToolsBehaviourSource.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/KeyAsModifierInputBehavior.h"
#include "EditorDragTools/EditorBoxSelect.h"
#include "EditorDragTools/EditorDragToolBehaviorTarget.h"
#include "EditorDragTools/EditorFrustumSelect.h"
#include "EditorDragTools/EditorMeasureTool.h"
#include "EditorDragTools/EditorViewportChange.h"
#include "EditorModeManager.h"
#include "EditorDragTools/EditorDuplicateDragSelection.h"
#include "EditorDragTools/EditorMoveCameraWithObject.h"
#include "Tools/EdModeInteractiveToolsContext.h"

namespace UE::Editor::DragTools
{
	// In case New TRS Gizmos are enabled, we need Mouse Drag + Key modifier Drag Tools Behaviors to be processed before
	// the Gizmo ones, otherwise Drag Behaviors will never be triggered. See:
	// * UTransformGizmo::SetupBehaviors()			[DEFAULT_GIZMO_PRIORITY]
	// * UTransformGizmo::SetupIndirectBehaviors()	[DEFAULT_GIZMO_PRIORITY - 1]
	static constexpr int HANDLED_BEFORE_GIZMO_PRIORITY = FInputCapturePriority::DEFAULT_GIZMO_PRIORITY - 3;

	// Conversely, tools which are activated with just mouse buttons input (no key modifiers, mouse drag behavior only) need to be processed after Gizmo Behaviors
	static constexpr int HANDLED_AFTER_GIZMO_PRIORITY = FInputCapturePriority::DEFAULT_GIZMO_PRIORITY + 1;

	static constexpr int FRUSTUM_SELECT_PRIORITY = HANDLED_BEFORE_GIZMO_PRIORITY;
	static constexpr int VIEWPORT_CHANGE_PRIORITY = HANDLED_BEFORE_GIZMO_PRIORITY;
	static constexpr int BOX_SELECT_PRIORITY = HANDLED_AFTER_GIZMO_PRIORITY;
	static constexpr int MEASURE_PRIORITY = VIEWPORT_CHANGE_PRIORITY + 1;

	// Used to group Viewport Change behaviors inside the BehaviorSet and remove them if needed
	static const FString ViewportChangeBehaviorGroup = TEXT("ViewportChange");

	// CVar initializer
	static int32 UseViewportChangeTool = 0;
	static FAutoConsoleVariableRef CVarEnableViewportChangeTool(
		TEXT("DragTools.EnableViewportChangeTool"),
		UseViewportChangeTool,
		TEXT("Is the ITF version of the viewport change tool enabled?"),
		FConsoleVariableDelegate::CreateLambda(
			[](const IConsoleVariable* InVariable)
			{
				if (UseViewportChangeTool)
				{
					UDragToolsBehaviorSource::OnViewportChangeToolActivated().Broadcast();
				}
				else
				{
					UDragToolsBehaviorSource::OnViewportChangeToolDeactivated().Broadcast();
				}
			}
		)
	);
}

void UDragToolsBehaviorSource::Initialize(const UEditorInteractiveToolsContext* InInteractiveToolsContext)
{
	if (!InInteractiveToolsContext)
	{
		return;
	}

	EditorInteractiveToolsContextWeak = InInteractiveToolsContext;

	TObjectPtr<UInteractiveToolManager> ToolManager = InInteractiveToolsContext->ToolManager;
	if (!ToolManager)
	{
		return;
	}

	FEditorViewportClient* EditorViewportClient = GetEditorViewportClient();

	BehaviorSet = NewObject<UInputBehaviorSet>();

	// Box Select
	{
		TSharedPtr<FEditorDragToolBehaviorTarget> BoxSelectTool = MakeShared<FEditorBoxSelect>(EditorViewportClient);

		DragTools.Add(EDragTool::BoxSelect, BoxSelectTool);

		// Add drag input behavior
		UClickDragInputBehavior* MouseBehavior = NewObject<UClickDragInputBehavior>();
		MouseBehavior->Initialize(BoxSelectTool.Get());
		MouseBehavior->SetDefaultPriority(UE::Editor::DragTools::BOX_SELECT_PRIORITY);
		BehaviorSet->Add(MouseBehavior);

		// Register modifier listeners
		UKeyAsModifierInputBehavior* KeyModifierInputBehavior = NewObject<UKeyAsModifierInputBehavior>();
		KeyModifierInputBehavior->Initialize(
			BoxSelectTool.Get(), UE::Editor::DragTools::ShiftKeyMod, FInputDeviceState::IsShiftKeyDown
		);
		KeyModifierInputBehavior->Initialize(
			BoxSelectTool.Get(), UE::Editor::DragTools::CtrlKeyMod, FInputDeviceState::IsCtrlKeyDown
		);
		KeyModifierInputBehavior->Initialize(
			BoxSelectTool.Get(), UE::Editor::DragTools::AltKeyMod, FInputDeviceState::IsAltKeyDown
		);
		KeyModifierInputBehavior->SetDefaultPriority(UE::Editor::DragTools::BOX_SELECT_PRIORITY);
		BehaviorSet->Add(KeyModifierInputBehavior);

		BoxSelectTool->OnActivateTool().AddUObject(this, &UDragToolsBehaviorSource::OnActivateTool, EDragTool::BoxSelect);
		BoxSelectTool->OnDeactivateTool().AddUObject(this, &UDragToolsBehaviorSource::OnDeactivateTool, EDragTool::BoxSelect);
	}

	// Frustum Select
	{
		TSharedPtr<FEditorDragToolBehaviorTarget> FrustumSelectTool =
			MakeShared<FEditorFrustumSelect>(EditorViewportClient);

		DragTools.Add(EDragTool::FrustumSelect, FrustumSelectTool);

		// Add drag input behavior
		UClickDragInputBehavior* MouseBehavior = NewObject<UClickDragInputBehavior>();
		MouseBehavior->Initialize(FrustumSelectTool.Get());
		MouseBehavior->SetDefaultPriority(UE::Editor::DragTools::FRUSTUM_SELECT_PRIORITY);
		MouseBehavior->SetUseLeftMouseButton();
		BehaviorSet->Add(MouseBehavior);

		// Register modifier listeners
		UKeyAsModifierInputBehavior* KeyModifierInputBehavior = NewObject<UKeyAsModifierInputBehavior>();
		KeyModifierInputBehavior->Initialize(
			FrustumSelectTool.Get(), UE::Editor::DragTools::ShiftKeyMod, FInputDeviceState::IsShiftKeyDown
		);
		KeyModifierInputBehavior->Initialize(
			FrustumSelectTool.Get(), UE::Editor::DragTools::CtrlKeyMod, FInputDeviceState::IsCtrlKeyDown
		);
		KeyModifierInputBehavior->Initialize(
			FrustumSelectTool.Get(), UE::Editor::DragTools::AltKeyMod, FInputDeviceState::IsAltKeyDown
		);
		KeyModifierInputBehavior->SetDefaultPriority(UE::Editor::DragTools::FRUSTUM_SELECT_PRIORITY);
		BehaviorSet->Add(KeyModifierInputBehavior);

		FrustumSelectTool->OnActivateTool().AddUObject(
			this, &UDragToolsBehaviorSource::OnActivateTool, EDragTool::FrustumSelect
		);
		FrustumSelectTool->OnDeactivateTool().AddUObject(
			this, &UDragToolsBehaviorSource::OnDeactivateTool, EDragTool::FrustumSelect
		);
	}

	// Measure
	{
		TSharedPtr<FEditorDragToolBehaviorTarget> MeasureTool = MakeShared<FEditorMeasureTool>(EditorViewportClient);

		DragTools.Add(EDragTool::Measure, MeasureTool);

		// Add drag input behavior
		UClickDragInputBehavior* MouseBehavior = NewObject<UClickDragInputBehavior>();
		MouseBehavior->Initialize(MeasureTool.Get());
		MouseBehavior->SetDefaultPriority(UE::Editor::DragTools::MEASURE_PRIORITY);
		MouseBehavior->SetUseMiddleMouseButton();
		BehaviorSet->Add(MouseBehavior);

		MeasureTool->OnActivateTool().AddUObject(this, &UDragToolsBehaviorSource::OnActivateTool, EDragTool::Measure);
		MeasureTool->OnDeactivateTool().AddUObject(this, &UDragToolsBehaviorSource::OnDeactivateTool, EDragTool::Measure);
	}

	// Viewport Change
	{
		if (IsViewportChangeToolEnabled())
		{
			ActivateViewportChangeTool();
		}

		OnViewportChangeToolActivated().AddUObject(this, &UDragToolsBehaviorSource::ActivateViewportChangeTool);
		OnViewportChangeToolDeactivated().AddUObject(this, &UDragToolsBehaviorSource::DeactivateViewportChangeTool);
	}

	// Duplicate Drag Tool
	{
		DuplicateDragSelection = MakeShared<FEditorDuplicateDragSelection>(ToolManager);
		UKeyAsModifierInputBehavior* KeyModifierInputBehavior = NewObject<UKeyAsModifierInputBehavior>();
		KeyModifierInputBehavior->Initialize(
			DuplicateDragSelection.Get(), UE::Editor::DragTools::AltKeyMod, FInputDeviceState::IsAltKeyDown
		);
		KeyModifierInputBehavior->SetDefaultPriority(UE::Editor::DragTools::HANDLED_BEFORE_GIZMO_PRIORITY);

		BehaviorSet->Add(KeyModifierInputBehavior);
	}

	// Move Camera with selection
	{
		MoveCameraWithObject = MakeShared<FEditorMoveCameraWithObject>(ToolManager);

		UKeyAsModifierInputBehavior* KeyModifierInputBehavior = NewObject<UKeyAsModifierInputBehavior>();
		KeyModifierInputBehavior->Initialize(MoveCameraWithObject.Get(), UE::Editor::DragTools::ShiftKeyMod, FInputDeviceState::IsShiftKeyDown);
		KeyModifierInputBehavior->SetDefaultPriority(UE::Editor::DragTools::HANDLED_BEFORE_GIZMO_PRIORITY);

		BehaviorSet->Add(KeyModifierInputBehavior);
	}
}

FEditorDragToolBehaviorTarget* UDragToolsBehaviorSource::GetActiveTool() const
{
	if (ActiveToolType >= 0)
	{
		EDragTool::Type ToolType = static_cast<EDragTool::Type>(ActiveToolType);
		if (DragTools.Contains(ToolType))
		{
			if (const TSharedPtr<FEditorDragToolBehaviorTarget>& ActiveTool = DragTools[ToolType])
			{
				return ActiveTool.Get();
			}
		}
	}

	return nullptr;
}

FEditorViewportClient* UDragToolsBehaviorSource::GetEditorViewportClient() const
{
	FEditorViewportClient* EditorViewportClient = nullptr;

	if (const UEditorInteractiveToolsContext* InteractiveToolsContext = EditorInteractiveToolsContextWeak.Get())
	{
		if (const FEditorModeTools* const ModeManager = InteractiveToolsContext->GetParentEditorModeManager())
		{
			EditorViewportClient = ModeManager->GetFocusedViewportClient();
		}

		if (!EditorViewportClient)
		{
			if (UInteractiveToolManager* ToolManager = InteractiveToolsContext->ToolManager)
			{
				if (IToolsContextQueriesAPI* ContextQueriesAPI = ToolManager->GetContextQueriesAPI())
				{
					if (FViewport* Viewport = ContextQueriesAPI->GetFocusedViewport())
					{
						EditorViewportClient = static_cast<FEditorViewportClient*>(Viewport->GetClient());
					}
				}
			}
		}
	}

	return EditorViewportClient;
}

bool UDragToolsBehaviorSource::IsViewportChangeToolEnabled()
{
	return UE::Editor::DragTools::UseViewportChangeTool == 1;
}

UDragToolsBehaviorSource::FOnOnViewportChangeToolToggleDelegate& UDragToolsBehaviorSource::OnViewportChangeToolActivated()
{
	static FOnOnViewportChangeToolToggleDelegate OnViewportChangedToolActivated;
	return OnViewportChangedToolActivated;
}

UDragToolsBehaviorSource::FOnOnViewportChangeToolToggleDelegate& UDragToolsBehaviorSource::OnViewportChangeToolDeactivated()
{
	static FOnOnViewportChangeToolToggleDelegate OnViewportChangedToolDeactivated;
	return OnViewportChangedToolDeactivated;
}

void UDragToolsBehaviorSource::RegisterSource()
{
	if (const UEditorInteractiveToolsContext* InteractiveToolsContext = EditorInteractiveToolsContextWeak.Get())
	{
		if (UInputRouter* InputRouter = InteractiveToolsContext->InputRouter)
		{
			InputRouter->RegisterSource(this);
		}
	}
}

void UDragToolsBehaviorSource::DeregisterSource()
{
	if (const UEditorInteractiveToolsContext* InteractiveToolsContext = EditorInteractiveToolsContextWeak.Get())
	{
		if (UInputRouter* InputRouter = InteractiveToolsContext->InputRouter)
		{
			InputRouter->DeregisterSource(this);
		}
	}
}

void UDragToolsBehaviorSource::OnActivateTool(EDragTool::Type InDragToolType)
{
	if (DragTools.Contains(InDragToolType))
	{
		ActiveToolType = InDragToolType;
	}
}

void UDragToolsBehaviorSource::OnDeactivateTool(EDragTool::Type InDragToolType)
{
	if (ActiveToolType == InDragToolType)
	{
		ActiveToolType = -1;
	}
}

void UDragToolsBehaviorSource::RenderTools(const FSceneView* InSceneView, FCanvas* InCanvas) const
{
	if (FEditorDragToolBehaviorTarget* ActiveDragTool = GetActiveTool())
	{
		ActiveDragTool->Render(InSceneView, InCanvas);
	}
}

void UDragToolsBehaviorSource::ActivateViewportChangeTool()
{
	// If tool is already active, ignore
	if (DragTools.Contains(EDragTool::ViewportChange))
	{
		return;
	}

	FEditorViewportClient* EditorViewportClient = GetEditorViewportClient();
	if (!EditorViewportClient)
	{
		return;
	}

	// Make sure we temporarily deregister this source while we change its behaviors list
	DeregisterSource();

	// Setup Drag Tool and Behaviors for Viewport Change
	{
		// Requires Level Viewport Client specifically.
		// todo: Maybe we could move some code from Level Viewport Client to Editor Viewport Client, to make this more generic?
		if (EditorViewportClient->IsLevelEditorClient())
		{
			TSharedPtr<FEditorDragToolBehaviorTarget> ViewportChangeTool =
				MakeShared<FEditorViewportChange>(EditorViewportClient);

			DragTools.Emplace(EDragTool::ViewportChange, ViewportChangeTool);

			// Add drag input behavior
			UClickDragInputBehavior* MouseBehavior = NewObject<UClickDragInputBehavior>();
			MouseBehavior->Initialize(ViewportChangeTool.Get());
			MouseBehavior->SetDefaultPriority(UE::Editor::DragTools::VIEWPORT_CHANGE_PRIORITY);
			MouseBehavior->SetUseMiddleMouseButton();
			BehaviorSet->Add(MouseBehavior, this, UE::Editor::DragTools::ViewportChangeBehaviorGroup);

			// Register modifier listeners
			UKeyAsModifierInputBehavior* KeyModifierInputBehavior = NewObject<UKeyAsModifierInputBehavior>();
			KeyModifierInputBehavior->Initialize(
				ViewportChangeTool.Get(), UE::Editor::DragTools::CtrlKeyMod, FInputDeviceState::IsCtrlKeyDown
			);
			KeyModifierInputBehavior->SetDefaultPriority(UE::Editor::DragTools::VIEWPORT_CHANGE_PRIORITY);
			BehaviorSet->Add(KeyModifierInputBehavior, this, UE::Editor::DragTools::ViewportChangeBehaviorGroup);

			ViewportChangeTool->OnActivateTool().AddUObject(
				this, &UDragToolsBehaviorSource::OnActivateTool, EDragTool::ViewportChange
			);
			ViewportChangeTool->OnDeactivateTool().AddUObject(
				this, &UDragToolsBehaviorSource::OnDeactivateTool, EDragTool::ViewportChange
			);
		}
	}

	// Register again
	RegisterSource();
}

void UDragToolsBehaviorSource::DeactivateViewportChangeTool()
{
	// Setup Drag Tool and Behaviors for Viewport Change
	DeregisterSource();

	// Remove Behaviors and DragTool for Viewport Change
	{
		if (BehaviorSet)
		{
			BehaviorSet->RemoveByGroup(UE::Editor::DragTools::ViewportChangeBehaviorGroup);
		}

		OnDeactivateTool(EDragTool::ViewportChange);
		if (DragTools.Contains(EDragTool::ViewportChange))
		{
			DragTools.FindAndRemoveChecked(EDragTool::ViewportChange)->OnForceEndCapture();
		}
	}

	// Register again
	RegisterSource();
}
