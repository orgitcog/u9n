// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorMeshEditingTools.h"
#include "MetaHumanCharacterEditorToolTargetUtil.h"
#include "MetaHumanCharacterEditorLog.h"
#include "MetaHumanCharacterEditorViewportClient.h"
#include "MetaHumanCharacterEditorToolCommandChange.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolObjects.h"
#include "TargetInterfaces/MeshTargetInterfaceTypes.h"
#include "DynamicMesh/MeshNormals.h"
#include "EditorViewportClient.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "SceneView.h"
#include "RBF/RBFSolver.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "BaseBehaviors/SingleClickOrDragBehavior.h"
#include "ContextObjectStore.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

FMetaHumanCharacterEditorFaceToolCommandChange::FMetaHumanCharacterEditorFaceToolCommandChange(
	TSharedRef<const FMetaHumanCharacterIdentity::FState> InOldState,
	TNotNull<UMetaHumanCharacter*> InCharacter,
	TNotNull<UInteractiveToolManager*> InToolManager)
	: FMetaHumanCharacterEditorToolCommandChange(InToolManager)
	, OldState{ InOldState }
	, NewState{ GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>()->CopyFaceState(InCharacter) }
{
}

void FMetaHumanCharacterEditorFaceToolCommandChange::Apply(UObject* InObject)
{
	UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);
	GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>()->CommitFaceState(Character, NewState);
}

void FMetaHumanCharacterEditorFaceToolCommandChange::Revert(UObject* InObject)
{
	UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);
	GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>()->CommitFaceState(Character, OldState);
}


bool UMetaHumanCharacterEditorMeshEditingTool::HitTest(const FRay& InRay, FHitResult& OutHit)
{
	const FVector StartPoint = InRay.Origin;
	const FVector EndPoint = InRay.PointAt(HALF_WORLD_MAX);

	SelectedManipulator = INDEX_NONE;
	float Distance = -1;

	// Simple loop to test if one of the manipulators was hit by the mouse
	for (int32 ManipulatorIndex = 0; ManipulatorIndex < ManipulatorComponents.Num(); ++ManipulatorIndex)
	{
		UStaticMeshComponent* ManipulatorComponent = ManipulatorComponents[ManipulatorIndex];

		const bool bTraceComplex = false;
		if (ManipulatorComponent->LineTraceComponent(OutHit, StartPoint, EndPoint, FCollisionQueryParams(SCENE_QUERY_STAT(HitTest), bTraceComplex)))
		{
			// Store the index of the manipulator that was hit and the hit distance, which is
			// used to calculate the movement delta of the gizmo translation
			if(Distance == -1 || OutHit.Distance < Distance)
			{
				SelectedManipulator = ManipulatorIndex;
			}
		}
	}

	// Return a hit if a manipulator was selected
	return SelectedManipulator != INDEX_NONE;
}

void UMetaHumanCharacterEditorMeshEditingTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(GetDescription());

	MeshEditingToolProperties = NewObject<UMetaHumanCharacterEditorMeshEditingToolProperties>(this);
	AddToolPropertySource(MeshEditingToolProperties);
	MeshEditingToolProperties->RestoreProperties(this, GetCommandChangeDescription().ToString());
	MeshEditingToolProperties->WatchProperty(MeshEditingToolProperties->Size,
											 [this](float)
											 {
												UpdateManipulatorsScale();
											 });

	// Store the Actor
	MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);

	// Initialize state change transactor
	InitStateChangeTransactor();

	DelegateHandle = MeshStateChangeTransactor->GetStateChangedDelegate(MetaHumanCharacter).AddWeakLambda(this, [this]
	{
		const TArray<FVector3f> Positions = GetManipulatorPositions();
		if (ManipulatorComponents.Num() != Positions.Num())
		{
			// This can occur when landmarks are removed and then the user cancels.
			RecreateManipulators(Positions);
		}
		else
		{
			UpdateManipulatorPositions(Positions);
		}
	});

	// Spawn an actor used as a container for the manipulator components
	ManipulatorsActor = GetTargetWorld()->SpawnActor<AInternalToolFrameworkActor>(FVector::ZeroVector, FRotator::ZeroRotator);

	const TArray<FVector3f> ManipulatorPositions = GetManipulatorPositions();
	for (const FVector3f& ManipulatorPosition : ManipulatorPositions)
	{
		CreateManipulator(ManipulatorPosition);
	}
}

