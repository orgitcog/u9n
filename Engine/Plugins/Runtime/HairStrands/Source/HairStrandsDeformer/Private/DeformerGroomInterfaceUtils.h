// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GroomComponent.h"
#include "GroomSolverComponent.h"
#include "RenderGraphBuilder.h"
#include "SkeletalMeshDeformerHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkeletalRenderPublic.h"
#include "SkinnedMeshSceneProxyDesc.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "DeformerGroomDomainsSource.h"

namespace UE::Groom::Private
{
	FORCEINLINE bool HasDeformationEnabledOrHasMeshDeformer(const UGroomComponent* GroomComponent, const int32 GroupIndex)
	{
		return GroomComponent->IsDeformationEnable(GroupIndex);
	}

	FORCEINLINE USkeletalMeshComponent* GetGroupSkelMesh(const UGroomComponent* GroomComponent, const int32 GroupIndex, int32& MeshLOD)
	{
		if(GroomComponent)
		{
			TArray<UActorComponent*> ActorComponents;
			if(AActor* RootActor = GroomComponent->GetAttachmentRootActor())
			{
				RootActor->GetComponents(USkeletalMeshComponent::StaticClass(), ActorComponents);
			}
			for(UActorComponent* ActorComponent : ActorComponents)
			{
				if(USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ActorComponent))
				{
					if(GroomComponent->GroomAsset && (SkeletalMeshComponent->GetSkeletalMeshAsset() ==
						GroomComponent->GroomAsset->GetDataflowSettings().GetSkeletalMesh(GroupIndex)))
					{
						MeshLOD = GroomComponent->GroomAsset->GetDataflowSettings().GetMeshLOD(GroupIndex);
						return SkeletalMeshComponent;
					}
				}
			}
		}
		MeshLOD = INDEX_NONE;
		return nullptr;
	}
	
	FORCEINLINE void GroomComponentsToInstances(const TArray<const UGroomComponent*>& GroomComponents, TArray<const FHairGroupInstance*>& GroupInstances)
 	{
 		GroupInstances.Reset();
 		for(const UGroomComponent* GroomComponent : GroomComponents)
 		{
 			if(GroomComponent)
 			{
 				const uint32 NumGroups = GroomComponent->GetGroupCount();
 				for (uint32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
 				{
 					if(HasDeformationEnabledOrHasMeshDeformer(GroomComponent, GroupIndex) && GroomComponent->GetGroupInstance(GroupIndex))
 					{
 						GroupInstances.Add(GroomComponent->GetGroupInstance(GroupIndex));
 					}
 				}
 			}
 		}
 	}
	FORCEINLINE void GroomComponentsToSkelmeshes(const TArray<const UGroomComponent*>& GroomComponents, TArray<const FSkeletalMeshObject*>& SkeletalMeshes, TArray<FMatrix44f>& SkeletalTransforms,
		TArray<TArray<FMatrix44f>>& BonesRefToLocals, TArray<const FHairGroupInstance*>& GroupInstances)
	{
		SkeletalMeshes.Reset();
		SkeletalTransforms.Reset();
		BonesRefToLocals.Reset();
		GroupInstances.Reset();

		TArray<FTransform> BonesTransforms;
		TArray<FMatrix44f> RefToLocals;
		int32 MeshLOD = INDEX_NONE;
		
		for(const UGroomComponent* GroomComponent : GroomComponents)
		{
			if(GroomComponent)
			{
				const uint32 NumGroups = GroomComponent->GetGroupCount();
				for (uint32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
				{
					if(GroomComponent->IsDeformationEnable(GroupIndex) && GroomComponent->GetGroupInstance(GroupIndex))
					{
						if(USkeletalMeshComponent* SkelMesh =  GetGroupSkelMesh(GroomComponent, GroupIndex, MeshLOD))
						{
							if(USkinnedAsset* SkinnedAsset = SkelMesh->GetSkinnedAsset())
							{
								const FSkeletalMeshRenderData* const RenderData = SkinnedAsset->GetResourceForRendering();
								if(RenderData && RenderData->LODRenderData.IsValidIndex(MeshLOD))
								{
									// Get the matching active indices used for skinning
									TArray<FBoneIndexType> GuidesBones = RenderData->LODRenderData[MeshLOD].ActiveBoneIndices;
								
									// Get inv ref pose matrices
									const TArray<FMatrix44f>* RefBasesInvMatrix = &SkelMesh->GetSkinnedAsset()->GetRefBasesInvMatrix();

									// Get the component space transforms
									BonesTransforms.Init(FTransform::Identity, RefBasesInvMatrix->Num());
								
									SkelMesh->GetSkinnedAsset()->FillComponentSpaceTransforms(
										SkelMesh->GetBoneSpaceTransforms(), GuidesBones, BonesTransforms);

									// Fill ref to local matrices
									RefToLocals.Init(FMatrix44f::Identity, RefBasesInvMatrix->Num());
								
									for (int32 BoneIndex = 0; BoneIndex < RefToLocals.Num(); ++BoneIndex)
									{
										RefToLocals[BoneIndex] = (*RefBasesInvMatrix)[BoneIndex] * (FMatrix44f)BonesTransforms[BoneIndex].ToMatrixWithScale();
									}
							
									const FTransform& BonesTransform = SkelMesh->GetComponentTransform();
									const FTransform& GroupTransform = GroomComponent->GetGroupInstance(GroupIndex)->GetCurrentLocalToWorld();
									//SkelMesh->GetCurrentRefToLocalMatrices(RefToLocals, SkelMesh->MeshObject->GetLOD(), &RequiredExtraBoneIndices);//MeshLOD);
		
									SkeletalTransforms.Add(FMatrix44f(BonesTransform.ToMatrixWithScale() * GroupTransform.ToInverseMatrixWithScale()));
									BonesRefToLocals.Add(RefToLocals);
									SkeletalMeshes.Add(SkelMesh->MeshObject);
									GroupInstances.Add(GroomComponent->GetGroupInstance(GroupIndex));
								}
							}
						}
					}
				}
			}
		}
	}
	
	FORCEINLINE void GatherGroomComponents(const UActorComponent* ActorComponent, TArray<const UGroomComponent*>& GroomComponents)
	{
		GroomComponents.Reset();
		if(const UGroomComponent* GroomComponent = Cast<UGroomComponent>(ActorComponent))
		{
			GroomComponents.Add(GroomComponent);
		}
		else if(const UGroomSolverComponent* GroomSolver = Cast<UGroomSolverComponent>(ActorComponent))
		{
			for(const TObjectPtr<UGroomComponent>& SolverGroom : GroomSolver->GetGroomComponents())
			{
				GroomComponents.Add(SolverGroom);
			}
		}
	}
	
	FORCEINLINE void GatherGroupInstances(const UActorComponent* ActorComponent, TArray<const FHairGroupInstance*>& GroupInstances)
	{
		TArray<const UGroomComponent*> GroomComponents;
		GatherGroomComponents(ActorComponent, GroomComponents);

		GroomComponentsToInstances(GroomComponents, GroupInstances);
	}

	FORCEINLINE void GatherGroupSkelmeshes(const UActorComponent* ActorComponent, TArray<const FSkeletalMeshObject*>& SkeletalMeshes, TArray<FMatrix44f>& SkeletalTransforms, TArray<TArray<FMatrix44f>>& BonesRefToLocals, TArray<const FHairGroupInstance*>& GroupInstances)
	{
		TArray<const UGroomComponent*> GroomComponents;
		GatherGroomComponents(ActorComponent, GroomComponents);

		GroomComponentsToSkelmeshes(GroomComponents, SkeletalMeshes, SkeletalTransforms, BonesRefToLocals, GroupInstances);
	}

	template<typename DataType>
	FORCEINLINE bool HaveValidInstanceResources(const DataType& InstanceData)
	{
		if(InstanceData.RestResource && InstanceData.DeformedResource)
		{
			if(InstanceData.RestResource->IsInitialized() && InstanceData.RestResource->bIsInitialized &&
			   InstanceData.DeformedResource->IsInitialized() && InstanceData.DeformedResource->bIsInitialized)
			{
				return true;
			}
		}
		return false;
	}

	FORCEINLINE bool HaveGuidesInstanceResources(const TArray<const FHairGroupInstance*>& GroupInstances)
	{
		for(const FHairGroupInstance* GroupInstance : GroupInstances)
		{
			if(!GroupInstance || (GroupInstance && !HaveValidInstanceResources(GroupInstance->Guides)))
			{
				return false;
			}
		}
		return true;
	}

	FORCEINLINE bool HaveStrandsInstanceResources(const TArray<const FHairGroupInstance*>& GroupInstances)
	{
		for(const FHairGroupInstance* GroupInstance : GroupInstances)
		{
			if(!GroupInstance || (GroupInstance && !HaveValidInstanceResources(GroupInstance->Strands)))
			{
				return false;
			}
		}
		return true;
	}

	template<typename DataType>
	FORCEINLINE bool HaveValidSkinnedResources(const DataType& SkinnedData, const int32 LODIndex)
	{
		if(SkinnedData.HasValidRootData() && (SkinnedData.RestRootResource->GetRootCount() > 0) &&
		  (SkinnedData.RestRootResource->LODs.Num() == SkinnedData.DeformedRootResource->LODs.Num()))
		{
			if(LODIndex < SkinnedData.RestRootResource->LODs.Num())
			{
				FHairStrandsLODRestRootResource* RestLODDatas = SkinnedData.RestRootResource->LODs[LODIndex];
				FHairStrandsLODDeformedRootResource* DeformedLODDatas = SkinnedData.DeformedRootResource->LODs[LODIndex];

				return (RestLODDatas && RestLODDatas->IsValid() && DeformedLODDatas && DeformedLODDatas->IsValid());
			}
		}
		return false;
	}

	FORCEINLINE bool HaveGuidesSkinnedResources(const TArray<const FHairGroupInstance*>& GroupInstances)
	{
		for(const FHairGroupInstance* GroupInstance : GroupInstances)
		{
			if(GroupInstance && GroupInstance->HairGroupPublicData)
			{
				if(GroupInstance->BindingType == EHairBindingType::Skinning)
				{
					if(!HaveValidSkinnedResources(GroupInstance->Guides, GroupInstance->HairGroupPublicData->MeshLODIndex ))
					{
						return false;
					}
				}
			}
			else
			{
				return false;
			}
		}
		return true;
	}

	FORCEINLINE bool HaveStrandsSkinnedResources(const TArray<const FHairGroupInstance*>& GroupInstances)
	{
		for(const FHairGroupInstance* GroupInstance : GroupInstances)
		{
			if(GroupInstance && GroupInstance->HairGroupPublicData)
			{
				if(GroupInstance->BindingType == EHairBindingType::Skinning)
				{
					if(!HaveValidSkinnedResources(GroupInstance->Strands, GroupInstance->HairGroupPublicData->MeshLODIndex ))
					{
						return false;
					}
				}
			}
			else
			{
				return false;
			}
		}
		return true;
	}

	FORCEINLINE void GetGroomGroupElementOffsets(const UGroomComponent* GroomComponent, const FName DomainName, TArray<int32>& GroupOffsets, const int32 GroupSize = INDEX_NONE, const bool bSourceElements = false)
	{
		if(GroomComponent) 
		{
			const uint32 NumGroups = GroomComponent->GetGroupCount();
			const float GroupAlignment = static_cast<float>(GroupSize);
			int32 MeshLOD = INDEX_NONE;
			
			GroupOffsets.Init(0, NumGroups+1);
			int32 NumElements = 0;
			for (uint32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
			{
				GroupOffsets[GroupIndex] = NumElements;
				
				if(DomainName == UOptimusGroomAssetComponentSource::FStrandsExecutionDomains::Objects ||
				   DomainName == UOptimusGroomAssetComponentSource::FGuidesExecutionDomains::Objects)
				{
					NumElements++;
				}
				if(const USkeletalMeshComponent* SkelMesh =  GetGroupSkelMesh(GroomComponent,GroupIndex, MeshLOD))
				{
					if (SkelMesh->MeshObject)
					{
						if(DomainName == UOptimusGroomAssetComponentSource::FMeshesExecutionDomains::Bones)
						{
							NumElements += SkelMesh->MeshObject->GetReferenceToLocalMatrices().Num();
						}
						else if(DomainName == UOptimusGroomAssetComponentSource::FMeshesExecutionDomains::Vertices)
						{
							NumElements += 0; // @todo : compute the total number of vertices to modify
						}
					}
				}
				
				if (const FHairGroupInstance* Instance = GroomComponent->GetGroupInstance(GroupIndex))
				{
					if(Instance->Strands.IsValid())
					{
						if(DomainName == UOptimusGroomAssetComponentSource::FStrandsExecutionDomains::Curves)
						{
							NumElements += bSourceElements ? Instance->Strands.GetData().GetNumSourceCurves() : Instance->Strands.GetData().GetNumCurves();
						}
						else if (DomainName == UOptimusGroomAssetComponentSource::FStrandsExecutionDomains::Edges)
						{
							NumElements +=  bSourceElements ? Instance->Strands.GetData().GetNumSourcePoints() - Instance->Strands.GetData().GetNumSourceCurves() :
									Instance->Strands.GetData().GetNumPoints() - Instance->Strands.GetData().GetNumCurves();
						}
						else if (DomainName == UOptimusGroomAssetComponentSource::FStrandsExecutionDomains::Points)
						{
							NumElements += bSourceElements ? Instance->Strands.GetData().GetNumSourcePoints() : Instance->Strands.GetData().GetNumPoints();
						}
					}
					if(Instance->Guides.IsValid())
					{
						if(DomainName == UOptimusGroomAssetComponentSource::FGuidesExecutionDomains::Curves)
						{
							NumElements += bSourceElements ? Instance->Guides.GetData().GetNumSourceCurves() : Instance->Guides.GetData().GetNumCurves();
						}
						else if (DomainName == UOptimusGroomAssetComponentSource::FGuidesExecutionDomains::Edges)
						{
							NumElements += bSourceElements ? Instance->Guides.GetData().GetNumSourcePoints() - Instance->Guides.GetData().GetNumSourceCurves() :
								Instance->Guides.GetData().GetNumPoints() - Instance->Guides.GetData().GetNumCurves();
						}
						else if (DomainName == UOptimusGroomAssetComponentSource::FGuidesExecutionDomains::Points)
						{
							NumElements += bSourceElements ? Instance->Guides.GetData().GetNumSourcePoints() : Instance->Guides.GetData().GetNumPoints();
						}
					}
				}
				NumElements = (GroupSize == INDEX_NONE) ? NumElements :
				   FMath::CeilToInt32(NumElements / GroupAlignment) * GroupAlignment;
			}
			GroupOffsets[NumGroups] = NumElements;
		}
	}

	FORCEINLINE int32 GetGroomInvocationElementCounts(const TArray<const UGroomComponent*>& GroomComponents, const FName DomainName, TArray<int32>& InvocationCounts, int32 GroupSize = INDEX_NONE, const bool bSourceElements = false)
	{
		TArray<int32> GroupOffsets;
		int32 TotalCount = 0;
		InvocationCounts.Reset();
		for(const UGroomComponent* GroomComponent : GroomComponents)
		{
			if(GroomComponent)
			{
				GetGroomGroupElementOffsets(GroomComponent, DomainName, GroupOffsets, GroupSize, bSourceElements);

				const uint32 NumGroups = GroomComponent->GetGroupCount();
				for (uint32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
				{
					if(HasDeformationEnabledOrHasMeshDeformer(GroomComponent, GroupIndex) && GroomComponent->GetGroupInstance(GroupIndex))
					{
						InvocationCounts.Add(GroupOffsets[GroupIndex+1]-GroupOffsets[GroupIndex]);
						TotalCount += InvocationCounts.Last();
					}
				}
			}
		}
		return TotalCount;
	}

	struct FGroupElements
	{
		TArray<int32> GroupIndices;
		TArray<int32> GroupOffsets;
		TArray<const FHairGroupInstance*> GroupInstances;
	};
	
	FORCEINLINE void GetGroomInvocationElementGroups(const TArray<const UGroomComponent*>& GroomComponents, const FName DomainName, TArray<TPair<UGroomAsset*, FGroupElements>>& InvocationGroups, int32 GroupSize = INDEX_NONE, const bool bSourceElements = false)
	{
		InvocationGroups.Reset();
		for(const UGroomComponent* GroomComponent : GroomComponents)
		{
			if(GroomComponent)
			{
				FGroupElements GroupData;
				GetGroomGroupElementOffsets(GroomComponent, DomainName, GroupData.GroupOffsets, GroupSize, bSourceElements);
				
				const uint32 NumGroups = GroomComponent->GetGroupCount();
				for (uint32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
				{ 
					if(HasDeformationEnabledOrHasMeshDeformer(GroomComponent, GroupIndex) && GroomComponent->GetGroupInstance(GroupIndex))
					{
						GroupData.GroupIndices.Add(GroupIndex);
						GroupData.GroupInstances.Add(GroomComponent->GetGroupInstance(GroupIndex));
					}
				}
				InvocationGroups.Add(MakeTuple(GroomComponent->GroomAsset, GroupData));
			}
			
		}
	}
}
