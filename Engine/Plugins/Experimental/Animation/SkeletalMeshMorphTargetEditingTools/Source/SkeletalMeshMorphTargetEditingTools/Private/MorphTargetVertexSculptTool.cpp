// Copyright Epic Games, Inc. All Rights Reserved.

#include "MorphTargetVertexSculptTool.h"

#include "ContextObjectStore.h"
#include "ToolBuilderUtil.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolChange.h"
#include "ModelingToolTargetUtil.h"
#include "ToolTargetManager.h"
#include "StaticMeshAttributes.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h"
#include "IPersonaEditorModeManager.h"
#include "PersonaModule.h"
#include "SKMMorphTargetBackedTarget.h"
#include "SkeletalMeshOperations.h"
#include "BaseGizmos/BrushStampIndicator.h"
#include "TargetInterfaces/DynamicMeshCommitter.h"
#include "TargetInterfaces/DynamicMeshProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "AnimationRuntime.h"
#include "EraseMorphTargetBrushOps.h"
#include "SkeletalMeshAttributes.h"

#define LOCTEXT_NAMESPACE "MorphTargetVertexSculptTool"


UMeshSurfacePointTool* UMorphTargetVertexSculptToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	UMorphTargetVertexSculptTool* MorphTargetEditorTool = NewObject<UMorphTargetVertexSculptTool>(SceneState.ToolManager);
	MorphTargetEditorTool->SetWorld(SceneState.World);
	return MorphTargetEditorTool;
}

const FToolTargetTypeRequirements& UMorphTargetVertexSculptToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements ToolRequirements({
		UMaterialProvider::StaticClass(),
		UDynamicMeshProvider::StaticClass(),
		UDynamicMeshCommitter::StaticClass(),
		USceneComponentBackedTarget::StaticClass(),
		USkeletalMeshMorphTargetBackedTarget::StaticClass(),
	});

	return ToolRequirements;
}


void UMorphTargetVertexSculptTool::Setup()
{
	SetupMorphEditingToolCommon();
	
	// Setup Vertex Sculpt Tool
	Super::Setup();

	MorphTargetBackedTarget = Cast<ISkeletalMeshMorphTargetBackedTarget>(GetTarget());

	OnToolMeshChangedDelegate = DynamicMeshComponent->OnMeshRegionChanged.AddUObject(this, &UMorphTargetVertexSculptTool::OnToolMeshChanged);

	MeshWithoutCurrentMorph = *GetSculptMesh();
	
	InitializeCache();
}

void UMorphTargetVertexSculptTool::RegisterBrushes()
{
	Super::RegisterBrushes();
	
	GetMeshWithoutCurrentMorphFunc = [this](){return &MeshWithoutCurrentMorph;};

	// Had to hijack the EraseSculptLayer identifier from base mesh vertex sculpt tool for our erase morph target tool since it is the simplest way to get an icon for the tool.
	RegisterBrushType(
		(int32)EMeshVertexSculptBrushType::EraseSculptLayer,
		LOCTEXT("EraseSculptLayerBrushTypeName", "EraseSculptLayer"),
		MakeUnique<FLambdaMeshSculptBrushOpFactory>([this]() { return MakeUnique<FEraseMorphTargetBrushOp>(GetMeshWithoutCurrentMorphFunc);}),
		NewObject<UEraseMorphTargetBrushOpProps>(this));
}

void UMorphTargetVertexSculptTool::Shutdown(EToolShutdownType ShutdownType)
{
	if (DynamicMeshComponent != nullptr)
	{
		DynamicMeshComponent->OnMeshChanged.Remove(OnDynamicMeshComponentChangedHandle);
	}

	if (ShutdownType == EToolShutdownType::Accept)
	{
		UpdateCacheIfNeeded();
    
    	if (bHasValidData)
    	{
    		MorphTargetBackedTarget->SetEditingMorphTargetName(EditorToolProperties->GetEditingMorphTargetName());
    		MorphTargetBackedTarget->SetDataToCommit(MoveTemp(ToolMeshDescription), ToolMorphTargetName);
    	}
    	else
    	{
    		GetToolManager()->DisplayMessage(LOCTEXT("MorphTargetInvalidData", "Morph Target is empty, no change was made") , EToolMessageLevel::UserNotification);
    		ShutdownType = EToolShutdownType::Cancel;
    	}	
	}
	
	Super::Shutdown(ShutdownType);

	ShutdownMorphEditingToolCommon();	
}