UStaticMeshComponent* UMetaHumanCharacterEditorMeshEditingTool::CreateManipulator(const FVector3f& InPosition)
{
	// Load the mesh to be used as the manipulation landmark
	UStaticMesh* ManipulatorMesh  = GetManipulatorMesh();
	check(ManipulatorMesh );

	// Use different material for landmarks
	UMaterialInterface* ManipulatorMaterial = GetManipulatorMaterial();
	check(ManipulatorMaterial);

	const float GizmoScale = GetManipulatorScale();

	UStaticMeshComponent* ManipulatorComponent = NewObject<UStaticMeshComponent>(ManipulatorsActor);
	ManipulatorComponent->SetStaticMesh(ManipulatorMesh );
	ManipulatorComponent->SetWorldScale3D(FVector{ GizmoScale * MeshEditingToolProperties->Size});
	ManipulatorComponent->SetWorldLocation(FVector{ InPosition });
	ManipulatorComponent->SetCastShadow(false);
	ManipulatorComponent->SetupAttachment(ManipulatorsActor->GetRootComponent());
	ManipulatorComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, ManipulatorMaterial);
	ManipulatorComponent->RegisterComponent();

	ManipulatorComponents.Add(ManipulatorComponent);
	return ManipulatorComponent;
}

void UMetaHumanCharacterEditorMeshEditingTool::RecreateManipulators(const TArray<FVector3f>& InManipulatorPositions)
{
	if (ManipulatorsActor)
	{
		for (UStaticMeshComponent* ManipulatorComponent : ManipulatorComponents)
		{
			if (ManipulatorComponent)
			{
				ManipulatorComponent->UnregisterComponent();
				ManipulatorComponent->DestroyComponent();
			}
		}
		ManipulatorComponents.Empty();

		for (const FVector3f& LandmarkPosition : InManipulatorPositions)
		{
			CreateManipulator(LandmarkPosition);
		}
	}
}

void UMetaHumanCharacterEditorMeshEditingTool::UpdateManipulatorPositions(const TArray<FVector3f>& InPositions)
{
	// Update the UI manipulator positions to reflect the changes in the model
	for (int32 ManipulatorIndex = 0; ManipulatorIndex < InPositions.Num(); ++ManipulatorIndex)
	{
		// Check if the index is valid here. It should always be since the number of regions is fixed
		check(ManipulatorComponents.IsValidIndex(ManipulatorIndex));

		ManipulatorComponents[ManipulatorIndex]->SetWorldLocation(FVector(InPositions[ManipulatorIndex]));
	}
}

void UMetaHumanCharacterEditorMeshEditingTool::UpdateManipulatorPositions()
{
	UpdateManipulatorPositions(GetManipulatorPositions());
}

void UMetaHumanCharacterEditorMeshEditingTool::Shutdown(EToolShutdownType InShutdownType)
{
	MeshStateChangeTransactor->CommitShutdownState(GetToolManager(), MetaHumanCharacter, InShutdownType, GetCommandChangeDescription());

	MeshEditingToolProperties->SaveProperties(this, GetCommandChangeDescription().ToString());

	if (ManipulatorsActor != nullptr)
	{
		ManipulatorsActor->Destroy();
		ManipulatorsActor = nullptr;
	}

	MeshStateChangeTransactor->GetStateChangedDelegate(MetaHumanCharacter).Remove(DelegateHandle);
	DelegateHandle.Reset();

	FViewport* Viewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	FMetaHumanCharacterViewportClient* MetaHumanCharacterViewportClient = static_cast<FMetaHumanCharacterViewportClient*>(Viewport->GetClient());
	if (MetaHumanCharacterViewportClient)
	{
		MetaHumanCharacterViewportClient->ClearShortcuts();
	}

}

void UMetaHumanCharacterEditorMeshEditingTool::OnTick(float InDeltaTime)
{
	// Update the manipulators hover state
	for (int32 ManipulatorIndex = 0; ManipulatorIndex < ManipulatorComponents.Num(); ++ManipulatorIndex)
	{
		const bool bIsSelected = ManipulatorIndex == SelectedManipulator;
		SetManipulatorHoverState(ManipulatorIndex, bIsSelected);
	}

	if (!PendingMoveDelta.IsZero() && SelectedManipulator != INDEX_NONE)
	{
		BeginDragMoveDelta += PendingMoveDelta;

		// Translate the manipulator and Update the Face Mesh
		const TArray<FVector3f> ManipulatorPositions = TranslateManipulator(SelectedManipulator, BeginDragMoveDelta * MeshEditingToolProperties->Speed);

		UpdateManipulatorPositions(ManipulatorPositions);

		PendingMoveDelta = FVector3f::ZeroVector;
	}
}

void UMetaHumanCharacterEditorMeshEditingTool::OnClickPress(const FInputDeviceRay& InClickPos)
{
	Super::OnClickPress(InClickPos);

	// Store the initial pixel position that the user clicked
	OldPixelPos = InClickPos.ScreenPosition;
}

