// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_GetPostProcessAnimation.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Graph/PostProcessAnimationAssetUserData.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/AnimNextSkeletalMeshComponentReferenceComponent.h"

FRigUnit_GetPostProcessAnimation_Execute()
{
	Graph = {};
	bShouldEvaluate = true;
	LODThreshold = -1;

	USkeletalMeshComponent* UsedSkeletalMeshComponent = SkeletalMeshComponent;
	if (!UsedSkeletalMeshComponent)
	{
		const FAnimNextModuleContextData& ModuleContextData = ExecuteContext.GetContextData<FAnimNextModuleContextData>();
		FAnimNextModuleInstance& ModuleInstance = ModuleContextData.GetModuleInstance();
		const FAnimNextSkeletalMeshComponentReferenceComponent& ComponentReference = ModuleInstance.GetComponent<FAnimNextSkeletalMeshComponentReferenceComponent>();
		UsedSkeletalMeshComponent = ComponentReference.GetComponent();
	}

	if (UsedSkeletalMeshComponent)
	{
		USkeletalMesh* SkeletalMesh = UsedSkeletalMeshComponent->GetSkeletalMeshAsset();
		if (SkeletalMesh)
		{
			UPostProcessAnimationUserAssetData* UserAssetData = Cast<UPostProcessAnimationUserAssetData>(SkeletalMesh->GetAssetUserDataOfClass(UPostProcessAnimationUserAssetData::StaticClass()));
			if (UserAssetData)
			{
				Graph.Asset = UserAssetData->AnimationAsset;
			}

			LODThreshold = SkeletalMesh->GetPostProcessAnimGraphLODThreshold();
			bShouldEvaluate = SkeletalMesh->ShouldEvaluatePostProcessAnimGraph(LODLevel);
		}
	}
}