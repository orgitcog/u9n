// Copyright Epic Games, Inc. All Rights Reserved.

#include "SKMMorphTargetToolTarget.h"

#include "DynamicMeshToMeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "SkeletalMeshAttributes.h"
#include "SkeletalMeshOperations.h"
#include "SkeletalMeshTypes.h"
#include "StaticMeshAttributes.h"
#include "Components/SkeletalMeshComponent.h"
#include "ConversionUtils/DynamicMeshViaMeshDescriptionUtil.h"
#include "UObject/Package.h"
#include "ConversionUtils/SkinnedMeshToDynamicMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "AnimationRuntime.h"
#include "SkeletalMeshEditorSubsystem.h"

UE::Geometry::FDynamicMesh3 USkeletalMeshMorphTargetToolTarget::GetDynamicMesh()
{
	FGetMeshParameters Params;
	return GetDynamicMesh(Params);
}

UE::Geometry::FDynamicMesh3 USkeletalMeshMorphTargetToolTarget::GetDynamicMesh(const FGetMeshParameters& InGetMeshParams)
{
	FDynamicMesh3 DynamicMesh;
	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(GetEditingMeshDescription(), DynamicMesh);
	
	return DynamicMesh;
}

void USkeletalMeshMorphTargetToolTarget::CommitDynamicMesh(const UE::Geometry::FDynamicMesh3& Mesh, const FDynamicMeshCommitInfo& CommitInfo)
{
	if (ToolMeshDescription.IsEmpty() || ToolMorphTargetName.IsNone() || EditingMorphTargetName.IsNone())
	{
		return;
	}
	
	FMeshDescription* TargetMeshDescription = GetEditingMeshDescription();
	FSkeletalMeshAttributes TargetAttributes(*TargetMeshDescription);

	if (!TargetAttributes.GetMorphTargetNames().Contains(EditingMorphTargetName))
	{
		TargetAttributes.RegisterMorphTargetAttribute(EditingMorphTargetName, false);
	}
	
	TVertexAttributesRef<FVector3f> TargetMorphTargetPosDeltaAttribute = TargetAttributes.GetVertexMorphPositionDelta(EditingMorphTargetName);

	
	FSkeletalMeshAttributes ToolAttributes(ToolMeshDescription);
	TVertexAttributesRef<FVector3f> ToolMorphTargetPosDeltaAttribute = ToolAttributes.GetVertexMorphPositionDelta(ToolMorphTargetName);
	
	
	for (FVertexID VertexID : TargetMeshDescription->Vertices().GetElementIDs())
	{
		TargetMorphTargetPosDeltaAttribute[VertexID] = ToolMorphTargetPosDeltaAttribute[VertexID.GetValue()];
	}		

	CommitEditedMeshDescription();

	USkeletalMesh* SkeletalMesh = GetSkeletalMesh();
	
	// As soon as a morph is edited in engine, we want to mark it such that reimports in the future don't overwrite our edits.
	USkeletalMeshEditorSubsystem::SetMorphTargetsToGeneratedByEngine(SkeletalMesh, {EditingMorphTargetName.ToString()});
	
	UAnimCurveMetaData* AnimCurveMetaData = SkeletalMesh->GetAssetUserData<UAnimCurveMetaData>();
	if (AnimCurveMetaData == nullptr)
	{
		AnimCurveMetaData = NewObject<UAnimCurveMetaData>(SkeletalMesh, NAME_None, RF_Transactional);
		SkeletalMesh->AddAssetUserData(AnimCurveMetaData);
	}

	AnimCurveMetaData->AddCurveMetaData(EditingMorphTargetName);

	// Ensure we have a morph flag set
	if (FCurveMetaData* CurveMetaData = AnimCurveMetaData->GetCurveMetaData(EditingMorphTargetName))
	{
		CurveMetaData->Type.bMorphtarget = true;
	}
}

void USkeletalMeshMorphTargetToolTarget::SetDataToCommit(FMeshDescription&& InMeshDescription, const FName& InToolMorphTargetName)
{
	ToolMeshDescription = MoveTemp(InMeshDescription);
	ToolMorphTargetName = InToolMorphTargetName;
}

void USkeletalMeshMorphTargetToolTarget::SetEditingMorphTargetName(const FName& InName)
{
	EditingMorphTargetName = InName;
}

TArray<FName> USkeletalMeshMorphTargetToolTarget::GetEditableMorphTargetNames()
{
	FSkeletalMeshAttributes TargetAttributes(*GetEditingMeshDescription());

	return TargetAttributes.GetMorphTargetNames();
}

FName USkeletalMeshMorphTargetToolTarget::GetValidNameForNewMorphTarget(const FName& InName)
{
	FSkeletalMeshAttributes TargetAttributes(*GetEditingMeshDescription());

	const TArray<FName>& MorphTargetNames = TargetAttributes.GetMorphTargetNames();
	FName NewMorphTargetName = InName;
	while (MorphTargetNames.Contains(NewMorphTargetName))
	{
		NewMorphTargetName.SetNumber(NewMorphTargetName.GetNumber() + 1);
	}

	return NewMorphTargetName;
}

FMeshDescription* USkeletalMeshMorphTargetToolTarget::GetEditingMeshDescription()
{
	USkeletalMesh* SkeletalMesh = GetSkeletalMesh();
	FMeshDescription* TargetMeshDescription = SkeletalMesh->GetMeshDescription(LodIndex0);

	return TargetMeshDescription;
}

void USkeletalMeshMorphTargetToolTarget::CommitEditedMeshDescription()
{
	USkeletalMesh* SkeletalMesh = GetSkeletalMesh();
	{
		FScopedSkeletalMeshPostEditChange ScopePostEditChange(SkeletalMesh);
		SkeletalMesh->PreEditChange(nullptr);
		SkeletalMesh->ModifyMeshDescription(LodIndex0);
		SkeletalMesh->CommitMeshDescription(LodIndex0);
	}
}

bool USkeletalMeshMorphTargetToolTargetFactory::CanBuildTarget(UObject* SourceObject,
                                                               const FToolTargetTypeRequirements& Requirements) const
{
	// We are using an exact cast here to prevent subclasses, which might not meet all
	// requirements for functionality such as the deprecated DestructibleMeshComponent, from 
	// being caught up as valid targets.
	// If you want to make the tool target work with some subclass of USkeletalMeshComponent,
	// just add another factory that allows that class specifically(but make sure that
	// GetMeshDescription and such work properly)
	
	bool bValid = Cast<USkeletalMeshComponent>(SourceObject) && Cast<USkeletalMeshComponent>(SourceObject)->GetSkeletalMeshAsset() &&
		ExactCast<USkeletalMesh>(Cast<USkeletalMeshComponent>(SourceObject)->GetSkeletalMeshAsset()) &&
		!ExactCast<USkeletalMesh>(Cast<USkeletalMeshComponent>(SourceObject)->GetSkeletalMeshAsset())->GetOutermost()->bIsCookedForEditor;
	if (!bValid)
	{
		return false;
	}

	if (!USkeletalMeshComponentToolTargetFactory::CanWriteToSource(SourceObject))
	{
		return false;
	}

	return Requirements.AreSatisfiedBy(USkeletalMeshMorphTargetToolTarget::StaticClass());
}

UToolTarget* USkeletalMeshMorphTargetToolTargetFactory::BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo)
{
	USkeletalMeshMorphTargetToolTarget* Target = NewObject<USkeletalMeshMorphTargetToolTarget>();

	Target->InitializeComponent(Cast<USkeletalMeshComponent>(SourceObject));
	
	return Target;
	
}