void UMetaHumanCharacterEditorMeshEditingTool::OnClickDrag(const FInputDeviceRay& InDragPos)
{
	// Store the new pixel position to be used in OnUpdateDrag
	NewPixelPos = InDragPos.ScreenPosition;

	// This will call OnUpdateDrag where the marker movement delta is calculated
	Super::OnClickDrag(InDragPos);

	// Update the pixel position
	OldPixelPos = NewPixelPos;
}

void UMetaHumanCharacterEditorMeshEditingTool::OnBeginDrag(const FRay& InRay)
{
	if (SelectedManipulator == INDEX_NONE)
	{
		return;
	}

	SetManipulatorDragState(SelectedManipulator, true);

	MeshStateChangeTransactor->StoreBeginDragState(MetaHumanCharacter);

	BeginDragMoveDelta = FVector3f::ZeroVector;
	PendingMoveDelta = FVector3f::ZeroVector;

	if (MeshEditingToolProperties->bHideWhileDragging)
	{
		for (int32 ManipulatorIndex = 0; ManipulatorIndex < ManipulatorComponents.Num(); ++ManipulatorIndex)
		{
			const bool bIsSelected = ManipulatorIndex == SelectedManipulator;
			ManipulatorComponents[ManipulatorIndex]->SetVisibility(bIsSelected);
		}
	}
}

void UMetaHumanCharacterEditorMeshEditingTool::OnUpdateDrag(const FRay& InRay)
{
	if (SelectedManipulator != INDEX_NONE && ManipulatorComponents.IsValidIndex(SelectedManipulator))
	{
		UStaticMeshComponent* ManipulatorComponent = ManipulatorComponents[SelectedManipulator];
		const FVector ManipulatorLocation = ManipulatorComponent->GetComponentLocation();
		SetManipulatorDragState(SelectedManipulator, true);

		// FSceneView is the struct that contains all the information about viewport.
		// It allows access the underlying matrices used for projection math. Note
		// that FSceneView objects are destroyed when owner FSceneViewFamilyContext
		// goes out of scope

		WithSceneView([this, &ManipulatorLocation](FSceneView* View)
					  {
						  // Calculates the delta to move a manipulator. The main idea is to calculate the
						  // delta in screen space and then project the delta back to the world in order
						  // to move the manipulator in screen space

						  // Projects the Marker location to screen space and performs the homogeneous division
						  // This divides XYZ by W but keeps W unchanged
						  const FPlane ManipScreenPos = View->Project(ManipulatorLocation);

						  // Converts the pixel locations to screen space and apply the perspective depth W
						  // of the marker location so that when unprojected they will have the same depth
						  FVector4 OldScreenPos = View->PixelToScreen(OldPixelPos.X, OldPixelPos.Y, ManipScreenPos.Z);
						  FVector4 NewScreenPos = View->PixelToScreen(NewPixelPos.X, NewPixelPos.Y, ManipScreenPos.Z);

						  OldScreenPos *= ManipScreenPos.W;
						  NewScreenPos *= ManipScreenPos.W;

						  // Project the screen positions back world
						  const FVector4 ProjectedOldPos = View->ScreenToWorld(OldScreenPos);
						  const FVector4 ProjectedNewPos = View->ScreenToWorld(NewScreenPos);

						  // Calculate the delta movement in world space
						  const FVector WorldDelta = ProjectedNewPos - ProjectedOldPos;

						  // Accumulate the calculated delta since multiple drag updates can happen between ticks
						  PendingMoveDelta += FVector3f(WorldDelta);
					  });
	}
}

void UMetaHumanCharacterEditorMeshEditingTool::OnEndDrag(const FRay& InRay)
{
	if (SelectedManipulator == INDEX_NONE)
	{
		return;
	}

	if (ManipulatorComponents.IsValidIndex(SelectedManipulator))
	{
		SetManipulatorDragState(SelectedManipulator, false);
		SelectedManipulator = INDEX_NONE;
	}

	// Restore the visibility of all manipulators
	for (int32 ManipulatorIndex = 0; ManipulatorIndex < ManipulatorComponents.Num(); ++ManipulatorIndex)
	{
		ManipulatorComponents[ManipulatorIndex]->SetVisibility(true);
	}

	// Reset the pending move delta to avoid OnTick moving manipulators that were not selected in BeginDragTranslateManipulator
	PendingMoveDelta = FVector3f::ZeroVector;

	MeshStateChangeTransactor->CommitEndDragState(GetToolManager(), MetaHumanCharacter, GetCommandChangeIntermediateDescription());
}

