// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorActor.h"
#include "MetaHumanCharacterEditorSubsystem.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "MetaHumanCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LODSyncComponent.h"
#include "Interfaces/IPluginManager.h"
#include "Animation/AnimInstance.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "SkelMeshDNAUtils.h"
#include "MetaHumanCharacterSkelMeshUtils.h"
#include "MetaHumanCharacterAnimInstance.h"
#include "RetargetComponent.h"
#include "Retargeter/IKRetargeter.h"

AMetaHumanCharacterEditorActor::AMetaHumanCharacterEditorActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	BodyComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	FaceComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));
	LODSyncComponent = CreateDefaultSubobject<ULODSyncComponent>(TEXT("LODSync"));

	FaceComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	BodyComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	BodyComponent->SetupAttachment(RootComponent);
	FaceComponent->SetupAttachment(BodyComponent);

	// Add the IK retargeter component and assign our IK retargeter asset.
	static ConstructorHelpers::FObjectFinder<UIKRetargeter> IKRetargeterFinder(TEXT("/" UE_PLUGIN_NAME "/Animation/Retargeting/RTG_MH_IKRig"));
	if (IKRetargeterFinder.Succeeded())
	{
		IKRetargeter = IKRetargeterFinder.Object;
	}

	RetargetComponent = CreateDefaultSubobject<URetargetComponent>(TEXT("Retarget"));
	ReinitAnimation();
}

void AMetaHumanCharacterEditorActor::ReinitAnimation()
{
	if (RetargetComponent)
	{
		RetargetComponent->SetForceOtherMeshesToFollowControlledMesh(false);
		RetargetComponent->SetRetargetAsset(IKRetargeter.Get());
		RetargetComponent->SetControlledMesh(BodyComponent);
	}
}

void AMetaHumanCharacterEditorActor::ResetAnimation()
{
	if (FaceComponent)
	{
		FaceComponent->SetAnimInstanceClass(nullptr);
	}

	if (BodyComponent)
	{
		BodyComponent->SetAnimInstanceClass(nullptr);
	}

	RetargetComponent->SetRetargetAsset(nullptr);
	RetargetComponent->SetControlledMesh(nullptr);
}

void AMetaHumanCharacterEditorActor::InitializeMetaHumanCharacterEditorActor(
	TNotNull<const UMetaHumanCharacterInstance*> InCharacterInstance,
	TNotNull<UMetaHumanCharacter*> InCharacter,
	TNotNull<USkeletalMesh*> InFaceMesh,
	TNotNull<USkeletalMesh*> InBodyMesh,
	int32 InNumLODs,
	const TArray<int32>& InFaceLODMapping,
	const TArray<int32>& InBodyLODMapping)
{
	CharacterInstance = InCharacterInstance;
	Character = InCharacter;
	FaceComponent->SetSkeletalMesh(InFaceMesh);
	BodyComponent->SetSkeletalMesh(InBodyMesh);

	{
		LODSyncComponent->NumLODs = InNumLODs;

		// Skeletal meshes
		LODSyncComponent->ComponentsToSync.Add(FComponentSync(TEXT("Body"), ESyncOption::Drive));
		LODSyncComponent->ComponentsToSync.Add(FComponentSync(TEXT("Face"), ESyncOption::Drive));

		// Grooms
		LODSyncComponent->ComponentsToSync.Add(FComponentSync(TEXT("Hair"), ESyncOption::Passive));
		LODSyncComponent->ComponentsToSync.Add(FComponentSync(TEXT("Eyebrows"), ESyncOption::Passive));
		LODSyncComponent->ComponentsToSync.Add(FComponentSync(TEXT("Eyelashes"), ESyncOption::Passive));
		LODSyncComponent->ComponentsToSync.Add(FComponentSync(TEXT("Mustache"), ESyncOption::Passive));
		LODSyncComponent->ComponentsToSync.Add(FComponentSync(TEXT("Beard"), ESyncOption::Passive));
		LODSyncComponent->ComponentsToSync.Add(FComponentSync(TEXT("Peachfuzz"), ESyncOption::Passive));

		FLODMappingData& FaceMappingData = LODSyncComponent->CustomLODMapping.FindOrAdd(TEXT("Face"));
		FaceMappingData.Mapping = InFaceLODMapping;

		FLODMappingData& BodyMappingData = LODSyncComponent->CustomLODMapping.FindOrAdd(TEXT("Body"));
		BodyMappingData.Mapping = InBodyLODMapping;

		LODSyncComponent->RefreshSyncComponents();
	}
}

void AMetaHumanCharacterEditorActor::SetForcedLOD(int32 InForcedLOD)
{
	LODSyncComponent->ForcedLOD = InForcedLOD;
}

TNotNull<UMetaHumanCharacter*> AMetaHumanCharacterEditorActor::GetCharacter() const
{
	return Character;
}

TNotNull<const USkeletalMeshComponent*> AMetaHumanCharacterEditorActor::GetFaceComponent() const
{
	return FaceComponent;
}

TNotNull<const USkeletalMeshComponent*> AMetaHumanCharacterEditorActor::GetBodyComponent() const
{
	return BodyComponent;
}

void AMetaHumanCharacterEditorActor::OnFaceMeshUpdated()
{
	FaceComponent->MarkRenderStateDirty();
	FaceComponent->UpdateBounds();
}

void AMetaHumanCharacterEditorActor::OnBodyMeshUpdated()
{
	BodyComponent->MarkRenderStateDirty();
	BodyComponent->UpdateBounds();
}

void AMetaHumanCharacterEditorActor::SetDrivingSkeletalMesh(USkeletalMeshComponent* DrivingSkelMeshComponent)
{
	RetargetComponent->SetSourcePerformerMesh(DrivingSkelMeshComponent);
}

void AMetaHumanCharacterEditorActor::UpdateFaceComponentMesh(USkeletalMesh* InFaceMesh)
{
	if (InFaceMesh)
	{
		FaceComponent->SetSkeletalMesh(InFaceMesh);
	}
}

void AMetaHumanCharacterEditorActor::UpdateBodyComponentMesh(USkeletalMesh* InBodyMesh)
{
	if (InBodyMesh)
	{
		BodyComponent->SetSkeletalMesh(InBodyMesh);
	}
}

void AMetaHumanCharacterEditorActor::SetHairVisibilityState(EMetaHumanHairVisibilityState State)
{
	Blueprint_SetHairVisibilityState(State);
}

void AMetaHumanCharacterEditorActor::SetClothingVisibilityState(EMetaHumanClothingVisibilityState State, UMaterialInterface* OverrideMaterial)
{
	Blueprint_SetClothingVisibilityState(State, OverrideMaterial);
}
