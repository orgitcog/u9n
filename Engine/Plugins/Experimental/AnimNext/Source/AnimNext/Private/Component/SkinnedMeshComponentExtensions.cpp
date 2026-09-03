// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/SkinnedMeshComponentExtensions.h"
#include "GenerationTools.h"
#include "Components/SkinnedMeshComponent.h"
#include "Animation/AnimTrace.h"
#include "Module/AnimNextModuleInstance.h"

namespace UE::Anim
{

void FSkinnedMeshComponentExtensions::CompleteAndDispatch(
	USkinnedMeshComponent* InComponent,
	TConstArrayView<FBoneIndexType> InParentIndices,
	TConstArrayView<FBoneIndexType> InRequiredBoneIndices,
	TConstArrayView<FTransform> InLocalSpaceTransforms)
{
	// Fill the component space transform buffer
	TArrayView<FTransform> ComponentSpaceTransforms = InComponent->GetEditableComponentSpaceTransforms();
	if (ComponentSpaceTransforms.Num() > 0)
	{
		UE::AnimNext::FGenerationTools::ConvertLocalSpaceToComponentSpace(InParentIndices, InLocalSpaceTransforms, InRequiredBoneIndices, ComponentSpaceTransforms);

		// Flag buffer for flip
		InComponent->bNeedToFlipSpaceBaseBuffers = true;

		InComponent->FlipEditableSpaceBases();
		InComponent->bHasValidBoneTransform = true;

		InComponent->InvalidateCachedBounds();
		InComponent->UpdateBounds();

		// Send updated transforms & bounds to the renderer
		InComponent->SendRenderDynamicData_Concurrent();
		InComponent->SendRenderTransform_Concurrent();

		if (InComponent->bHasSocketAttachments)
		{
			FAnimNextModuleInstance::RunTaskOnGameThread([WeakComponent = TWeakObjectPtr<USkinnedMeshComponent>(InComponent)]()
			{
				SCOPED_NAMED_EVENT(AnimNext_SkinnedMesh_CompleteAndDispatch_GameThread, FColor::Orange);
				USkinnedMeshComponent* Component = WeakComponent.Get();
				if (Component == nullptr)
				{
					return;
				}

				Component->UpdateChildTransforms(EUpdateTransformFlags::OnlyUpdateIfUsingSocket);
			});
		}
		
		if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InComponent))
		{
			TRACE_SKELETAL_MESH_COMPONENT(SkeletalMeshComponent);
		}
	}
}

}
