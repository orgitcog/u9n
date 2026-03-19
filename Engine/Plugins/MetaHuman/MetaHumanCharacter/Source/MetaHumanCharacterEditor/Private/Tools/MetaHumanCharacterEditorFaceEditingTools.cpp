// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorFaceEditingTools.h"
#include "MetaHumanCharacterEditorMeshEditingTools.h"
#include "MetaHumanCharacterEditorToolCommandChange.h"
#include "MetaHumanCharacterEditorToolTargetUtil.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterEditorActor.h"
#include "MetaHumanCharacterEditorSettings.h"
#include "MetaHumanCharacterEditorLog.h"
#include "MetaHumanCharacterEditorViewportClient.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanRigEvaluatedState.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "DynamicMesh/MeshNormals.h"
#include "ToolTargetManager.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolChange.h"
#include "InteractiveToolObjects.h"
#include "CanvasItem.h"
#include "ModelingToolTargetUtil.h"
#include "EditorViewportClient.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "PrimitiveDrawingUtils.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "BaseGizmos/TransformProxy.h"
#include "BaseGizmos/CombinedTransformGizmo.h"
#include "SceneView.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditorFaceEditingTools"

static TAutoConsoleVariable<bool> CVarMHCharacterShowSculptingVertices
{
	TEXT("mh.Character.ShowSculptingVertices"),
	false,
	TEXT("Set to true to show the face vertices during sculpting."),
	ECVF_Default
};


UInteractiveTool* UMetaHumanCharacterEditorFaceEditingToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	switch (ToolType)
	{
		case EMetaHumanCharacterFaceEditingTool::Move:
		{
			UMetaHumanCharacterEditorFaceMoveTool* MoveTool = NewObject<UMetaHumanCharacterEditorFaceMoveTool>(InSceneState.ToolManager);
			MoveTool->SetTarget(Target);
			MoveTool->SetWorld(InSceneState.World);
			return MoveTool;
		}

		case EMetaHumanCharacterFaceEditingTool::Sculpt:
		{
			UMetaHumanCharacterEditorFaceSculptTool* SculptTool = NewObject<UMetaHumanCharacterEditorFaceSculptTool>(InSceneState.ToolManager);
			SculptTool->SetTarget(Target);
			SculptTool->SetWorld(InSceneState.World);
			return SculptTool;
		}

		case EMetaHumanCharacterFaceEditingTool::Blend:
		{
			UMetaHumanCharacterEditorFaceBlendTool* BlendTool = NewObject<UMetaHumanCharacterEditorFaceBlendTool>(InSceneState.ToolManager);
			BlendTool->SetTarget(Target);
			BlendTool->SetWorld(InSceneState.World);
			return BlendTool;
		}

		default:
			check(false);
			break;
	}


	return nullptr;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorFaceEditingToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass(),
		}
	);

	return TypeRequirements;
}

// -----------------------------------------------------
// FaceStateChangeTransactor implementation ------------
// -----------------------------------------------------

FSimpleMulticastDelegate& UFaceStateChangeTransactor::GetStateChangedDelegate(UMetaHumanCharacter* InMetaHumanCharacter)
{
	return UMetaHumanCharacterEditorSubsystem::Get()->OnFaceStateChanged(InMetaHumanCharacter);
}

void UFaceStateChangeTransactor::CommitShutdownState(UInteractiveToolManager* InToolManager, UMetaHumanCharacter* InMetaHumanCharacter, EToolShutdownType InShutdownType, const FText& InCommandChangeDescription)
{
	// If BeginDragState is valid it means we are shutting down in the middle of making changes so we create a transaction
	// this could happen (for example) if we auto-rig
	if (BeginDragState.IsValid())
	{

		const FText CommandChangeDescription = FText::Format(LOCTEXT("FaceEditingCommandChangeTransaction", "{0} {1}"),
															 UEnum::GetDisplayValueAsText(InShutdownType),
															 InCommandChangeDescription);

		// Creates a command change that allows the user to revert back the state
		TUniquePtr<FMetaHumanCharacterEditorFaceToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorFaceToolCommandChange>(BeginDragState.ToSharedRef(), InMetaHumanCharacter, InToolManager);
		InToolManager->GetContextTransactionsAPI()->AppendChange(InMetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);
	}

	// commit the current face state if needed during shutdown
	UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	if (TSharedPtr<FMetaHumanCharacterIdentity::FState> NewState = Subsystem->CopyFaceState(InMetaHumanCharacter))
	{
		Subsystem->CommitFaceState(InMetaHumanCharacter, NewState.ToSharedRef());
	}
}
	
void UFaceStateChangeTransactor::StoreBeginDragState(UMetaHumanCharacter* InMetaHumanCharacter)
{
	// Stores the face state when the drag start to allow it to be undone while the tool is active
	UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	BeginDragState = Subsystem->CopyFaceState(InMetaHumanCharacter);
}

void UFaceStateChangeTransactor::CommitEndDragState(UInteractiveToolManager* InToolManager, UMetaHumanCharacter* InMetaHumanCharacter, const FText& InCommandChangeDescription)
{
	TUniquePtr<FMetaHumanCharacterEditorFaceToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorFaceToolCommandChange>(
		BeginDragState.ToSharedRef(), 
		InMetaHumanCharacter, 
		InToolManager);

	InToolManager->GetContextTransactionsAPI()->AppendChange(InMetaHumanCharacter, MoveTemp(CommandChange), InCommandChangeDescription);
	BeginDragState = nullptr;
}

TSharedRef<FMetaHumanCharacterIdentity::FState> UFaceStateChangeTransactor::GetBeginDragState() const
{
	return BeginDragState.ToSharedRef();
}

bool UFaceStateChangeTransactor::IsDragStateValid() const
{
	return BeginDragState.IsValid();
}

void UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Override function to process EPropertyChangeType::ValueSet events for the edited properties
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnPropertyValueSetDelegate.ExecuteIfBound(PropertyChangedEvent);
}

void UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties::CopyFrom(const FMetaHumanCharacterFaceEvaluationSettings& InFaceEvaluationSettings)
{
	GlobalDelta = InFaceEvaluationSettings.GlobalDelta;
	HeadScale = InFaceEvaluationSettings.HeadScale;
}

void UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties::CopyTo(FMetaHumanCharacterFaceEvaluationSettings& OutFaceEvaluationSettings)
{
	OutFaceEvaluationSettings.GlobalDelta = GlobalDelta;
	OutFaceEvaluationSettings.HeadScale = HeadScale;
}


// -----------------------------------------------------
// FaceMoveTool implementation -------------------------
// -----------------------------------------------------

FVector3f FGizmoBoundaryConstraintFunctions::GizmoTranslationFunction(const FVector3f& Delta) const
{
	FVector3f NewPosition = BeginDragGizmoPosition + Delta;
	FVector3f NewBoundedPosition = NewPosition.BoundToBox(MinGizmoPosition, MaxGizmoPosition);
	FVector3f BoundDelta = NewPosition - NewBoundedPosition;
	for (int32 k = 0; k < 3; ++k)
	{
		// soft bounds to allow the user to translate a region outside the model bounds
		NewPosition[k] = NewBoundedPosition[k] + 2.0f / (1.0f + FMath::Exp(-2.0f * BoundDelta[k] * BBoxSoftBound)) - 1.0f;
	}
	return NewPosition - BeginDragGizmoPosition;
}

FVector3f FGizmoBoundaryConstraintFunctions::GizmoRotationFunction(const FVector3f& Delta) const
{
	FVector3f DeltaDeg = FMathf::RadToDeg * Delta;
	for (int k = 0; k < 3; ++k)
	{
		while (DeltaDeg[k] >= 180.0f) DeltaDeg[k] -= 360.0f;
		while (DeltaDeg[k] < -180.0f) DeltaDeg[k] += 360.0f;
	}
	FVector3f NewRotation = BeginDragGizmoRotation + DeltaDeg;
	FVector3f NewBoundedRotation = NewRotation.BoundToBox(MinGizmoRotation, MaxGizmoRotation);
	FVector3f BoundDelta = NewRotation - NewBoundedRotation;
	for (int32 k = 0; k < 3; ++k)
	{
		NewRotation[k] = NewBoundedRotation[k] + 2.0f / (1.0f + FMath::Exp(-2.0f * BoundDelta[k] * BBoxSoftBound)) - 1.0f;
	}
	return (NewRotation - BeginDragGizmoRotation) * FMathf::DegToRad;
}

void UMetaHumanCharacterEditorFaceMoveToolProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Override function to process EPropertyChangeType::ValueSet events for the edited properties
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnPropertyValueSetDelegate.ExecuteIfBound(PropertyChangedEvent);
}

