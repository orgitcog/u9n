// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigLogicInstanceData.h"
#include "RigLogicAnimNext.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "DNAAsset.h"
#include "LODPose.h"
#include "SharedRigRuntimeContext.h"

namespace UE::AnimNext
{
	void FRigLogicAnimNextInstanceData::Init(const UE::AnimNext::FReferencePose* ReferencePose)
	{
		if (!ReferencePose)
		{
			UE_LOG(LogRigLogicAnimNext, Error, TEXT("Reference pose invalid."));
			return;
		}

		USkeletalMesh* SkeletalMesh = const_cast<USkeletalMesh*>(ReferencePose->SkeletalMesh.Get());
		if (!SkeletalMesh)
		{
			UE_LOG(LogRigLogicAnimNext, Error, TEXT("No skeletal mesh assigned to reference pose."));
			return;
		}

		const USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
		if (!Skeleton)
		{
			UE_LOG(LogRigLogicAnimNext, Error, TEXT("No skeleton assigned to the skeletal mesh."));
			return;
		}

		UDNAAsset* DNAAsset = Cast<UDNAAsset>(SkeletalMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass()));
		if (!DNAAsset)
		{
			UE_LOG(LogRigLogicAnimNext, Warning, TEXT("No DNA asset assigned to the skeletal mesh."));
			return;
		}

		TSharedPtr<FSharedRigRuntimeContext> SharedRigRuntimeContext = DNAAsset->GetRigRuntimeContext();
		if (!SharedRigRuntimeContext.IsValid())
		{
			UE_LOG(LogRigLogicAnimNext, Error, TEXT("Can't get the shared rig runtime context."));
			return;
		}

		if (CachedRigRuntimeContext != SharedRigRuntimeContext)
		{
			CachedRigRuntimeContext = SharedRigRuntimeContext;
		}

		CachedDNAIndexMapping = MakeShared<FDNAIndexMapping>();
		CachedDNAIndexMapping->Init(DNAAsset->GetBehaviorReader().Get(), Skeleton, SkeletalMesh);

		NumLODs = CachedRigRuntimeContext->VariableJointIndicesPerLOD.Num();

		InitBoneIndexMapping(ReferencePose);
		InitSparseAndDenseDriverJointMapping(ReferencePose);
	}

	void FRigLogicAnimNextInstanceData::InitBoneIndexMapping(const UE::AnimNext::FReferencePose* ReferencePose)
	{
		RigLogicToSkeletonBoneIndexMappingPerLOD.Empty();
		RigLogicToSkeletonBoneIndexMappingPerLOD.SetNum(NumLODs);

		const TArrayView<const FBoneIndexType> MeshToPoseBoneIndexMap = ReferencePose->GetMeshBoneIndexToLODBoneIndexMap(); // mesh -> pose

		for (uint32 LODLevel = 0; LODLevel < NumLODs; ++LODLevel)
		{
			const TArray<uint16>& VariableJointIndices = CachedRigRuntimeContext->VariableJointIndicesPerLOD[LODLevel].Values;
			RigLogicToSkeletonBoneIndexMappingPerLOD[LODLevel].Reserve(VariableJointIndices.Num());

			for (const uint16 RigLogicJointIndex : VariableJointIndices)
			{
				// Get the mesh bone index from the RigLogic joint index. Index originating from a FindBoneName() on the mesh's reference skeleton.
				const FMeshPoseBoneIndex MeshBoneIndex = CachedDNAIndexMapping->JointsMapDNAIndicesToMeshPoseBoneIndices[RigLogicJointIndex];
				if (MeshBoneIndex.IsValid())
				{
					// Convert the skeleton bone index to a pose bone index for the given LOD level.
					const int32 PoseBoneIndex = MeshToPoseBoneIndexMap[MeshBoneIndex.GetInt()];
					if (ReferencePose->IsBoneEnabled(PoseBoneIndex, LODLevel))
					{
						RigLogicToSkeletonBoneIndexMappingPerLOD[LODLevel].Add({ RigLogicJointIndex, PoseBoneIndex });
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Could not find bone in skeleton for RigLogic joint with index %i."), RigLogicJointIndex);
				}
			}
		}
	}

	void FRigLogicAnimNextInstanceData::InitSparseAndDenseDriverJointMapping(const UE::AnimNext::FReferencePose* ReferencePose)
	{
		// Populate driver joint to raw control attribute mapping (used to feed RigLogic with inputs from the joint hierarchy)
		SparseDriverJointsToControlAttributesMapPerLOD.Empty();
		DenseDriverJointsToControlAttributesMapPerLOD.Empty();

		SparseDriverJointsToControlAttributesMapPerLOD.SetNum(NumLODs);
		DenseDriverJointsToControlAttributesMapPerLOD.SetNum(NumLODs);

		const TArrayView<const FBoneIndexType> MeshToPoseBoneIndexMap = ReferencePose->GetMeshBoneIndexToLODBoneIndexMap(); // mesh -> pose
		
		for (uint32 LODLevel = 0; LODLevel < NumLODs; ++LODLevel)
		{
			// Sparse mapping will likely remain empty so no reservation happens
			for (const auto& Mapping : CachedDNAIndexMapping->DriverJointsToControlAttributesMap)
			{
				const FMeshPoseBoneIndex MeshBoneIndex = Mapping.MeshPoseBoneIndex;
				if (MeshBoneIndex.IsValid())
				{
					// Convert the mesh bone index to a pose bone index for the given LOD level.
					const int32 PoseBoneIndex = MeshToPoseBoneIndexMap[MeshBoneIndex.GetInt()];
					if (ReferencePose->IsBoneEnabled(PoseBoneIndex, LODLevel))
					{
						if ((Mapping.RotationX != INDEX_NONE) && (Mapping.RotationY != INDEX_NONE) && (Mapping.RotationZ != INDEX_NONE) && (Mapping.RotationW != INDEX_NONE))
						{
							DenseDriverJointsToControlAttributesMapPerLOD[LODLevel].Add({ PoseBoneIndex, Mapping.DNAJointIndex, Mapping.RotationX, Mapping.RotationY, Mapping.RotationZ, Mapping.RotationW });
						}
						else
						{
							SparseDriverJointsToControlAttributesMapPerLOD[LODLevel].Add({ PoseBoneIndex, Mapping.DNAJointIndex, Mapping.RotationX, Mapping.RotationY, Mapping.RotationZ, Mapping.RotationW });
						}
					}
				}
			}
		}
	}
} // namespace UE::AnimNext