void UMetaHumanCharacterEditorMeshEditingTool::SetManipulatorDragState(int32 InManipulatorIndex, bool bInIsDragging)
{
	check(ManipulatorComponents.IsValidIndex(InManipulatorIndex));
	UMaterialInstanceDynamic* ManipulatorMaterialInstance = CastChecked<UMaterialInstanceDynamic>(ManipulatorComponents[InManipulatorIndex]->GetMaterial(0));
	ManipulatorMaterialInstance->SetScalarParameterValue(TEXT("Drag"), bInIsDragging ? 1.0f : 0.0f);
}

void UMetaHumanCharacterEditorMeshEditingTool::SetManipulatorHoverState(int32 InManipulatorIndex, bool bInIsHovering)
{
	check(ManipulatorComponents.IsValidIndex(InManipulatorIndex));
	UMaterialInstanceDynamic* ManipulatorMaterialInstance = CastChecked<UMaterialInstanceDynamic>(ManipulatorComponents[InManipulatorIndex]->GetMaterial(0));
	ManipulatorMaterialInstance->SetScalarParameterValue(TEXT("Hover"), bInIsHovering ? 1.0f : 0.0f);
}

void UMetaHumanCharacterEditorMeshEditingTool::SetManipulatorMarkedState(int32 InManipulatorIndex, bool bInIsDragging)
{
	check(ManipulatorComponents.IsValidIndex(InManipulatorIndex));
	UMaterialInstanceDynamic* ManipulatorMaterialInstance = CastChecked<UMaterialInstanceDynamic>(ManipulatorComponents[InManipulatorIndex]->GetMaterial(0));
	ManipulatorMaterialInstance->SetScalarParameterValue(TEXT("Marked"), bInIsDragging ? 1.0f : 0.0f);
}

void UMetaHumanCharacterEditorMeshEditingTool::UpdateManipulatorsScale()
{
	for (UStaticMeshComponent* Component : ManipulatorComponents)
	{
		Component->SetWorldScale3D(FVector{ GetManipulatorScale() * MeshEditingToolProperties->Size });
	}
}

const FText UMetaHumanCharacterEditorMeshEditingTool::GetDescription() const
{
	return LOCTEXT("BaseMeshEditingTool", "Mesh Editing");
}

void UMetaHumanCharacterEditorMeshEditingTool::WithSceneView(TFunction<void(FSceneView*)> InCallback) const
{
	FViewport* Viewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(Viewport->GetClient());

	FSceneViewFamily::ConstructionValues SceneViewFamilyArgs(Viewport,
															 ViewportClient->GetScene(),
															 ViewportClient->EngineShowFlags);
	SceneViewFamilyArgs.SetRealtimeUpdate(ViewportClient->IsRealtime());
	FSceneViewFamilyContext ViewFamily(SceneViewFamilyArgs);

	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	InCallback(View);
}

bool UMetaHumanCharacterEditorMeshEditingTool::IsManipulatorOccluded(const FRay& InRay) const
{
	if (SelectedManipulator == INDEX_NONE)
	{
		return true;
	}
	// check if mesh is occluding the selected manipulator 
	FVector HitVertex;
	FVector HitNormal;
	int32 HitVertexID = UMetaHumanCharacterEditorSubsystem::Get()->SelectFaceVertex(MetaHumanCharacter, InRay, HitVertex, HitNormal);
	if (HitVertexID >= 0)
	{
		FVector3f ManipulatorPosition = GetManipulatorPositions()[SelectedManipulator];
		float Threshold = 1.0f;
		if ((HitVertex - InRay.Origin).Length() + Threshold < (FVector(ManipulatorPosition.X, ManipulatorPosition.Y, ManipulatorPosition.Z) - InRay.Origin).Length())
		{
			return true;
		}
	}
	return false;
}

// -----------------------------------------------------
// FaceTool implementation -----------------------
// -----------------------------------------------------

