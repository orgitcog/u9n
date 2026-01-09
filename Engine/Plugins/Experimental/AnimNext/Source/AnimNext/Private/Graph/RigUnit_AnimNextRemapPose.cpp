// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_AnimNextRemapPose.h"

#include "GenerationTools.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AnimNextRemapPose)
DEFINE_STAT(STAT_AnimNext_RigUnit_RemapPose);

FRigUnit_AnimNextRemapPose_Execute()
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_RigUnit_RemapPose);

	using namespace UE::AnimNext;

	if(!TargetAnimGraphRefPose.ReferencePose.IsValid())
	{
		return;
	}
	
	if(!Pose.LODPose.IsValid())
	{
		return;
	}

	FMemMark MemMark(FMemStack::Get());

	const UE::AnimNext::FLODPoseHeap& SourcePose = Pose.LODPose;
	const UE::AnimNext::FReferencePose& SourceRefPose = SourcePose.GetRefPose();
	const USkeletalMesh* SourceMesh = SourceRefPose.SkeletalMesh.Get();

	const UE::AnimNext::FReferencePose& TargetRefPose = TargetAnimGraphRefPose.ReferencePose.GetRef<UE::AnimNext::FReferencePose>();
	UE::AnimNext::FLODPoseHeap& TargetPose = Result.LODPose;
	const USkeletalMesh* TargetMesh = TargetRefPose.SkeletalMesh.Get();

	if (SourceMesh == TargetMesh)
	{
		TargetPose.PrepareForLOD(TargetRefPose, SourcePose.LODLevel, /*bSetRefPose=*/false, SourcePose.IsAdditive());
		// Just copy the pose to the target in case there is nothing to convert.
		TargetPose.CopyFrom(SourcePose);
	}
	else
	{
		// Pre-create the mapping and cache it to avoid runtime lookups.
		if (RemapPoseData.ShouldReinit(SourceRefPose, TargetRefPose))
		{
			RemapPoseData.Reinit(SourceRefPose, TargetRefPose);
		}

		Result.Curves.CopyFrom(Pose.Curves);
		RemapPoseData.RemapPose(SourcePose, TargetPose);
		RemapPoseData.RemapAttributes(Pose.LODPose, Pose.Attributes, Result.LODPose, Result.Attributes);
	}
}