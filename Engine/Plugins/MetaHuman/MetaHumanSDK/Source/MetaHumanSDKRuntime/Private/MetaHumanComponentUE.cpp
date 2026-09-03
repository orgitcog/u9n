// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanComponentUE.h"
#include "Components/SkeletalMeshComponent.h"

#include "Animation/AnimInstance.h"
#include "ControlRig.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"

void UMetaHumanComponentUE::OnRegister()
{
	Super::OnRegister();
}

void UMetaHumanComponentUE::BeginPlay()
{
	Super::BeginPlay();

	SetupCustomizableBodyPart(Torso);
	SetupCustomizableBodyPart(Legs);
	SetupCustomizableBodyPart(Feet);

	if (USkeletalMeshComponent* FaceSkelMeshComponent = GetSkelMeshComponentByName(FaceComponentName))
	{
		PostInitAnimBP(FaceSkelMeshComponent, FaceSkelMeshComponent->GetPostProcessInstance());
	}

	if (USkeletalMeshComponent* BodySkelMeshComponent = GetBodySkelMeshComponent())
	{
		UAnimInstance* AnimInstance = BodySkelMeshComponent->GetPostProcessInstance();
		if (AnimInstance)
		{
			MetaHumanComponentHelpers::ConnectVariable<FBoolProperty, bool>(AnimInstance, TEXT("Enable Body Correctives"), bEnableBodyCorrectives);
		}
	}
}

void UMetaHumanComponentUE::OnUnregister()
{
	Super::OnUnregister();
}

void UMetaHumanComponentUE::SetupCustomizableBodyPart(FMetaHumanCustomizableBodyPart& BodyPart)
{
	USkeletalMeshComponent* BodyPartSkelMeshComponent = GetSkelMeshComponentByName(BodyPart.ComponentName);
	if (!BodyPartSkelMeshComponent)
	{
		return;
	}

	BodyPartSkelMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	// Retrieve the physics asset as well as the control rig set by the skeletal mesh asset.
	UPhysicsAsset* SkelMeshPhysicsAsset = nullptr;
	TSubclassOf<UControlRig> SkelMeshControlRigClass = nullptr;
	if (USkeletalMesh* SkeletalMeshAsset = BodyPartSkelMeshComponent->GetSkeletalMeshAsset())
	{
		if (TSubclassOf<UAnimInstance> PostProcessAnimBPClass = SkeletalMeshAsset->GetPostProcessAnimBlueprint())
		{
			if (UAnimInstance* DefaultAnimBP = PostProcessAnimBPClass.GetDefaultObject())
			{
				static constexpr FStringView OverridePhysicsAssetPropertyName = TEXTVIEW("Override Physics Asset");
				MetaHumanComponentHelpers::GetPropertyValue(DefaultAnimBP, OverridePhysicsAssetPropertyName, SkelMeshPhysicsAsset);

				static constexpr FStringView ControlRigClassPropertyName = TEXTVIEW("Control Rig Class");
				MetaHumanComponentHelpers::GetPropertyValue(DefaultAnimBP, ControlRigClassPropertyName, SkelMeshControlRigClass);
			}
		}
	}

	bool ShouldEvalInstancePostProcessAnimBP = (PostProcessAnimBP && (BodyPart.ControlRigClass || BodyPart.PhysicsAsset) && (BodyPart.PhysicsAsset != SkelMeshPhysicsAsset || BodyPart.ControlRigClass != SkelMeshControlRigClass));
	if (ShouldEvalInstancePostProcessAnimBP)
	{
		// Run post-processing AnimBP on the skeletal mesh component (instance) and overwrite the post-processing AnimBP that might be possibly set on the skeletal mesh asset.
		LoadAndRunAnimBP(PostProcessAnimBP, BodyPartSkelMeshComponent, /*IsPostProcessingAnimBP*/true, /*RunAsOverridePostAnimBP*/true);

		// Force nulling the leader pose component to disable following another skel mesh component's pose.
		// When using a post-processing AnimBP we use a copy pose from mesh anim graph node to sync the skeletons.
		BodyPartSkelMeshComponent->SetLeaderPoseComponent(nullptr);
	}
	else
	{
		if (SkelMeshPhysicsAsset || SkelMeshControlRigClass)
		{
			// Keep running the post-processing AnimBP from the skeletal mesh asset, hook into the variables so we can control its performance and LOD thresholds on the instance.
			PostConnectAnimBPVariables(BodyPart, BodyPartSkelMeshComponent, BodyPartSkelMeshComponent->GetPostProcessInstance());
		}

		if (USkeletalMesh* SkeletalMesh = BodyPartSkelMeshComponent->GetSkeletalMeshAsset(); IsValid(SkeletalMesh))
		{
			if (!SkeletalMesh->GetPostProcessAnimBlueprint() && !BodyPartSkelMeshComponent->GetAnimInstance())
			{
				// Didn't have a post-processing AnimBP and AnimBP running, use leader-follower pose.
				SetFollowBody(BodyPartSkelMeshComponent);
			}
		}
	}
}

void UMetaHumanComponentUE::PostInitAnimBP(USkeletalMeshComponent* SkeletalMeshComponent, UAnimInstance* AnimInstance) const
{
	if (!AnimInstance)
	{
		return;
	}

	UMetaHumanComponentBase::PostInitAnimBP(SkeletalMeshComponent, AnimInstance);

	PostConnectAnimBPVariables(Torso, SkeletalMeshComponent, AnimInstance);
	PostConnectAnimBPVariables(Legs, SkeletalMeshComponent, AnimInstance);
	PostConnectAnimBPVariables(Feet, SkeletalMeshComponent, AnimInstance);

	// Refresh the given skeletal mesh component and update the pose. This is needed to see an updated and correct pose
	// in the editor in case it is not ticking or in the game before the first tick. Otherwise any post-processing of the override AnimBPs won't be visible.
	SkeletalMeshComponent->TickAnimation(0.0f, false /*bNeedsValidRootMotion*/);
	SkeletalMeshComponent->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);
	SkeletalMeshComponent->RefreshBoneTransforms(nullptr /*TickFunction*/);
	SkeletalMeshComponent->RefreshFollowerComponents();
}