void UMetaHumanCharacterEditorFaceMoveTool::Setup()
{
	// Instead of this just create a function for adding UTransformProxy to all of the Manipulators
	Super::Setup();

	MoveProperties = NewObject<UMetaHumanCharacterEditorFaceMoveToolProperties>(this);
	AddToolPropertySource(MoveProperties);
	MoveProperties->RestoreProperties(this, GetCommandChangeDescription().ToString());

	MoveProperties->OnPropertyValueSetDelegate.BindWeakLambda(this, [this](const FPropertyChangedEvent& PropertyChangedEvent)
	{
		SetGizmoType(MoveProperties->GizmoType);
	});


	TransformProxy = NewObject<UTransformProxy>(this);
	// Give random position initially
	TransformProxy->SetTransform(ManipulatorComponents[0]->GetComponentTransform());
	TransformProxy->bRotatePerObject = true;

	UInteractiveGizmoManager* GizmoManager = GetToolManager()->GetPairedGizmoManager();
	// currently no free translate as delta functions are not supported in that mode
	TransformGizmo = GizmoManager->CreateCustomTransformGizmo(ETransformGizmoSubElements::TranslateAllAxes | ETransformGizmoSubElements::TranslateAllPlanes | ETransformGizmoSubElements::RotateAllAxes
		| ETransformGizmoSubElements::ScaleUniform /* | ETransformGizmoSubElements::FreeTranslate */, this);
	TransformGizmo->SetActiveTarget(TransformProxy, GetToolManager());
	TransformGizmo->SetVisibility(false);

	TransformGizmo->ActiveGizmoMode = EToolContextTransformGizmoMode::NoGizmo;
		
	TransformGizmo->bUseContextGizmoMode = false;
	TransformGizmo->bSnapToWorldGrid = false;
	TransformGizmo->bSnapToWorldRotGrid = false;
	TransformGizmo->bSnapToScaleGrid = false;
	TransformGizmo->bUseContextCoordinateSystem = false;
	TransformGizmo->CurrentCoordinateSystem = EToolContextCoordinateSystem::Local;

	TFunction<bool(double AxisDelta, double& SnappedDelta)> XAxis = [this](double AxisDelta, double& SnappedDelta) -> bool {
		SnappedDelta = GizmoConstraints.GizmoTranslationFunction(FVector3f{ (float)AxisDelta, 0, 0 } * MeshEditingToolProperties->Speed)[0];
		return true;
		};
	TFunction<bool(double AxisDelta, double& SnappedDelta)> YAxis = [this](double AxisDelta, double& SnappedDelta) -> bool {
		SnappedDelta = GizmoConstraints.GizmoTranslationFunction(FVector3f{ 0, (float)AxisDelta, 0 } * MeshEditingToolProperties->Speed)[1];
		return true;
		};
	TFunction<bool(double AxisDelta, double& SnappedDelta)> ZAxis = [this](double AxisDelta, double& SnappedDelta) -> bool {
		SnappedDelta = GizmoConstraints.GizmoTranslationFunction(FVector3f{ 0, 0, (float)AxisDelta } * MeshEditingToolProperties->Speed)[2];
		return true;
		};
	TransformGizmo->SetCustomTranslationDeltaFunctions(XAxis, YAxis, ZAxis);

	XAxis = [this](double AxisDelta, double& SnappedDelta) -> bool {
		SnappedDelta = - GizmoConstraints.GizmoRotationFunction(FVector3f{ - (float)AxisDelta, 0, 0 } * MeshEditingToolProperties->Speed)[0];
		DraggedGizmoElements |= ETransformGizmoSubElements::RotateAxisX;
		return true;
		};
	YAxis = [this](double AxisDelta, double& SnappedDelta) -> bool {
		SnappedDelta = - GizmoConstraints.GizmoRotationFunction(FVector3f{ 0, - (float)AxisDelta, 0 } * MeshEditingToolProperties->Speed)[1];
		DraggedGizmoElements |= ETransformGizmoSubElements::RotateAxisY;
		return true;
		};
	ZAxis = [this](double AxisDelta, double& SnappedDelta) -> bool {
		SnappedDelta = GizmoConstraints.GizmoRotationFunction(FVector3f{ 0, 0, (float)AxisDelta } * MeshEditingToolProperties->Speed)[2];
		DraggedGizmoElements |= ETransformGizmoSubElements::RotateAxisZ;
		return true;
		};
	TransformGizmo->SetCustomRotationDeltaFunctions(XAxis, YAxis, ZAxis);

	TransformProxy->OnBeginTransformEdit.AddLambda([this](UTransformProxy* Proxy)
	{		
		if (MoveProperties->GizmoType != EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace && SelectedGizmoManipulator != INDEX_NONE)
		{
			UStaticMeshComponent* ManipulatorComponent = ManipulatorComponents[SelectedGizmoManipulator];
			SetManipulatorDragState(SelectedGizmoManipulator, true);
			MeshStateChangeTransactor->StoreBeginDragState(MetaHumanCharacter);

			TSharedRef<FMetaHumanCharacterIdentity::FState> BeginDragState = Cast<UFaceStateChangeTransactor>(MeshStateChangeTransactor.GetObject())->GetBeginDragState();

			BeginDragState->GetGizmoPosition(SelectedGizmoManipulator, GizmoConstraints.BeginDragGizmoPosition);
			BeginDragState->GetGizmoPositionBounds(SelectedGizmoManipulator, GizmoConstraints.MinGizmoPosition, GizmoConstraints.MaxGizmoPosition, GizmoConstraints.BBoxReduction, GizmoConstraints.bExpandToCurrent);
			BeginDragState->GetGizmoRotation(SelectedGizmoManipulator, GizmoConstraints.BeginDragGizmoRotation);
			BeginDragState->GetGizmoRotationBounds(SelectedGizmoManipulator, GizmoConstraints.MinGizmoRotation, GizmoConstraints.MaxGizmoRotation, GizmoConstraints.bExpandToCurrent);
			BeginDragState->GetGizmoScale(SelectedGizmoManipulator, GizmoConstraints.BeginDragGizmoScale);
			BeginDragState->GetGizmoScaleBounds(SelectedGizmoManipulator, GizmoConstraints.MinGizmoScale, GizmoConstraints.MaxGizmoScale, GizmoConstraints.bExpandToCurrent);
			DraggedGizmoElements = ETransformGizmoSubElements::None;

			BeginDragTransform = Proxy->GetTransform();
			
			if (MeshEditingToolProperties->bHideWhileDragging)
			{
				for (int32 ManipulatorIndex = 0; ManipulatorIndex < ManipulatorComponents.Num(); ++ManipulatorIndex)
				{
					ManipulatorComponents[ManipulatorIndex]->SetVisibility(false);
				}
			}
		}
	});

	TransformProxy->OnTransformChanged.AddLambda([this](UTransformProxy* Proxy, FTransform NewTransform)
	{
		if (SelectedGizmoManipulator != INDEX_NONE && BeginDragTransform.IsSet())
		{
			if (MoveProperties->GizmoType != EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace)
			{
				CurrentDragTransform = NewTransform;
			}
		}
	});

	TransformProxy->OnEndTransformEdit.AddLambda([this](UTransformProxy* Proxy)
	{
		if (MoveProperties->GizmoType != EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace && SelectedGizmoManipulator != INDEX_NONE)
		{
			if (ManipulatorComponents.IsValidIndex(SelectedGizmoManipulator))
			{
				SetManipulatorDragState(SelectedGizmoManipulator, false);
				TransformGizmo->ReinitializeGizmoTransform(ManipulatorComponents[SelectedGizmoManipulator]->GetComponentTransform());
			}

			// Restore the visibility of all manipulators
			for (int32 ManipulatorIndex = 0; ManipulatorIndex < ManipulatorComponents.Num(); ++ManipulatorIndex)
			{
				ManipulatorComponents[ManipulatorIndex]->SetVisibility(true);
			}

			MeshStateChangeTransactor->CommitEndDragState(GetToolManager(), MetaHumanCharacter, GetCommandChangeIntermediateDescription());
		}

		// Reset the pending move delta to avoid OnTick moving manipulators that were not selected in BeginDragTranslateManipulator
		PendingMoveDelta = FVector3f::ZeroVector;
		CurrentDragTransform.Reset();
		BeginDragTransform.Reset();
	});

	SetGizmoType(MoveProperties->GizmoType);
}

void UMetaHumanCharacterEditorFaceMoveTool::Shutdown(EToolShutdownType InShutdownType)
{	
	Super::Shutdown(InShutdownType);

	MoveProperties->SaveProperties(this, GetCommandChangeDescription().ToString());

	GetToolManager()->GetPairedGizmoManager()->DestroyAllGizmosByOwner(this);
}

