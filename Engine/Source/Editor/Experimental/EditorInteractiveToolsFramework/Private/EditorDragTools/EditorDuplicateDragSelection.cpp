// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorDragTools/EditorDuplicateDragSelection.h"

#include "ContextObjectStore.h"
#include "EditorGizmos/EditorTransformGizmo.h"
#include "EditorGizmos/EditorTransformGizmoUtil.h"
#include "EditorInteractiveGizmoManager.h"
#include "EditorModeManager.h"
#include "Elements/Framework/TypedElementCommonActions.h"
#include "ILevelEditor.h"
#include "InteractiveGizmo.h"
#include "InteractiveGizmoManager.h"
#include "InteractiveToolManager.h"
#include "LevelEditorViewport.h"
#include "EditorDragTools/EditorDragToolBehaviorTarget.h"
#include "Tools/AssetEditorContextInterface.h"

FEditorDuplicateDragSelection::FEditorDuplicateDragSelection(UInteractiveToolManager* InToolManager)
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
			ContextObject->OnGizmoCreatedDelegate().AddRaw(this, &FEditorDuplicateDragSelection::OnGizmoCreatedDelegate);
		}
	}

	// In case new TRS Gizmos are disabled, we want to know that, so we can stop listening to Drag delegates
	UEditorInteractiveGizmoManager::OnUsesNewTRSGizmosChangedDelegate().AddRaw(
		this, &FEditorDuplicateDragSelection::OnUsesNewTRSGizmosChanged
	);
}

void FEditorDuplicateDragSelection::Initialize()
{
	if (TransformGizmo)
	{
		// Register Proxy Delegates
		if (TObjectPtr<UTransformProxy> TransformProxy = TransformGizmo->ActiveTarget)
		{
			TransformProxy->OnBeginTransformEdit.AddRaw(
				this, &FEditorDuplicateDragSelection::OnGizmoMovementBegin
			);

			TransformProxy->OnTransformChanged.AddRaw(this, &FEditorDuplicateDragSelection::OnGizmoTransformChanged);

			TransformProxy->OnEndTransformEdit.AddRaw(
				this, &FEditorDuplicateDragSelection::OnGizmoMovementEnd
			);
		}
	}
}

void FEditorDuplicateDragSelection::Reset()
{
	if (TransformGizmo)
	{
		// Unregister Proxy delegates
		if (TObjectPtr<UTransformProxy> TransformProxy = TransformGizmo->ActiveTarget)
		{
			TransformProxy->OnBeginTransformEdit.RemoveAll(this);
			TransformProxy->OnEndTransformEdit.RemoveAll(this);
			TransformProxy->OnTransformChanged.RemoveAll(this);
		}

		TransformGizmo = nullptr;
	}
}

void FEditorDuplicateDragSelection::OnGizmoCreatedDelegate(UTransformGizmo* InTransformGizmo)
{
	if (!TransformGizmo)
	{
		TransformGizmo = Cast<UEditorTransformGizmo>(InTransformGizmo);

		Initialize();
	}
}

void FEditorDuplicateDragSelection::OnUsesNewTRSGizmosChanged(bool bInUseNewTRSGizmos)
{
	if (!bInUseNewTRSGizmos)
	{
		Reset();
	}
}

FEditorDuplicateDragSelection::~FEditorDuplicateDragSelection()
{
	UEditorInteractiveGizmoManager::OnUsesNewTRSGizmosChangedDelegate().RemoveAll(this);
	Reset();
}

void FEditorDuplicateDragSelection::AddReferencedObjects(FReferenceCollector& InCollector)
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

void FEditorDuplicateDragSelection::OnUpdateModifierState(int InModifierID, bool bInIsOn)
{
	// Currently only supports one modifier
	constexpr int KeyModifierID = UE::Editor::DragTools::AltKeyMod;

	if (InModifierID == KeyModifierID)
	{
		bModifierKeyIsPressed = bInIsOn;

		// If already dragging, let's duplicate without selecting new objects
		// This will "drop" a copy of selection matching current drag transform
		if (bGizmoIsDragged && bModifierKeyIsPressed)
		{
			constexpr bool bSelectNewElements = false;
			DuplicateSelection(bSelectNewElements);
		}
	}
}

void FEditorDuplicateDragSelection::OnForceEndCapture()
{
	bGizmoIsDragged = false;
	bModifierKeyIsPressed = false;
}

void FEditorDuplicateDragSelection::OnGizmoMovementBegin(UTransformProxy* InTransformProxy)
{
	if (!bGizmoIsDragged)
	{
		bGizmoIsDragged = true;
	}
}

void FEditorDuplicateDragSelection::OnGizmoMovementEnd(UTransformProxy* InTransformProxy)
{
	bGizmoIsDragged = false;
	bGizmoTransformHasChanged = false;
}

void FEditorDuplicateDragSelection::OnGizmoTransformChanged(UTransformProxy* InTransformProxy, UE::Math::TTransform<double> InTransform)
{
	if (bGizmoTransformHasChanged || InTransform.GetTranslation().IsNearlyZero())
	{
		return;
	}

	bGizmoTransformHasChanged = true;
	if (bModifierKeyIsPressed)
	{
		// Duplicate selection and select the newly created objects
		constexpr bool bSelectNewElements = true;
		DuplicateSelection(bSelectNewElements);
	}
}

void FEditorDuplicateDragSelection::DuplicateSelection(bool bInSelectNewElements) const
{
	if (GCurrentLevelEditingViewportClient)
	{
		if (TSharedPtr<ILevelEditor> LevelEditor = GCurrentLevelEditingViewportClient->ParentLevelEditor.Pin())
		{
			if (UTypedElementCommonActions* CommonActions = LevelEditor->GetCommonActions())
			{
				FToolBuilderState State;
				if (IAssetEditorContextInterface* AssetEditorContext =
						ToolManager->GetContextObjectStore()->FindContext<IAssetEditorContextInterface>())
				{
					if (UTypedElementSelectionSet* MutableSelectionSet = AssetEditorContext->GetMutableSelectionSet())
					{
						const TArray<FTypedElementHandle> DuplicatedElements = CommonActions->DuplicateSelectedElements(
							MutableSelectionSet, LevelEditor->GetWorld(), FVector::ZeroVector
						);
						if (DuplicatedElements.Num() > 0)
						{
							// (assuming chosen modifier key is Alt)
							// Alt Duplicate has 2 modes:
							// 1 - with Alt pressed, dragging will create and select a copy, then drag it
							// 2 - while dragging, hitting Alt will crate a copy, NOT selecting it

							// Select newly created elements: Alt + Drag only
							if (bInSelectNewElements)
							{
								// Select the newly created elements
								MutableSelectionSet->SetSelection(DuplicatedElements, FTypedElementSelectionOptions());
								MutableSelectionSet->NotifyPendingChanges();
							}

							// Notify the global mode tools, the selection set should be identical to the new actors at this point
							TArray<AActor*> SelectedActors = MutableSelectionSet->GetSelectedObjects<AActor>();
							constexpr bool bDidOffsetDuplicate = false;
							GLevelEditorModeTools().ActorsDuplicatedNotify(SelectedActors, SelectedActors, bDidOffsetDuplicate);
						}

						// Invalidate all viewports, so the new gizmo is rendered in each one
						GEditor->RedrawLevelEditingViewports();
					}
				}
			}
		}
	}
}