void UMorphTargetVertexSculptTool::OnTick(float DeltaTime)
{
	if (UPersonaEditorModeManagerContext* PersonaModeManagerContext = GetToolManager()->GetContextObjectStore()->FindContext<UPersonaEditorModeManagerContext>())
	{
		if (!PersonaModeManagerContext->GetPersonaEditorModeManager()->IsModeActive(FPersonaEditModes::SkeletonSelection))
		{
			PersonaModeManagerContext->GetPersonaEditorModeManager()->ActivateMode(FPersonaEditModes::SkeletonSelection);	
		}
	}

	Super::OnTick(DeltaTime);

	if (InStroke())
	{
		bCached = false;
	}
	else 
	{
		PoseToolMesh();		
	}
}

void UMorphTargetVertexSculptTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	Super::OnPropertyModified(PropertySet, Property);

	if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMorphTargetEditingToolProperties, NewMorphTargetName))
	{
		EditorToolProperties->NewMorphTargetName = MorphTargetBackedTarget->GetValidNameForNewMorphTarget(EditorToolProperties->NewMorphTargetName);
	}
	else if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMorphTargetEditingToolProperties, Operation))
	{
		if (EditorToolProperties->Operation == EMorphTargetEditorOperation::Edit)
		{
			if (EditorToolProperties->GetMorphTargetNames().Num() == 0)
			{
				EditorToolProperties->Operation = EMorphTargetEditorOperation::New;
			}
		}
	}
}

/*
 * internal Change classes
 */

class FMorphTargetVertexSculptNonSymmetricChange : public FToolCommandChange
{
public:
	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
};


void FMorphTargetVertexSculptNonSymmetricChange::Apply(UObject* Object)
{
	if (Cast<UMorphTargetVertexSculptTool>(Object))
	{
		Cast<UMorphTargetVertexSculptTool>(Object)->UndoRedo_RestoreSymmetryPossibleState(false);
	}
}
void FMorphTargetVertexSculptNonSymmetricChange::Revert(UObject* Object)
{
	if (Cast<UMorphTargetVertexSculptTool>(Object))
	{
		Cast<UMorphTargetVertexSculptTool>(Object)->UndoRedo_RestoreSymmetryPossibleState(true);
	}
}

void UMorphTargetVertexSculptTool::OnEndStroke()
{
	// update spatial
	bTargetDirty = true;

	GetActiveBrushOp()->EndStroke(GetSculptMesh(), LastStamp, VertexROI);

	check(ActiveVertexChange);

	TUniquePtr<TWrappedToolCommandChange<FMeshVertexChange>> NewChange = MakeUnique<TWrappedToolCommandChange<FMeshVertexChange>>();
	NewChange->WrappedChange = MoveTemp(ActiveVertexChange->Change);
	NewChange->BeforeModify = [this](bool bRevert)
	{
		this->WaitForPendingUndoRedo();
		// Any sculpt change needs to be applied to the latest posed mesh during undo / redo, so pose the mesh immediately instead of waiting
		// for the next tick
		this->PoseToolMesh();
	};

	GetToolManager()->EmitObjectChange(DynamicMeshComponent, MoveTemp(NewChange), LOCTEXT("VertexSculptChange", "Brush Stroke"));
	if (bMeshSymmetryIsValid && bApplySymmetry == false)
	{
		// if we end a stroke while symmetry is possible but disabled, we now have to assume that symmetry is no longer possible
		GetToolManager()->EmitObjectChange(this, MakeUnique<FMorphTargetVertexSculptNonSymmetricChange>(), LOCTEXT("DisableSymmetryChange", "Disable Symmetry"));
		bMeshSymmetryIsValid = false;
		SymmetryProperties->bSymmetryCanBeEnabled = bMeshSymmetryIsValid;
	}
	LongTransactions.Close(GetToolManager());

	delete ActiveVertexChange;
	ActiveVertexChange = nullptr;
}