void UMetaHumanCharacterEditorFaceMoveTool::OnTick(float InDeltaTime)
{
	//Update the manipulators hover state
	for (int32 ManipulatorIndex = 0; ManipulatorIndex < ManipulatorComponents.Num(); ++ManipulatorIndex)
	{
		const bool bIsSelected = (ManipulatorIndex == SelectedManipulator) || (ManipulatorIndex == SelectedGizmoManipulator);
		SetManipulatorHoverState(ManipulatorIndex, bIsSelected);
	}

	UFaceStateChangeTransactor* FaceStateChangeTransactor = Cast<UFaceStateChangeTransactor>(MeshStateChangeTransactor.GetObject());
	
	if (FaceStateChangeTransactor && FaceStateChangeTransactor->IsDragStateValid())
	{
		// Update Translation
		if (!PendingMoveDelta.IsZero() && MoveProperties->GizmoType == EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace)
		{
			if (SelectedManipulator != INDEX_NONE)
			{
				if (BeginDragMoveDelta.Length() == 0)
				{
					FaceStateChangeTransactor->GetBeginDragState()->GetGizmoPosition(SelectedManipulator, GizmoConstraints.BeginDragGizmoPosition);
					FaceStateChangeTransactor->GetBeginDragState()->GetGizmoPositionBounds(SelectedManipulator, GizmoConstraints.MinGizmoPosition, GizmoConstraints.MaxGizmoPosition, GizmoConstraints.BBoxReduction, GizmoConstraints.bExpandToCurrent);
				}
				BeginDragMoveDelta += PendingMoveDelta;
				FVector3f Delta = GizmoConstraints.GizmoTranslationFunction(BeginDragMoveDelta * MeshEditingToolProperties->Speed);
				FVector3f NewPosition = GizmoConstraints.BeginDragGizmoPosition + Delta;
				const TArray<FVector3f> ManipulatorPositions = UMetaHumanCharacterEditorSubsystem::Get()->SetFaceGizmoPosition(MetaHumanCharacter, FaceStateChangeTransactor->GetBeginDragState(), SelectedManipulator, NewPosition, MeshEditingToolProperties->bSymmetricModeling, /*bInEnforceBounds*/ false);
				UpdateManipulatorPositions(ManipulatorPositions);
				PendingMoveDelta = FVector3f::ZeroVector;
			}
		}

		if (BeginDragTransform.IsSet() && CurrentDragTransform.IsSet() && SelectedGizmoManipulator != INDEX_NONE)
		{
			// Update Translation
			if (MoveProperties->GizmoType == EMetaHumanCharacterMoveToolManipulationGizmos::Translate)
			{
				FVector3f Delta = FVector3f(CurrentDragTransform.GetValue().GetTranslation() - BeginDragTransform.GetValue().GetTranslation());
				FVector3f NewPosition = GizmoConstraints.BeginDragGizmoPosition + Delta;
				const TArray<FVector3f> ManipulatorPositions = UMetaHumanCharacterEditorSubsystem::Get()->SetFaceGizmoPosition(MetaHumanCharacter, FaceStateChangeTransactor->GetBeginDragState(), SelectedGizmoManipulator, NewPosition, MeshEditingToolProperties->bSymmetricModeling, /*bInEnforceBounds*/ false);
				UpdateManipulatorPositions(ManipulatorPositions);
			}

			// Update Rotation
			if (MoveProperties->GizmoType == EMetaHumanCharacterMoveToolManipulationGizmos::Rotate)
			{
				FVector3f Delta = FVector3f(CurrentDragTransform.GetValue().GetRotation().Euler());
				// make sure to only apply the delta rotation on the rotation axis that has been modified.
				if ((DraggedGizmoElements & ETransformGizmoSubElements::RotateAxisX) != ETransformGizmoSubElements::RotateAxisX) Delta[0] = 0;
				if ((DraggedGizmoElements & ETransformGizmoSubElements::RotateAxisY) != ETransformGizmoSubElements::RotateAxisY) Delta[1] = 0;
				if ((DraggedGizmoElements & ETransformGizmoSubElements::RotateAxisZ) != ETransformGizmoSubElements::RotateAxisZ) Delta[2] = 0;
				FVector3f NewRotation = GizmoConstraints.BeginDragGizmoRotation + Delta;
				const TArray<FVector3f> ManipulatorPositions = UMetaHumanCharacterEditorSubsystem::Get()->SetFaceGizmoRotation(MetaHumanCharacter, FaceStateChangeTransactor->GetBeginDragState(), SelectedGizmoManipulator, NewRotation, MeshEditingToolProperties->bSymmetricModeling, /*bInEnforceBounds*/ false);
				UpdateManipulatorPositions(ManipulatorPositions);
			}

			// Update Scale
			if (MoveProperties->GizmoType == EMetaHumanCharacterMoveToolManipulationGizmos::UniformScale)
			{
				float BaseScale = ManipulatorComponents[0]->GetRelativeScale3D().X;
				float Delta = CurrentDragTransform.GetValue().GetScale3D().X - BeginDragTransform.GetValue().GetScale3D().X;
				float NewScale = GizmoConstraints.BeginDragGizmoScale + Delta / BaseScale;
				NewScale = FMath::Clamp(NewScale, GizmoConstraints.MinGizmoScale, GizmoConstraints.MaxGizmoScale);
				const TArray<FVector3f> ManipulatorPositions = UMetaHumanCharacterEditorSubsystem::Get()->SetFaceGizmoScale(MetaHumanCharacter, FaceStateChangeTransactor->GetBeginDragState(), SelectedGizmoManipulator, NewScale, MeshEditingToolProperties->bSymmetricModeling, /*bInEnforceBounds*/ false);
				UpdateManipulatorPositions(ManipulatorPositions);
			}
			CurrentDragTransform.Reset();
		}
	}
}

void UMetaHumanCharacterEditorFaceMoveTool::OnClickPress(const FInputDeviceRay& InClickPos)
{
	if(MoveProperties->GizmoType == EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace)
	{
		Super::OnClickPress(InClickPos);
		SelectedGizmoManipulator = INDEX_NONE;
		TransformGizmo->SetVisibility(false);
	}
	else
	{
		if (SelectedManipulator != INDEX_NONE && MoveProperties->GizmoType != EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace)
		{
			if (!BeginDragTransform.IsSet())
			{
				if (!IsManipulatorOccluded(InClickPos.WorldRay))
				{
					SelectedGizmoManipulator = SelectedManipulator;
					TransformGizmo->ReinitializeGizmoTransform(ManipulatorComponents[SelectedGizmoManipulator]->GetComponentTransform());
					TransformGizmo->SetVisibility(true);
				}
			}
		}
		else
		{
			SelectedGizmoManipulator = INDEX_NONE;
			TransformGizmo->SetVisibility(false);
		}
	}
}

