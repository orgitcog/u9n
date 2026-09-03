// Copyright Epic Games, Inc. All Rights Reserved.


#include "PCapBoneVisualizer.h"
#include "AnimationCoreLibrary.h"
#include "SkeletalMeshAttributes.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"


// Sets default values for this component's properties
UPCapBoneVisualiser::UPCapBoneVisualiser(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true; //make this component tick in editor
	VisualizationType = EBoneVisualType::Joint;
	Color = FLinearColor(0.5,0.5,0.5,1.0);
	UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bAlwaysCreatePhysicsState = false;
	SetCastShadow(false);
}

void UPCapBoneVisualiser::OnRegister()
{
	Super::OnRegister();

	ASkeletalMeshActor* OwnerActor = Cast<ASkeletalMeshActor>(GetOwner());
	
	if(OwnerActor)
	{
		SkelmeshComponent = OwnerActor->GetSkeletalMeshComponent();
	}

	ClearInstances();

	if(SkelmeshComponent)
	{
		TArray<FTransform> BoneTransforms = GetBoneTransforms(SkelmeshComponent);
		AddInstances(BoneTransforms, false, true, false);

		if(!DynamicMaterial)
		{
			UMaterialInterface* BaseMaterial = GetMaterial(0);
			if(BaseMaterial) //Check if the BaseMaterial is Valid. It will only be valid if a mesh has already been sent. 
			{
				DynamicMaterial = CreateDynamicMaterialInstance(0, BaseMaterial, TEXT("MaterialInstance"));
				SetMaterial(0, DynamicMaterial);
				DynamicMaterial->ClearParameterValues();
				DynamicMaterial->SetVectorParameterValue(FName(TEXT("Color")), Color);
			}
		}
	}
}

void UPCapBoneVisualiser::OnUnregister()
{
	Super::OnUnregister();

	ClearInstances();
}

void UPCapBoneVisualiser::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if(SkelmeshComponent)
	{
		switch (VisualizationType)
		{
		case EBoneVisualType::Bone:
			{
				BatchUpdateInstancesTransforms(0, GetBoneTransforms(SkelmeshComponent), true, true, false);
			}
			break;
		case EBoneVisualType::Joint:
			{
				BatchUpdateInstancesTransforms(0, GetJointTransforms(SkelmeshComponent), true, true, false);
			}

			break;
		default:
			checkNoEntry();
		}
	}
}

TArray<FTransform> UPCapBoneVisualiser::GetJointTransforms(USkeletalMeshComponent* InSkelMeshComponent) const
{
	TArray<FTransform> FoundTransforms;

	int32 i = InSkelMeshComponent->GetNumBones();
	FTransform BoneTransform;

	for (i = 0; i < InSkelMeshComponent->GetNumBones(); i++ )
	{
		BoneTransform = InSkelMeshComponent->GetBoneTransform(InSkelMeshComponent->GetBoneName(i));
		FoundTransforms.Add(BoneTransform);
	}

	return FoundTransforms;
}

TArray<FTransform> UPCapBoneVisualiser::GetBoneTransforms(USkeletalMeshComponent* InSkelMeshComponent) const
{
	TArray<FTransform> FoundTransforms;

	int32 i = InSkelMeshComponent->GetNumBones();
	FTransform BoneTransform;
	FTransform ParentBoneTransform;
	double BoneLength;
	FVector Aim = FVector( 0, 0, 1);

	for (i = 0; i < InSkelMeshComponent->GetNumBones(); i++ )
	{
		BoneTransform = InSkelMeshComponent->GetBoneTransform(InSkelMeshComponent->GetBoneName(i));
		ParentBoneTransform = InSkelMeshComponent->GetBoneTransform(InSkelMeshComponent->GetParentBone(InSkelMeshComponent->GetBoneName(i)));
		BoneLength = FVector::Distance(BoneTransform.GetLocation(), ParentBoneTransform.GetLocation());
		BoneTransform.SetScale3D(FVector(1, 1, BoneLength));
		FQuat DiffRotation = AnimationCore::SolveAim(BoneTransform, ParentBoneTransform.GetLocation(), Aim.GetSafeNormal(), false, FVector(1, 1, 1), float(0));
		BoneTransform.SetRotation(DiffRotation);
		FoundTransforms.Add(BoneTransform);
	}
	return FoundTransforms;
}

void UPCapBoneVisualiser::UpdateColor(FLinearColor NewColor)
{
	Color = NewColor;
	if(DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(FName(TEXT("Color")), Color);
	}
}

void UPCapBoneVisualiser::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(PropertyChangedEvent.Property != nullptr)
	{
		if(PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCapBoneVisualiser, Color))
		{
			UpdateColor(Color);
		}
	}
}