int32 UMorphTargetVertexSculptTool::FindHitSculptMeshTriangle(const FRay3d& LocalRay)
{
	WaitForPendingUndoRedo();
	return Super::FindHitSculptMeshTriangle(LocalRay);
}

void UMorphTargetVertexSculptTool::OnToolMeshChanged(UDynamicMeshComponent* Component, const FMeshRegionChangeBase* Change, bool bRevert)
{
	if (!bPosingSculptMesh)
	{
		// If not posing, it must have been a sculpt change that was reverted/applied, should update the cache
		bCached = false;
	}
}


void UMorphTargetVertexSculptTool::SetupCommonProperties(const TFunction<void(UMorphTargetEditingToolProperties*)>& InSetupFunction)
{
	EditorToolProperties = NewObject<UMorphTargetEditingToolProperties>(this);
	EditorToolProperties->SetFlags(RF_Transactional);

	InSetupFunction(EditorToolProperties);
	
	AddToolPropertySource(EditorToolProperties);
}

void UMorphTargetVertexSculptTool::HandleSkeletalMeshModified(const TArray<FName>& Payload, const ESkeletalMeshNotifyType InNotifyType)
{
}

void UMorphTargetVertexSculptTool::InitializeCache()
{
	UPrimitiveComponent* Component = Cast<IPrimitiveComponentBackedTarget>(GetTarget())->GetOwnerComponent();
	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component);

	ToolMeshDescription = *SkeletalMeshComponent->GetSkeletalMeshAsset()->GetMeshDescription(LodIndex0);

	// Pre compact the mesh description to avoid compacting one every frame
	if (ToolMeshDescription.NeedsCompact())
	{
		FElementIDRemappings Remappings;
		ToolMeshDescription.Compact(Remappings);
	}
	else
	{
		// Make sure indexers are built before entering parallel work
		ToolMeshDescription.BuildVertexIndexers();
	}
	
	FSkeletalMeshAttributes Attributes(ToolMeshDescription);
	
	ToolMorphTargetName = EditorToolProperties->GetEditingMorphTargetName();
	
	if (EditorToolProperties->Operation == EMorphTargetEditorOperation::New)
	{
		check(!Attributes.GetMorphTargetNames().Contains(ToolMorphTargetName))
		Attributes.RegisterMorphTargetAttribute(ToolMorphTargetName, false);
	}

	bCached = true;
}