void UMetaHumanCharacterEditorFaceMoveTool::OnClickDrag(const FInputDeviceRay& InDragPos)
{
	if (MoveProperties->GizmoType == EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace)
	{
		Super::OnClickDrag(InDragPos);
	}
}

void UMetaHumanCharacterEditorFaceMoveTool::OnBeginDrag(const FRay& InRay)
{
	if (MoveProperties->GizmoType == EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace)
	{
		Super::OnBeginDrag(InRay);
	}
}

void UMetaHumanCharacterEditorFaceMoveTool::OnUpdateDrag(const FRay& InRay)
{
	if (MoveProperties->GizmoType == EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace)
	{
		Super::OnUpdateDrag(InRay);
	}
}

void UMetaHumanCharacterEditorFaceMoveTool::OnEndDrag(const FRay& InRay)
{
	if (MoveProperties->GizmoType == EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace)
	{
		Super::OnEndDrag(InRay);
	}
}

void UMetaHumanCharacterEditorFaceMoveTool::InitStateChangeTransactor()
{
	UFaceStateChangeTransactor* FaceStateChangeTransactor = NewObject<UFaceStateChangeTransactor>(this);
	if (FaceStateChangeTransactor && FaceStateChangeTransactor->GetClass()->ImplementsInterface(UMeshStateChangeTransactorInterface::StaticClass()))
	{
		MeshStateChangeTransactor.SetInterface(Cast<IMeshStateChangeTransactorInterface>(FaceStateChangeTransactor));
		MeshStateChangeTransactor.SetObject(FaceStateChangeTransactor);
	}
}

const FText UMetaHumanCharacterEditorFaceMoveTool::GetDescription() const
{
	return LOCTEXT("FaceMoveToolName", "Move");
}

const FText UMetaHumanCharacterEditorFaceMoveTool::GetCommandChangeDescription() const
{
	return LOCTEXT("FaceMoveToolCommandChange", "Face Move Tool");
}

const FText UMetaHumanCharacterEditorFaceMoveTool::GetCommandChangeIntermediateDescription() const
{
	return LOCTEXT("FaceMoveToolIntermediateCommandChange", "Move Face Gizmo");
}

UStaticMesh* UMetaHumanCharacterEditorFaceMoveTool::GetManipulatorMesh() const
{
	UStaticMesh* MoveManipulatorMesh = nullptr;

	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();
	if (!Settings->MoveManipulatorMesh.IsNull())
	{
		MoveManipulatorMesh = Settings->MoveManipulatorMesh.LoadSynchronous();
	}
	else
	{
		// Fallback to a simple sphere
		MoveManipulatorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Sphere.Sphere'"));
	}

	check(MoveManipulatorMesh);
	return MoveManipulatorMesh;
}

UMaterialInterface* UMetaHumanCharacterEditorFaceMoveTool::GetManipulatorMaterial() const
{
	UMaterialInterface* MoveManipulatorMaterial = nullptr;

	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();
	if (!Settings->MoveManipulatorMesh.IsNull())
	{
		MoveManipulatorMaterial = Settings->MoveManipulatorMesh->GetMaterial(0);
	}
	else
	{
		// Fallback to a simple material
		MoveManipulatorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Tools/M_MoveTool_Gizmo.M_MoveTool_Gizmo'"));
	}

	check(MoveManipulatorMaterial);
	return MoveManipulatorMaterial;
}

float UMetaHumanCharacterEditorFaceMoveTool::GetManipulatorScale() const
{
	const float GizmoScale = 0.0035f;
	return GizmoScale;
}

TArray<FVector3f> UMetaHumanCharacterEditorFaceMoveTool::GetManipulatorPositions() const
{
	return UMetaHumanCharacterEditorSubsystem::Get()->GetFaceGizmos(MetaHumanCharacter);
}

TArray<FVector3f> UMetaHumanCharacterEditorFaceMoveTool::TranslateManipulator(int32 InManipulatorIndex, const FVector3f& InDelta)
{
	// unused
	return UMetaHumanCharacterEditorSubsystem::Get()->GetFaceGizmos(MetaHumanCharacter);
}

void UMetaHumanCharacterEditorFaceMoveTool::SetGizmoType(EMetaHumanCharacterMoveToolManipulationGizmos InSelection)
{
	if (BeginDragTransform.IsSet())
	{
		return;
	}

	MoveProperties->GizmoType = InSelection;
	switch (MoveProperties->GizmoType)
	{
	case EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace:
		TransformGizmo->ActiveGizmoMode = EToolContextTransformGizmoMode::NoGizmo;
		SelectedGizmoManipulator = INDEX_NONE;
		break;
	case EMetaHumanCharacterMoveToolManipulationGizmos::Translate:
		TransformGizmo->ActiveGizmoMode = EToolContextTransformGizmoMode::Translation;
		break;
	case EMetaHumanCharacterMoveToolManipulationGizmos::Rotate:
		TransformGizmo->ActiveGizmoMode = EToolContextTransformGizmoMode::Rotation;
		break;
	case EMetaHumanCharacterMoveToolManipulationGizmos::UniformScale:
		TransformGizmo->ActiveGizmoMode = EToolContextTransformGizmoMode::Scale;
		break;
	}
}