void UMetaHumanCharacterEditorFaceTool::Setup()
{
	Super::Setup();

	FaceToolHeadParameterProperties = NewObject<UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties>(this);
	AddToolPropertySource(FaceToolHeadParameterProperties);
	FaceToolHeadParameterProperties->RestoreProperties(this, GetCommandChangeDescription().ToString());

	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);
	FaceToolHeadParameterProperties->CopyFrom(Character->FaceEvaluationSettings);

	PreviousFaceEvaluationSettings = Character->FaceEvaluationSettings;

	// Bind to the ValueSet event of the Blend Properties to fill in the undo stack
	GetFaceToolHeadParameterProperties()->OnPropertyValueSetDelegate.BindWeakLambda(this, [this](const FPropertyChangedEvent& PropertyChangedEvent)
		{
			if (UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target))
			{
				// update the face settings only if they differ
				FMetaHumanCharacterFaceEvaluationSettings NewFaceEvaluationSettings = Character->FaceEvaluationSettings;
				GetFaceToolHeadParameterProperties()->CopyTo(NewFaceEvaluationSettings);

				if (Character->FaceEvaluationSettings == NewFaceEvaluationSettings)
				{
					return;
				}

				UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
				check(Subsystem);

				if ((PropertyChangedEvent.ChangeType & (EPropertyChangeType::ValueSet | EPropertyChangeType::ResetToDefault)) != 0u && ((PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive) == 0u))
				{
					Subsystem->CommitFaceEvaluationSettings(Character, NewFaceEvaluationSettings);

					FOnSettingsUpdateDelegate OnSettingsUpdateDelegate;
					OnSettingsUpdateDelegate.BindWeakLambda(this, [this](TWeakObjectPtr<UInteractiveToolManager> ToolManager, const FMetaHumanCharacterFaceEvaluationSettings& FaceEvaluationSettings)
						{
							UpdateFaceToolHeadParameterProperties(ToolManager, FaceEvaluationSettings);
							UpdateManipulatorPositions();
						});

					TUniquePtr<FMetaHumanCharacterEditorFaceEvaluationSettingsCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorFaceEvaluationSettingsCommandChange>(Character, PreviousFaceEvaluationSettings, OnSettingsUpdateDelegate, GetToolManager());
					GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("FaceEditingToolHeadParametersChange", "Face Editing Tool Head Parameters"));
					PreviousFaceEvaluationSettings = NewFaceEvaluationSettings;
				}
				else
				{
					Subsystem->ApplyFaceEvaluationSettings(Character, NewFaceEvaluationSettings);
				}

				UpdateManipulatorPositions();
			}
		});
}

void UMetaHumanCharacterEditorFaceTool::Shutdown(EToolShutdownType InShutdownType)
{
	Super::Shutdown(InShutdownType);

	FaceToolHeadParameterProperties->SaveProperties(this, GetCommandChangeDescription().ToString());
}

void UMetaHumanCharacterEditorFaceTool::UpdateFaceToolHeadParameterProperties(TWeakObjectPtr<UInteractiveToolManager> ToolManager, const FMetaHumanCharacterFaceEvaluationSettings& FaceEvaluationSettings)
{
	if (ToolManager.IsValid())
	{
		if (UMetaHumanCharacterEditorFaceTool* FaceTool = Cast<UMetaHumanCharacterEditorFaceTool>(ToolManager->GetActiveTool(EToolSide::Left)))
		{
			UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties* HeadParameterProperties = nullptr;
			if (FaceTool->GetToolProperties().FindItemByClass<UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties>(&HeadParameterProperties))
			{
				HeadParameterProperties->CopyFrom(FaceEvaluationSettings);
				HeadParameterProperties->SilentUpdateWatched();

				// Restore the PreviousSkinSettings of the tool to what we are applying so that
				// new commands are created with the correct previous settings
				PreviousFaceEvaluationSettings = FaceEvaluationSettings;
			}
		}
	}
}


void UMetaHumanCharacterEditorFaceTool::ResetFace()
{
	UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();

	TSharedPtr<const FMetaHumanCharacterIdentity::FState> CurrState = Subsystem->GetFaceState(MetaHumanCharacter);
	TSharedPtr<FMetaHumanCharacterIdentity::FState> NewState = Subsystem->CopyFaceState(MetaHumanCharacter);
	NewState->Reset();
	Subsystem->ApplyFaceState(MetaHumanCharacter, NewState.ToSharedRef());

	UpdateManipulatorPositions();

	TUniquePtr<FMetaHumanCharacterEditorFaceToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorFaceToolCommandChange>(CurrState.ToSharedRef(), MetaHumanCharacter, GetToolManager());
	GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), LOCTEXT("ResetFaceCommandChange", "Reset Face"));
}

void UMetaHumanCharacterEditorFaceTool::ResetFaceNeck()
{
	UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();

	TSharedPtr<const FMetaHumanCharacterIdentity::FState> CurrState = Subsystem->GetFaceState(MetaHumanCharacter);
	TSharedPtr<FMetaHumanCharacterIdentity::FState> NewState = Subsystem->CopyFaceState(MetaHumanCharacter);

	NewState->ResetNeckRegion();
	Subsystem->ApplyFaceState(MetaHumanCharacter, NewState.ToSharedRef());

	UpdateManipulatorPositions();

	TUniquePtr<FMetaHumanCharacterEditorFaceToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorFaceToolCommandChange>(CurrState.ToSharedRef(), MetaHumanCharacter, GetToolManager());
	GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), LOCTEXT("ResetFaceNeckCommandChange", "Reset Face Neck"));
}

// -----------------------------------------------------
// Mesh blend tool implementation ----------------------
// -----------------------------------------------------