void UMorphTargetVertexSculptTool::UpdateCacheIfNeeded()
{
	WaitForPendingStampUpdate();
	WaitForPendingUndoRedo();
	UPrimitiveComponent* Component = Cast<IPrimitiveComponentBackedTarget>(GetTarget())->GetOwnerComponent();
	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component);

	if (!SkeletalMeshComponent->GetSkeletalMeshAsset())
	{
		return;
	}
	
	if (bCached)
	{
		return;
	}

	// Don't update the cache if the morph cannot be extracted from the mesh
	if (FMath::IsNearlyZero(EditorToolProperties->MorphTargetWeight))
	{
		bCached = true;
		return;
	}

	const FReferenceSkeleton& RefSkeleton = SkeletalMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
	
	TArray<FTransform> RefComponentSpaceTransforms;
	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		// Need local to component transform
		RefComponentSpaceTransforms.Add(FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex));
	}


	TMap<FName, float> MorphTargetWeights = PreviousMorphWeights;
	MorphTargetWeights.Remove(ToolMorphTargetName);

	// In the case that source mesh contains non-manifold verts, extra duplicated verts will be added to end of the dynamic mesh verts array
	// see FMeshDescriptionToDynamicMesh::Convert
	check(GetSculptMesh()->VertexCount() >= ToolMeshDescription.GetVertexPositions().GetNumElements());
	for (FVertexID VertexID : ToolMeshDescription.Vertices().GetElementIDs())
	{
		ToolMeshDescription.GetVertexPositions()[VertexID] = FVector3f(GetSculptMesh()->GetVertex(VertexID));
	}
	
	FMeshDescription* RefMeshDescription = SkeletalMeshComponent->GetSkeletalMeshAsset()->GetMeshDescription(LodIndex0);

	const TArray<FTransform>& ComponentSpaceTransforms = PreviousPoseComponentSpace;

	FSkeletalMeshAttributes Attributes(ToolMeshDescription);
	TVertexAttributesRef<FVector3f> MorphTargetPosDeltaAttribute = Attributes.GetVertexMorphPositionDelta(ToolMorphTargetName);
	
	if (FSkeletalMeshOperations::GetUnposedMeshInPlace(ToolMeshDescription, *RefMeshDescription, RefComponentSpaceTransforms, ComponentSpaceTransforms, NAME_None, MorphTargetWeights))
	{
		bHasValidData = false;
		for (FVertexID VertexID : ToolMeshDescription.Vertices().GetElementIDs())
		{
			FVector3f Delta = ToolMeshDescription.GetVertexPosition(VertexID) - RefMeshDescription->GetVertexPosition(VertexID);
			Delta /= EditorToolProperties->MorphTargetWeight;

			if (!bHasValidData && Delta.SizeSquared() > FMath::Square(UE_THRESH_POINTS_ARE_NEAR))
			{
				bHasValidData = true;
			}
			MorphTargetPosDeltaAttribute[VertexID] = Delta;
		}
	}

	bCached = true;
}