void UMetaHumanCharacterEditorFaceMoveTool::RegisterActions(FInteractiveToolActionSet& ActionSet)
{
	Super::RegisterActions(ActionSet);

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 1201,
		TEXT("MoveToolSelectScreenSpace"),
		LOCTEXT("MoveToolSelectScreenSpace", "Select Screen Space Move Tool"),
		LOCTEXT("MoveToolSelectScreenSpaceTooltip", "Select Screen Space Move Tool"),
		EModifierKey::None, EKeys::Q,
		[this]() { SetGizmoType(EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 1202,
		TEXT("MoveToolSelectTranslate"),
		LOCTEXT("MoveToolSelectTranslate", "Select Translate Move Tool"),
		LOCTEXT("MoveToolSelectTranslateTooltip", "Select Translate Move Tool"),
		EModifierKey::None, EKeys::W,
		[this]() { SetGizmoType(EMetaHumanCharacterMoveToolManipulationGizmos::Translate); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 1203,
		TEXT("MoveToolSelectRotation"),
		LOCTEXT("MoveToolSelectRotation", "Select Rotate Move Tool"),
		LOCTEXT("MoveToolSelectRotationTooltip", "Select Rotate Move Tool"),
		EModifierKey::None, EKeys::E,
		[this]() { SetGizmoType(EMetaHumanCharacterMoveToolManipulationGizmos::Rotate); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 1204,
		TEXT("MoveToolScaleRotation"),
		LOCTEXT("MoveToolScaleRotation", "Select Scale Move Tool"),
		LOCTEXT("MoveToolScaleRotationTooltip", "Select Scale Move Tool"),
		EModifierKey::None, EKeys::R,
		[this]() { SetGizmoType(EMetaHumanCharacterMoveToolManipulationGizmos::UniformScale); });
}

// -----------------------------------------------------
// FaceSculptTool implementation -----------------------
// -----------------------------------------------------

void UMetaHumanCharacterEditorFaceSculptTool::Setup()
{
	Super::Setup();

	FViewport* Viewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	FMetaHumanCharacterViewportClient* MetaHumanCharacterViewportClient = static_cast<FMetaHumanCharacterViewportClient*>(Viewport->GetClient());
	if (MetaHumanCharacterViewportClient)
	{
		MetaHumanCharacterViewportClient->SetShortcuts({ {LOCTEXT("FaceSculptToolShortcutKey", "CTRL"), LOCTEXT("FaceSculptToolShortcutValue", "toggle markers")} });
	}
}

void UMetaHumanCharacterEditorFaceSculptTool::Shutdown(EToolShutdownType InShutdownType)
{
	Super::Shutdown(InShutdownType);

	FViewport* Viewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	FMetaHumanCharacterViewportClient* MetaHumanCharacterViewportClient = static_cast<FMetaHumanCharacterViewportClient*>(Viewport->GetClient());
	MetaHumanCharacterViewportClient->ClearShortcuts();
}

void UMetaHumanCharacterEditorFaceSculptTool::InitStateChangeTransactor()
{
	UFaceStateChangeTransactor* FaceStateChangeTransactor = NewObject<UFaceStateChangeTransactor>(this);
	if (FaceStateChangeTransactor && FaceStateChangeTransactor->GetClass()->ImplementsInterface(UMeshStateChangeTransactorInterface::StaticClass()))
	{
		MeshStateChangeTransactor.SetInterface(Cast<IMeshStateChangeTransactorInterface>(FaceStateChangeTransactor));
		MeshStateChangeTransactor.SetObject(FaceStateChangeTransactor);
	}
}

void UMetaHumanCharacterEditorFaceSculptTool::Render(IToolsContextRenderAPI* InRenderAPI)
{
	Super::Render(InRenderAPI);

	if (FPrimitiveDrawInterface* PDI = InRenderAPI->GetPrimitiveDrawInterface())
	{
		// See if there is a better way to check if FHitResult is valid
		if (HitVertexID != INDEX_NONE)
		{
			// TODO: Extent this to allow the user to set these properties using UInteractiveToolPropertySet's
			const FVector StartPoint = HitVertex;
			const FVector EndPoint = HitVertex + HitNormal * 5.0;
			const uint8 DepthPriorityGroup = 0;
			const float DepthBias = 0.0f;
			const float Thickness = 0.0f;
			const bool bScreenSpace = false;
			PDI->DrawLine(StartPoint,
				EndPoint,
				FLinearColor::Red,
				DepthPriorityGroup,
				Thickness,
				DepthBias,
				bScreenSpace);
		}

		for (int32 k = 0; k < DebugVertices.Num(); ++k)
		{
			FVector pt(DebugVertices[k].X, DebugVertices[k].Y, DebugVertices[k].Z);
			PDI->DrawPoint(pt, FLinearColor::Red, 1.0f, 0);
		}
	}
}

void UMetaHumanCharacterEditorFaceSculptTool::OnTick(float InDeltaTime)
{
	if (GetCtrlToggle())
	{
		// Superclass detects if the manipulator is being hit, and updates the SelectedManipulator index.
		if (SelectedManipulator == IndexConstants::InvalidID)
		{
			HitVertexID = UMetaHumanCharacterEditorSubsystem::Get()->SelectFaceVertex(MetaHumanCharacter, LastWorldRay, HitVertex, HitNormal);			
		}

		// Update the manipulators hover state to red because it is potentially being deleted.
		for (int32 ManipulatorIndex = 0; ManipulatorIndex < ManipulatorComponents.Num(); ++ManipulatorIndex)
		{
			const bool bIsSelected = ManipulatorIndex == SelectedManipulator;
			// Some other UI indication for removing landmark would be suitable here.
			SetManipulatorMarkedState(ManipulatorIndex, bIsSelected);
		}
	}
	else
	{
		for (int32 ManipulatorIndex = 0; ManipulatorIndex < ManipulatorComponents.Num(); ++ManipulatorIndex)
		{
			SetManipulatorMarkedState(ManipulatorIndex, false);
		}
		CtrlToggledOnBeginDrag = false;
		HitVertexID = INDEX_NONE;
		Super::OnTick(InDeltaTime);
	}

	DebugVertices.Reset();
	if (CVarMHCharacterShowSculptingVertices.GetValueOnAnyThread())
	{
		DebugVertices = UMetaHumanCharacterEditorSubsystem::Get()->GetFaceState(MetaHumanCharacter)->Evaluate().Vertices;
		for (int32 Idx = 0; Idx < DebugVertices.Num(); ++Idx)
		{
			DebugVertices[Idx] = FVector3f(DebugVertices[Idx].X, DebugVertices[Idx].Z, DebugVertices[Idx].Y);
		}
	}
}

bool UMetaHumanCharacterEditorFaceSculptTool::HitTest(const FRay& InRay, FHitResult& OutHit)
{
	if (Super::HitTest(InRay, OutHit))
	{
		HitVertexID = INDEX_NONE;
		return true;
	}
	return (HitVertexID != INDEX_NONE);
}

void UMetaHumanCharacterEditorFaceSculptTool::OnBeginDrag(const FRay& InRay)
{
	CtrlToggledOnBeginDrag = GetCtrlToggle();

	if (!CtrlToggledOnBeginDrag)
	{
		Super::OnBeginDrag(InRay);
		return;
	}

	FHitResult HitResult;
	if (HitTest(InRay, HitResult))
	{
		// Stores the face state when the drag start to allow it to be undone while the tool is active
		MeshStateChangeTransactor->StoreBeginDragState(MetaHumanCharacter);
	}
}

void UMetaHumanCharacterEditorFaceSculptTool::OnUpdateDrag(const FRay& InRay)
{
	if (!CtrlToggledOnBeginDrag)
	{
		Super::OnUpdateDrag(InRay);
		return;
	}
}

void UMetaHumanCharacterEditorFaceSculptTool::OnEndDrag(const FRay& InRay)
{
	if (CtrlToggledOnBeginDrag)
	{
		if (SelectedManipulator != INDEX_NONE)
		{
			// First check if Manipulator is selected and remove it.
			if (ManipulatorComponents.IsValidIndex(SelectedManipulator))
			{
				GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>()->RemoveFaceLandmark(MetaHumanCharacter, SelectedManipulator);

				const TArray<FVector3f> ManipulatorPositions = GetManipulatorPositions();
				// Because of the symmetry it is possible that 2 landmarks are removed at the same time, so manipulators components will be reset an recreated.
				RecreateManipulators(ManipulatorPositions);
			}
		}
		else if (HitVertexID != INDEX_NONE)
		{
			GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>()->AddFaceLandmark(MetaHumanCharacter, HitVertexID);

			// Get the current number of landmarks since CreateManipulator changes the array
			const int32 NumLandmarks = ManipulatorComponents.Num();

			// Iterate over the newly added landmarks to create new manipulators
			const TArray<FVector3f> LandmarkPositions = GetManipulatorPositions();
			for (int32 NewLandmarkIndex = NumLandmarks; NewLandmarkIndex < LandmarkPositions.Num(); ++NewLandmarkIndex)
			{
				CreateManipulator(LandmarkPositions[NewLandmarkIndex]);
			}
		}
	}

	// After adding or removing a landmark the base class will create a command that can undo
	// the changes including adding or removing landmarks
	Super::OnEndDrag(InRay);
}

void UMetaHumanCharacterEditorFaceSculptTool::OnCancelDrag()
{
	SelectedManipulator = INDEX_NONE;
	HitVertexID = INDEX_NONE;
}

const FText UMetaHumanCharacterEditorFaceSculptTool::GetDescription() const
{
	return LOCTEXT("FaceSculptToolName", "Sculpt");
}

const FText UMetaHumanCharacterEditorFaceSculptTool::GetCommandChangeDescription() const
{
	return LOCTEXT("FaceSculptToolCommandChange", "Face Sculpt Tool");
}

const FText UMetaHumanCharacterEditorFaceSculptTool::GetCommandChangeIntermediateDescription() const
{
	if (CtrlToggledOnBeginDrag)
	{
		return LOCTEXT("FaceSculptToolCommandAddRemoveLandmarkChange", "Change Face Landmarks");
	}
	else
	{
		return LOCTEXT("FaceSculptToolCommandIntermediateChange", "Move Face Landmark");
	}
}

UStaticMesh* UMetaHumanCharacterEditorFaceSculptTool::GetManipulatorMesh() const
{
	UStaticMesh* SculptManipulatorMesh = nullptr;

	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();
	if (!Settings->SculptManipulatorMesh.IsNull())
	{
		SculptManipulatorMesh = Settings->SculptManipulatorMesh.LoadSynchronous();
	}
	else
	{
		// Fallback to a simple sphere
		SculptManipulatorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Sphere.Sphere'"));
	}

	check(SculptManipulatorMesh);
	return SculptManipulatorMesh;
}

UMaterialInterface* UMetaHumanCharacterEditorFaceSculptTool::GetManipulatorMaterial() const
{
	UMaterialInterface* SculptManipulatorMaterial = nullptr;

	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();
	if (!Settings->SculptManipulatorMesh.IsNull())
	{
		SculptManipulatorMaterial = Settings->SculptManipulatorMesh->GetMaterial(0);
	}
	else
	{
		// Fallback to a simple material
		SculptManipulatorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Tools/M_MoveTool_Gizmo.M_MoveTool_Gizmo'"));
	}

	check(SculptManipulatorMaterial);
	return SculptManipulatorMaterial;
}

float UMetaHumanCharacterEditorFaceSculptTool::GetManipulatorScale() const
{
	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();
	const float GizmoScale = Settings->SculptManipulatorMesh.IsNull() ? 0.004f : 0.0017f;
	return GizmoScale * MeshEditingToolProperties->Size;
}

TArray<FVector3f> UMetaHumanCharacterEditorFaceSculptTool::GetManipulatorPositions() const
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	return MetaHumanCharacterEditorSubsystem->GetFaceLandmarks(MetaHumanCharacter);
}

TArray<FVector3f> UMetaHumanCharacterEditorFaceSculptTool::TranslateManipulator(int32 InGizmoIndex, const FVector3f& InDelta)
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	TSharedRef<FMetaHumanCharacterIdentity::FState> BeginDragState = Cast<UFaceStateChangeTransactor>(MeshStateChangeTransactor.GetObject())->GetBeginDragState();
	return MetaHumanCharacterEditorSubsystem->TranslateFaceLandmark(MetaHumanCharacter, BeginDragState, SelectedManipulator, InDelta, MeshEditingToolProperties->bSymmetricModeling);
}

// -----------------------------------------------------
// FaceBlendTool implementation ------------------------
// -----------------------------------------------------

void UMetaHumanCharacterEditorFaceBlendTool::InitStateChangeTransactor()
{
	UFaceStateChangeTransactor* FaceStateChangeTransactor = NewObject<UFaceStateChangeTransactor>(this);
	if (FaceStateChangeTransactor && FaceStateChangeTransactor->GetClass()->ImplementsInterface(UMeshStateChangeTransactorInterface::StaticClass()))
	{
		MeshStateChangeTransactor.SetInterface(Cast<IMeshStateChangeTransactorInterface>(FaceStateChangeTransactor));
		MeshStateChangeTransactor.SetObject(FaceStateChangeTransactor);
	}
}

void UMetaHumanCharacterEditorFaceBlendTool::Setup()
{
	Super::Setup();
	
	BlendProperties = NewObject<UMetaHumanCharacterEditorFaceBlendToolProperties>(this);
	AddToolPropertySource(BlendProperties);

	BlendProperties->RestoreProperties(this, GetCommandChangeDescription().ToString());
}

void UMetaHumanCharacterEditorFaceBlendTool::Shutdown(EToolShutdownType InShutdownType)
{
	Super::Shutdown(InShutdownType);

	BlendProperties->SaveProperties(this, GetCommandChangeDescription().ToString());
}

const FText UMetaHumanCharacterEditorFaceBlendTool::GetDescription() const
{
	return LOCTEXT("FaceBlendToolName", "Blend");
}

const FText UMetaHumanCharacterEditorFaceBlendTool::GetCommandChangeDescription() const
{
	return LOCTEXT("FaceBlendToolCommandChange", "Face Blend Tool");
}

const FText UMetaHumanCharacterEditorFaceBlendTool::GetCommandChangeIntermediateDescription() const
{ 
	return LOCTEXT("FaceBlendToolIntermediateCommandChange", "Move Face Blend Manipulator");
}

TArray<FVector3f> UMetaHumanCharacterEditorFaceBlendTool::GetManipulatorPositions() const
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	return MetaHumanCharacterEditorSubsystem->GetFaceGizmos(MetaHumanCharacter);
}