void UMetaHumanCharacterEditorMeshBlendTool::Setup()
{
	Super::Setup();

	FViewport* Viewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	FMetaHumanCharacterViewportClient* MetaHumanCharacterViewportClient = static_cast<FMetaHumanCharacterViewportClient*>(Viewport->GetClient());
	if (MetaHumanCharacterViewportClient)
	{
		MetaHumanCharacterViewportClient->SetShortcuts({ {LOCTEXT("MeshBlendToolShortcutKey", "SHIFT"), LOCTEXT("MeshBlendToolShortcutValue", "blend all features")} });
	}
}

void UMetaHumanCharacterEditorMeshBlendTool::OnBeginDrag(const FRay& InRay)
{
	if (SelectedManipulator == INDEX_NONE)
	{
		return;
	}

	// check if mesh is occluding the selected manipulator 
	if (IsManipulatorOccluded(InRay))
	{
		SelectedManipulator = INDEX_NONE;
		return;
	}

	// Spawn preset widget components.
	UStaticMesh* ManipulatorMesh = GetManipulatorMesh();
	check(ManipulatorMesh);
	// Use different material for landmarks
	UMaterialInterface* ManipulatorMaterial = GetManipulatorMaterial();
	check(ManipulatorMaterial);
	UStaticMeshComponent* ManipulatorComponent = ManipulatorComponents[SelectedManipulator];
	const float GizmoScale = GetManipulatorScale();
 
	const FVector OriginalPosition = ManipulatorComponent->GetComponentLocation();
	UStaticMesh* AncestoryPlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/" UE_PLUGIN_NAME "/Tools/SM_BlendTool_AncestryCircle.SM_BlendTool_AncestryCircle'"));
	UStaticMesh* ItemMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/" UE_PLUGIN_NAME "/Tools/SM_BlendTool_Item.SM_BlendTool_Item'"));
	UMaterialInterface* ItemMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Tools/MI_CatalogItem.MI_CatalogItem'"));
	check(AncestoryPlaneMesh);
	check(ItemMesh);
 
	ManipulatorComponents[SelectedManipulator]->SetStaticMesh(GetManipulatorDragHandleMesh());
	ManipulatorComponents[SelectedManipulator]->CreateAndSetMaterialInstanceDynamicFromMaterial(0, GetManipulatorDragHandleMaterial());
 
	// Create Ancestry circle with central item
	AncestryCircleComponent = NewObject<UStaticMeshComponent>(ManipulatorsActor);
	AncestryCircleComponent->SetStaticMesh(AncestoryPlaneMesh);
	AncestryCircleComponent->SetWorldScale3D(FVector{ GizmoScale * MeshEditingToolProperties->Size } * 10);
	AncestryCircleComponent->SetWorldLocation(OriginalPosition);
	AncestryCircleComponent->SetupAttachment(ManipulatorsActor->GetRootComponent());
	AncestryCircleComponent->RegisterComponent();
 
	CreatePresetItem(ItemMesh, GizmoScale, OriginalPosition, ItemMaterial);
 
	// Calculate the offsets of meshes on Camera-Facing plane
	FViewport* Viewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(Viewport->GetClient());
 
	check(ViewportClient);

	// ensure we are in orbit mode
	ViewportClient->ToggleOrbitCamera(true);
	const FMatrix CameraRot = ViewportClient->GetViewTransform().ComputeOrbitMatrix();
	const FVector CameraRight = CameraRot.GetTransposed().GetUnitAxis(EAxis::Y);
	const FVector CameraUp = CameraRot.GetTransposed().GetUnitAxis(EAxis::Z);
	
	// We just use three meshes around the circle here, can be adjusted for multiple
	const float StartingAngleDegrees = 90.f;
	const int32 NumOfPresets = 3;
	for(uint16 i = 0 ; i < NumOfPresets ; i++)
	{
		float AngleRad = FMath::DegreesToRadians(StartingAngleDegrees + 120.f * i); // Instead of 120.f it would be 360.f / NumberOfPresetItems if we wanted more that three
		FVector OffsetForMesh = (CameraRight * FMath::Cos(AngleRad) + CameraUp * FMath::Sin(AngleRad)) * GetAncestryCircleRadius();
		
		CreatePresetItem(ItemMesh, GizmoScale, OriginalPosition + OffsetForMesh, ItemMaterial);
	}

	Super::OnBeginDrag(InRay);
}

