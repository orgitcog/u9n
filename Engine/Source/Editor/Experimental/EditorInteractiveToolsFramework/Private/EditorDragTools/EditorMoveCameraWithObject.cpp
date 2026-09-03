// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorDragTools/EditorMoveCameraWithObject.h"

#include "ContextObjectStore.h"
#include "EditorDragTools/EditorDragToolBehaviorTarget.h"
#include "EditorGizmos/EditorTransformGizmo.h"
#include "EditorGizmos/EditorTransformGizmoUtil.h"
#include "EditorInteractiveGizmoManager.h"
#include "ILevelEditor.h"
#include "InteractiveGizmo.h"
#include "InteractiveGizmoManager.h"
#include "InteractiveToolManager.h"
#include "LevelEditorViewport.h"
#include "SLevelViewport.h"
#include "Tools/EdModeInteractiveToolsContext.h"

FEditorMoveCameraWithObject::FEditorMoveCameraWithObject(UInteractiveToolManager* InToolManager)
	: ToolManager(InToolManager)
{
	// New TRS Gizmos are already enabled - let's retrieve the Transform Gizmo, from which we register to drag Begin and End delegates
	if (UEditorInteractiveGizmoManager::UsesNewTRSGizmos())
	{
		const TArray<UInteractiveGizmo*> Gizmos =
			ToolManager->GetPairedGizmoManager()->FindAllGizmosOfType("EditorTransformGizmoBuilder");

		if (!Gizmos.IsEmpty())
		{
			TransformGizmo = Cast<UEditorTransformGizmo>(Gizmos[0]);
		}

		Initialize();
	}

	// If New TRS Gizmos are not enabled, we need to be able to know as soon as they are enabled
	// This allows to retrieve the Transform Gizmo, from which we register to drag Begin and End delegates
	if (UContextObjectStore* ContextStore = ToolManager->GetContextObjectStore())
	{
		if (UEditorTransformGizmoContextObject* ContextObject =
				ContextStore->FindContext<UEditorTransformGizmoContextObject>())
		{
			ContextObject->OnGizmoCreatedDelegate().AddRaw(this, &FEditorMoveCameraWithObject::OnGizmoCreatedDelegate);
		}
	}

	// In case new TRS Gizmos are disabled, we want to know that, so we can stop listening to Drag delegates
	UEditorInteractiveGizmoManager::OnUsesNewTRSGizmosChangedDelegate().AddRaw(
		this, &FEditorMoveCameraWithObject::OnUsesNewTRSGizmosChanged
	);
}

void FEditorMoveCameraWithObject::Initialize()
{
	if (TransformGizmo)
	{
		// Register Proxy Delegates
		if (TObjectPtr<UTransformProxy> TransformProxy = TransformGizmo->ActiveTarget)
		{
			OnBeginPivotEditDelegate =
				TransformProxy->OnBeginTransformEdit.AddRaw(this, &FEditorMoveCameraWithObject::OnGizmoMovementBegin);

			OnEndPivotEditDelegate =
				TransformProxy->OnEndTransformEdit.AddRaw(this, &FEditorMoveCameraWithObject::OnGizmoMovementEnd);

			OnTransformChangedDelegate =
				TransformProxy->OnTransformChanged.AddRaw(this, &FEditorMoveCameraWithObject::OnGizmoTransformChanged);
		}
	}
}

void FEditorMoveCameraWithObject::Reset()
{
	if (TransformGizmo)
	{
		// Unregister Proxy delegates
		if (TObjectPtr<UTransformProxy> TransformProxy = TransformGizmo->ActiveTarget)
		{
			TransformProxy->OnBeginTransformEdit.Remove(OnBeginPivotEditDelegate);
			TransformProxy->OnEndTransformEdit.Remove(OnEndPivotEditDelegate);
			TransformProxy->OnTransformChanged.Remove(OnTransformChangedDelegate);
		}

		TransformGizmo = nullptr;
	}
}

void FEditorMoveCameraWithObject::OnGizmoCreatedDelegate(UTransformGizmo* InTransformGizmo)
{
	if (!TransformGizmo)
	{
		TransformGizmo = Cast<UEditorTransformGizmo>(InTransformGizmo);

		Initialize();
	}
}

void FEditorMoveCameraWithObject::OnUsesNewTRSGizmosChanged(bool bInUseNewTRSGizmos)
{
	if (!bInUseNewTRSGizmos)
	{
		Reset();
	}
}

FEditorMoveCameraWithObject::~FEditorMoveCameraWithObject()
{
	UEditorInteractiveGizmoManager::OnUsesNewTRSGizmosChangedDelegate().RemoveAll(this);
	Reset();
}

void FEditorMoveCameraWithObject::AddReferencedObjects(FReferenceCollector& InCollector)
{
	if (ToolManager)
	{
		InCollector.AddReferencedObject(ToolManager);
	}

	if (TransformGizmo)
	{
		InCollector.AddReferencedObject(TransformGizmo);
	}
}

void FEditorMoveCameraWithObject::OnUpdateModifierState(int InModifierID, bool bInIsOn)
{
	// Currently only supports one modifier
	constexpr int KeyModifierID = UE::Editor::DragTools::ShiftKeyMod;

	if (InModifierID == KeyModifierID)
	{
		bModifierKeyIsPressed = bInIsOn;
	}
}

void FEditorMoveCameraWithObject::OnGizmoMovementBegin(UTransformProxy* InTransformProxy)
{
	bGizmoIsDragged = true;
}

void FEditorMoveCameraWithObject::OnGizmoMovementEnd(UTransformProxy* InTransformProxy)
{
	bGizmoIsDragged = false;
}

void FEditorMoveCameraWithObject::OnGizmoTransformChanged(UTransformProxy* InTransformProxy, UE::Math::TTransform<double> InTransform)
{
	if (bGizmoIsDragged && bModifierKeyIsPressed)
	{
		if (GCurrentLevelEditingViewportClient)
		{
			const FVector DragDelta = InTransform.GetLocation();

			if (TSharedPtr<ILevelEditor> LevelEditor = GCurrentLevelEditingViewportClient->ParentLevelEditor.Pin())
			{
				if (TSharedPtr<SLevelViewport> ViewportInterface = LevelEditor->GetActiveViewportInterface())
				{
					FLevelEditorViewportClient& LevelViewportClient = ViewportInterface->GetLevelViewportClient();
					LevelViewportClient.MoveViewportCamera(DragDelta, FRotator::ZeroRotator);
				}
			}
		}
	}
}