TArray<FVector3f> UMetaHumanCharacterEditorFaceBlendTool::BlendPresets(int32 InManipulatorIndex, const TArray<float>& Weights)
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	TSharedRef<const FMetaHumanCharacterIdentity::FState> BeginDragState = Cast<UFaceStateChangeTransactor>(MeshStateChangeTransactor.GetObject())->GetBeginDragState();	
	const UMetaHumanCharacterEditorFaceBlendToolProperties* FaceBlendProperties = Cast<UMetaHumanCharacterEditorFaceBlendToolProperties>(BlendProperties);
	return MetaHumanCharacterEditorSubsystem->BlendFaceRegion(MetaHumanCharacter, InManipulatorIndex, BeginDragState, PresetStates, Weights, FaceBlendProperties->BlendOptions, MeshEditingToolProperties->bSymmetricModeling);
}

void UMetaHumanCharacterEditorFaceBlendTool::AddMetaHumanCharacterPreset(UMetaHumanCharacter* InCharacterPreset, int32 InItemIndex)
{
	UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	TSharedPtr<FMetaHumanCharacterIdentity::FState> PresetState = Subsystem->CopyFaceState(MetaHumanCharacter);
	PresetState->Deserialize(InCharacterPreset->GetFaceStateData());
	FMetaHumanCharacterIdentity::FSettings Settings = PresetState->GetSettings();
	Settings.SetGlobalVertexDeltaScale(MetaHumanCharacter->FaceEvaluationSettings.GlobalDelta);
	if (PresetStates.Num() <= InItemIndex)
	{
		PresetStates.AddDefaulted(InItemIndex - PresetStates.Num() + 1);
	}
	PresetStates[InItemIndex] = PresetState;
}