void UMetaHumanCharacterEditorMeshBlendTool::OnEndDrag(const FRay& InRay)
{
	if (SelectedManipulator == INDEX_NONE)
	{
		return;
	}

	AncestryCircleComponent->UnregisterComponent();
	AncestryCircleComponent->DestroyComponent();
 
	ManipulatorComponents[SelectedManipulator]->SetStaticMesh(GetManipulatorMesh());
	ManipulatorComponents[SelectedManipulator]->CreateAndSetMaterialInstanceDynamicFromMaterial(0, GetManipulatorMaterial());
 
	TArray<FVector3f> ManipulatorPositions = GetManipulatorPositions();
	if (ManipulatorPositions.IsValidIndex(SelectedManipulator))
	{
		ManipulatorComponents[SelectedManipulator]->SetWorldLocation(FVector(ManipulatorPositions[SelectedManipulator]));
	}

	if (ManipulatorsActor)
	{
		for (UStaticMeshComponent* PresetWidgetComponent : PresetItemComponents)
		{
			if (PresetWidgetComponent)
			{
				PresetWidgetComponent->UnregisterComponent();
				PresetWidgetComponent->DestroyComponent();
			}
		}
		PresetItemComponents.Empty();
		PresetItemPositions.Empty();
	}
 
	Super::OnEndDrag(InRay);
}

void UMetaHumanCharacterEditorMeshBlendTool::OnTick(float InDeltaTime)
{
	// Update the manipulators hover state
	for (int32 ManipulatorIndex = 0; ManipulatorIndex < ManipulatorComponents.Num(); ++ManipulatorIndex)
	{
		const bool bIsSelected = ManipulatorIndex == SelectedManipulator;
		SetManipulatorHoverState(ManipulatorIndex, bIsSelected);
		SetManipulatorDragState(ManipulatorIndex, false);
	}
 
	if (!PendingMoveDelta.IsZero() && SelectedManipulator != INDEX_NONE)
	{
		BeginDragMoveDelta += PendingMoveDelta;
		// Translate the manipulator and update the Mesh
		
		TArray<FVector3f> ManipulatorPositions;
		TArray<float> Weights;
		if (SelectedManipulator != INDEX_NONE && ManipulatorComponents.IsValidIndex(SelectedManipulator))
		{
			if (CalculateWeights(ManipulatorComponents[SelectedManipulator]->GetComponentLocation(), PresetItemPositions, Weights))
			{
				// Update materials in preset items based on weight
				int32 RegionIndex = SelectedManipulator;
				if (GetShiftToggle())
				{
					// Blend all regions.
					RegionIndex = INDEX_NONE;
				}
				ManipulatorPositions = BlendPresets(RegionIndex, Weights);
			}
		}
		UpdateManipulatorPositions(ManipulatorPositions);

		FVector MoveLocationBeforeClamping = PresetItemPositions[0] + FVector(BeginDragMoveDelta);
 
		// Clamping move location to ancestry circle borders
		FViewport* Viewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
		FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(Viewport->GetClient());
 
		check(ViewportClient);
 
		const FMatrix CameraRot = ViewportClient->GetViewTransform().ComputeOrbitMatrix();
		const FVector CameraRight = CameraRot.GetTransposed().GetUnitAxis(EAxis::Y);
		const FVector CameraUp = CameraRot.GetTransposed().GetUnitAxis(EAxis::Z);
 
		// We project the move delta to Camera-Facing plane to calculate the offset in that 2D plane
		const float X = FVector::DotProduct(FVector(BeginDragMoveDelta), CameraRight);
		const float Y = FVector::DotProduct(FVector(BeginDragMoveDelta), CameraUp);
 
		FVector2D LocalOffset2D(X,Y);
 
		// Clamp if needed
		if(LocalOffset2D.Size() > GetAncestryCircleRadius())
		{
			LocalOffset2D = LocalOffset2D.GetSafeNormal() * GetAncestryCircleRadius();
		}
 
		FVector ClampedWorldOffset = (CameraRight * LocalOffset2D.X) + (CameraUp * LocalOffset2D.Y);
 
		ManipulatorComponents[SelectedManipulator]->SetWorldLocation(PresetItemPositions[0] + ClampedWorldOffset);
 
		SetWeightOnPresetMaterials(Weights);
 
		PendingMoveDelta = FVector3f::ZeroVector;
	}
}

void UMetaHumanCharacterEditorMeshBlendTool::OnClickPress(const FInputDeviceRay& InClickPos)
{
	Super::OnClickPress(InClickPos);
}

UStaticMesh* UMetaHumanCharacterEditorMeshBlendTool::GetManipulatorMesh() const
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/" UE_PLUGIN_NAME "/Tools/SM_Blend_Gizmo.SM_Blend_Gizmo'"));
}

UMaterialInterface* UMetaHumanCharacterEditorMeshBlendTool::GetManipulatorMaterial() const
{
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Tools/MI_BlendTool_Gizmo.MI_BlendTool_Gizmo'"));
}

float UMetaHumanCharacterEditorMeshBlendTool::GetManipulatorScale() const
{
	return 0.002;
}