void UMorphTargetVertexSculptTool::PoseToolMesh()
{
	if (!AllowToolMeshUpdates())
	{
		return;
	}
	
	auto IsMorphWeightChanged = [&PreviousMorphWeights = this->PreviousMorphWeights](const FName& InName, float InMorphWeight) -> bool
	{
		if (float* PreviousWeight = PreviousMorphWeights.Find(InName))
		{
			if (!FMath::IsNearlyEqual(*PreviousWeight, InMorphWeight))
			{
				return true;
			}
		}
		else
		{
			return true;
		}
		return false;
	};
	
	
	UPrimitiveComponent* Component = Cast<IPrimitiveComponentBackedTarget>(GetTarget())->GetOwnerComponent();
	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component);
	
	TMap<FName, float> MorphTargetWeights;

	for (const TPair<const UMorphTarget*, int32>& MorphTarget: SkeletalMeshComponent->ActiveMorphTargets)
	{
		const FName MorphName = MorphTarget.Key->GetFName();
		const float MorphWeight = SkeletalMeshComponent->MorphTargetWeights[MorphTarget.Value];

		MorphTargetWeights.Add(MorphName, MorphWeight);
	}
	
	MorphTargetWeights.FindOrAdd(ToolMorphTargetName) = EditorToolProperties->MorphTargetWeight;

	const TArray<FTransform>& ComponentSpaceTransforms = SkeletalMeshComponent->GetComponentSpaceTransforms();
	
	bool bPoseChanged = false;
	
	// First tick, force an update
	if (!bPoseOpInitialized)
	{
		bPoseOpInitialized = true;
		
		PreviousMorphWeights = MorphTargetWeights;
		PreviousPoseComponentSpace = ComponentSpaceTransforms;
		bPoseChanged = true;
	}
	else
	{
		// Check for pose changes and update accordingly
		
		if (!bPoseChanged)
		{
			if (MorphTargetWeights.Num() != PreviousMorphWeights.Num())
			{
				bPoseChanged = true;
			}
		}
		
		if (!bPoseChanged)
		{
			for (const TPair<FName, float>& MorphWeight: MorphTargetWeights)
			{
				if (IsMorphWeightChanged(MorphWeight.Key, MorphWeight.Value))
				{
					bPoseChanged = true;
					break;
				}
			}
		}
		
		if (!bPoseChanged)
		{
			for (int32 BoneIndex=0; BoneIndex< ComponentSpaceTransforms.Num(); ++BoneIndex)
			{
				const FTransform& CurrentBoneTransform = ComponentSpaceTransforms[BoneIndex];
				const FTransform& PrevBoneTransform = PreviousPoseComponentSpace[BoneIndex];
				if (!CurrentBoneTransform.Equals(PrevBoneTransform))
				{
					bPoseChanged = true;
					break;
				}
			}
		}
	}

	if (!bPoseChanged)
	{
		if (bPoseChangedLastTick)
		{
			FMeshReplacementChange Change(MeshBeforePosing, MakeShared<FDynamicMesh3>(*GetSculptMesh()));

			TGuardValue<bool> PosingSculptMeshScope(bPosingSculptMesh, true);
			OnDynamicMeshComponentChanged(DynamicMeshComponent, &Change, false);
		}

		bPoseChangedLastTick = bPoseChanged;
		return;
	}

	if (!bPoseChangedLastTick)
	{
		MeshBeforePosing = MakeShared<FDynamicMesh3>(*GetSculptMesh());
	}
	bPoseChangedLastTick = bPoseChanged;
	
	
	UpdateCacheIfNeeded();
	
	PreviousPoseComponentSpace = ComponentSpaceTransforms;
	PreviousMorphWeights = MorphTargetWeights;

	// have to wait for any outstanding stamp/undo update to finish...
	WaitForPendingStampUpdate();
	WaitForPendingUndoRedo();

	TArray<FTransform> RefComponentSpaceTransforms;
	for (int32 BoneIndex = 0; BoneIndex < ComponentSpaceTransforms.Num(); ++BoneIndex)
	{
		// Need local to component transform
		RefComponentSpaceTransforms.Add(FAnimationRuntime::GetComponentSpaceTransformRefPose(SkeletalMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton(), BoneIndex));
	}
			
	FMeshDescription* RefMeshDescription = SkeletalMeshComponent->GetSkeletalMeshAsset()->GetMeshDescription(LodIndex0);
	// Reset verts to ref pose for posing
	for (FVertexID VertexID : ToolMeshDescription.Vertices().GetElementIDs())
	{
		ToolMeshDescription.GetVertexPositions()[VertexID] = RefMeshDescription->GetVertexPositions()[VertexID];
	}

	// No need to compute normals for the mesh description, the sculpt dynamic mesh recomputes its own normals during OnDynamicMeshComponentChanged()
	constexpr bool bSkipRecomputeNormalsTangents = true;
	FSkeletalMeshOperations::GetPosedMeshInPlace(ToolMeshDescription, ComponentSpaceTransforms, NAME_None, MorphTargetWeights, bSkipRecomputeNormalsTangents);

	FDynamicMesh3& Mesh = *GetSculptMesh();
	
	TArrayView<const FVector3f> VertexPositionsAttribute = ToolMeshDescription.GetVertexPositions().GetRawArray();
	
	FSkeletalMeshAttributes Attributes(ToolMeshDescription);
	TVertexAttributesRef<FVector3f> MorphTargetPosDeltaAttribute = Attributes.GetVertexMorphPositionDelta(ToolMorphTargetName);
	
	for (int32 Index = 0; Index < VertexPositionsAttribute.Num(); ++Index)
	{
		Mesh.SetVertex(Index, FVector3d(VertexPositionsAttribute[Index]));

		FVector3f Delta = MorphTargetPosDeltaAttribute[Index] * EditorToolProperties->MorphTargetWeight;
		MeshWithoutCurrentMorph.SetVertex(Index, FVector3d(VertexPositionsAttribute[Index] - Delta));
	}	
	
	DynamicMeshComponent->FastNotifyPositionsUpdated();
}	


#undef LOCTEXT_NAMESPACE