void UMetaHumanCharacterEditorFaceBlendTool::RemoveMetaHumanCharacterPreset(int32 InItemIndex)
{
	if (InItemIndex < PresetStates.Num())
	{
		PresetStates[InItemIndex].Reset();
	}
}

void UMetaHumanCharacterEditorFaceBlendTool::BlendToMetaHumanCharacterPreset(UMetaHumanCharacter* InCharacterPreset)
{
	MeshStateChangeTransactor->StoreBeginDragState(MetaHumanCharacter);

	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	const UMetaHumanCharacterEditorFaceBlendToolProperties* FaceBlendProperties = Cast<UMetaHumanCharacterEditorFaceBlendToolProperties>(BlendProperties);
	TSharedPtr<const FMetaHumanCharacterIdentity::FState> InitState = MetaHumanCharacterEditorSubsystem->CopyFaceState(MetaHumanCharacter);
	TSharedPtr<FMetaHumanCharacterIdentity::FState> State = MetaHumanCharacterEditorSubsystem->CopyFaceState(MetaHumanCharacter);
	State->Deserialize(InCharacterPreset->GetFaceStateData());
	TArray<TSharedPtr<const FMetaHumanCharacterIdentity::FState>> States = { State };
	TArray<float> Weights = { 1.0f };
	TArray<FVector3f> ManipulatorPositions = MetaHumanCharacterEditorSubsystem->BlendFaceRegion(MetaHumanCharacter, -1, InitState, States, Weights, FaceBlendProperties->BlendOptions, /*bInBlendSymmetrically*/ true);
	UpdateManipulatorPositions(ManipulatorPositions);

	MeshStateChangeTransactor->CommitEndDragState(GetToolManager(), MetaHumanCharacter, GetCommandChangeIntermediateDescription());
}

#undef LOCTEXT_NAMESPACE