TArray<FVector3f> UMetaHumanCharacterEditorMeshBlendTool::TranslateManipulator(int32 InGizmoIndex, const FVector3f& InDelta)
{
	if (SelectedManipulator != INDEX_NONE && ManipulatorComponents.IsValidIndex(SelectedManipulator))
	{
		TArray<float> Weights;
		if (CalculateWeights(ManipulatorComponents[SelectedManipulator]->GetComponentLocation(), PresetItemPositions, Weights))
		{
			// Update materials in preset items based on weight
			return BlendPresets(InGizmoIndex, Weights);
		}
	}
	return {};
}


void UMetaHumanCharacterEditorMeshBlendTool::CreatePresetItem(UStaticMesh* ManipulatorMesh, const float GizmoScale, const FVector WidgetPosition, UMaterialInterface* ManipulatorMaterial)
{
	UStaticMeshComponent* PresetItemComponent = NewObject<UStaticMeshComponent>(ManipulatorsActor);
	PresetItemComponent->SetStaticMesh(ManipulatorMesh);
	PresetItemComponent->SetWorldScale3D(FVector{ GizmoScale * MeshEditingToolProperties->Size } );
	PresetItemComponent->SetWorldLocation(WidgetPosition);
	PresetItemComponent->SetCastShadow(false);
	PresetItemComponent->SetupAttachment(ManipulatorsActor->GetRootComponent());
	PresetItemComponent->RegisterComponent();
	PresetItemComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, ManipulatorMaterial);
	PresetItemComponents.Add(PresetItemComponent);
	PresetItemPositions.Add(WidgetPosition);
}

bool UMetaHumanCharacterEditorMeshBlendTool::CalculateWeights(const FVector& InputPosition, const TArray<FVector>& Targets, TArray<float>& OutResult)
{
	const int32 TargetsCount = Targets.Num();

	// At least 3 targets are needed because we need to calculate the plane of rotation
	if (TargetsCount <= 2)
	{
		return false;
	}

	// Clear result
	OutResult.Empty();
	OutResult.AddZeroed(TargetsCount - 1);

	const double Dist = (InputPosition - Targets[0]).Length();
	const double Radius = (Targets[1] - Targets[0]).Length();
	const double Ratio = FMath::Clamp(Dist / Radius, 0.0, 1.0);

	// threshold to not start blending
	if (Dist > 0.02)
	{
		FVector XDir = (Targets[1] - Targets[0]);
		XDir.Normalize();
		FVector Normal = (Targets[2] - Targets[0]).Cross(XDir);
		Normal.Normalize();
		FVector YDir = XDir.Cross(Normal);
		YDir.Normalize();
		FVector Dir = (InputPosition - Targets[0]);
		Dir.Normalize();
		double X = Dir.Dot(XDir);
		double Y = Dir.Dot(YDir);
		double DIdx = (FMath::Atan2(Y, X) + 2.0 * PI) / (2.0 * PI) * double(TargetsCount - 1);
		int32 Idx = (int32)DIdx;
		double Delta = DIdx - (double)Idx;
		OutResult[Idx % (TargetsCount - 1)] = (1.0 - Delta) * Ratio;
		OutResult[(Idx + 1) % (TargetsCount - 1)] = Delta * Ratio;
	}

	return true;
}

float UMetaHumanCharacterEditorMeshBlendTool::GetAncestryCircleRadius() const
{
	return 3.f;
}

UMaterialInterface* UMetaHumanCharacterEditorMeshBlendTool::GetManipulatorDragHandleMaterial() const
{
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Tools/MI_CatalogHandler.MI_CatalogHandler'"));
}

void UMetaHumanCharacterEditorMeshBlendTool::SetWeightOnPresetMaterials(const TArray<float>& Weights)
{
	float Total = 0;
	for (int32 Idx = 0; Idx < Weights.Num(); ++Idx)
	{
		Total += Weights[Idx];
	}
	for (uint16 i = 0; i < 4; i++)
	{
		UMaterialInstanceDynamic* ManipulatorMaterialInstance = CastChecked<UMaterialInstanceDynamic>(PresetItemComponents[i]->GetMaterial(0));
		if (i == 0)
		{
			ManipulatorMaterialInstance->SetScalarParameterValue(TEXT("Weight"), 1.f - Total);
		}
		else if (i < Weights.Num() + 1)
		{
			ManipulatorMaterialInstance->SetScalarParameterValue(TEXT("Weight"), Weights[i - 1]);
		}		
	}
}

UStaticMesh* UMetaHumanCharacterEditorMeshBlendTool::GetManipulatorDragHandleMesh() const
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/" UE_PLUGIN_NAME "/Tools/SM_BlendTool_Handler.SM_BlendTool_Handler'"));
}

#undef LOCTEXT_NAMESPACE