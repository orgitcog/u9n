// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchAssetSamplerLibrary.h"
#include "Animation/AnimInstance.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkinnedAsset.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchMirrorDataCache.h"

FPoseSearchAssetSamplerPose UPoseSearchAssetSamplerLibrary::SamplePose(const UAnimInstance* AnimInstance, const FPoseSearchAssetSamplerInput Input)
{
	FPoseSearchAssetSamplerPose AssetSamplerPose;
	using namespace UE::PoseSearch;
	if (!Input.Animation)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchAssetSamplerLibrary::SamplePose invalid Input.Animation"));
	}
	else if (!AnimInstance)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchAssetSamplerLibrary::SamplePose invalid AnimInstance"));
	}
	else if (Input.bMirrored && !Input.MirrorDataTable)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchAssetSamplerLibrary::SamplePose unable to mirror the pose from %s at time %f because of invalid MirrorDataTable"), *Input.Animation->GetName(), Input.AnimationTime);
	}
	else
	{
		const FBoneContainer& BoneContainer = AnimInstance->GetRequiredBonesOnAnyThread();

		FMemMark Mark(FMemStack::Get());

		bool bPreProcessRootTransform = true;
		const FAnimationAssetSampler Sampler(Input.Animation, Input.RootTransformOrigin, Input.BlendParameters, Input.RootTransformSamplingRate, bPreProcessRootTransform);

		FBlendedCurve Curve;
		FCompactPose Pose;
		Pose.SetBoneContainer(&BoneContainer);

		Sampler.ExtractPose(Input.AnimationTime, Pose, Curve);
		AssetSamplerPose.RootTransform = Sampler.ExtractRootTransform(Input.AnimationTime);

		if (Input.bMirrored)
		{
			const FMirrorDataCache MirrorDataCache(Input.MirrorDataTable, BoneContainer);
			MirrorDataCache.MirrorPose(Pose);
			AssetSamplerPose.RootTransform = MirrorDataCache.MirrorTransform(AssetSamplerPose.RootTransform);
		}

		AssetSamplerPose.Pose.CopyBonesFrom(Pose);
		AssetSamplerPose.ComponentSpacePose.InitPose(AssetSamplerPose.Pose);
	}
	return AssetSamplerPose;
}

FTransform UPoseSearchAssetSamplerLibrary::GetTransform(FPoseSearchAssetSamplerPose& AssetSamplerPose, FCompactPoseBoneIndex CompactPoseBoneIndex, EPoseSearchAssetSamplerSpace Space)
{
	return GetTransform(AssetSamplerPose.ComponentSpacePose, AssetSamplerPose.RootTransform, CompactPoseBoneIndex, Space);
}

FTransform UPoseSearchAssetSamplerLibrary::GetTransformByName(UPARAM(ref) FPoseSearchAssetSamplerPose& AssetSamplerPose, FName BoneName, EPoseSearchAssetSamplerSpace Space)
{
	if (!AssetSamplerPose.Pose.IsValid())
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchAssetSamplerLibrary::GetTransformByName invalid AssetSamplerPose.Pose"));
		return FTransform::Identity;
	}

	const FBoneContainer& BoneContainer = AssetSamplerPose.Pose.GetBoneContainer();
	const USkeleton* Skeleton = BoneContainer.GetSkeletonAsset();

	FBoneReference BoneReference;
	BoneReference.BoneName = BoneName;
	BoneReference.Initialize(Skeleton);
	if (!BoneReference.HasValidSetup())
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchAssetSamplerLibrary::GetTransformByName invalid BoneName %s for Skeleton %s"), *BoneName.ToString(), *GetNameSafe(Skeleton));
		return FTransform::Identity;
	}

	const FCompactPoseBoneIndex CompactPoseBoneIndex = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(FSkeletonPoseBoneIndex(BoneReference.BoneIndex));
	if (!CompactPoseBoneIndex.IsValid())
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchAssetSamplerLibrary::GetTransformByName invalid FCompactPoseBoneIndex for BoneName %s for Skeleton %s"), *BoneName.ToString(), *GetNameSafe(Skeleton));
		return FTransform::Identity;
	}

	return GetTransform(AssetSamplerPose, CompactPoseBoneIndex, Space);
}

void UPoseSearchAssetSamplerLibrary::Draw(const UAnimInstance* AnimInstance, UPARAM(ref) FPoseSearchAssetSamplerPose& AssetSamplerPose)
{
	check(IsInGameThread());

#if ENABLE_DRAW_DEBUG
	static float DebugDrawSamplerRootAxisLength = 20.f;
	static float DebugDrawSamplerSize = 6.f;

	if (AnimInstance)
	{
		Draw(AnimInstance->GetWorld(), AssetSamplerPose.ComponentSpacePose, AssetSamplerPose.RootTransform);
	}
#endif // ENABLE_DRAW_DEBUG
}

#if ENABLE_VISUAL_LOG
void UPoseSearchAssetSamplerLibrary::VLogDraw(const UObject* VLogContext, const USkeletalMeshComponent* Mesh, const TCHAR* VLogName, const FColor Color, float DebugDrawSamplerRootAxisLength)
{
	check(IsInGameThread());

	if (!Mesh)
	{
		return;
	}
		
	const USkinnedAsset* SkinnedAsset = Mesh->GetSkinnedAsset();
	if (!SkinnedAsset)
	{
		return;
	}
	
	if (DebugDrawSamplerRootAxisLength > 0.f)
	{
		const FTransform& AxisWorldTransform = Mesh->GetComponentTransform();
		UE_VLOG_SEGMENT(VLogContext, VLogName, Display, AxisWorldTransform.GetTranslation(), AxisWorldTransform.GetTranslation() + AxisWorldTransform.GetScaledAxis(EAxis::X) * DebugDrawSamplerRootAxisLength, FColor::Red, TEXT(""));
		UE_VLOG_SEGMENT(VLogContext, VLogName, Display, AxisWorldTransform.GetTranslation(), AxisWorldTransform.GetTranslation() + AxisWorldTransform.GetScaledAxis(EAxis::Y) * DebugDrawSamplerRootAxisLength, FColor::Green, TEXT(""));
		UE_VLOG_SEGMENT(VLogContext, VLogName, Display, AxisWorldTransform.GetTranslation(), AxisWorldTransform.GetTranslation() + AxisWorldTransform.GetScaledAxis(EAxis::Z) * DebugDrawSamplerRootAxisLength, FColor::Blue, TEXT(""));
	}

	const int NumBones = Mesh->GetNumBones();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const int32 ParentBoneIndex = SkinnedAsset->GetRefSkeleton().GetParentIndex(BoneIndex);
		if (ParentBoneIndex != INDEX_NONE)
		{
			const FTransform BoneWorldTransform = Mesh->GetBoneTransform(BoneIndex);
			const FTransform ParentBoneWorldTransform = Mesh->GetBoneTransform(ParentBoneIndex);
			UE_VLOG_SEGMENT(VLogContext, VLogName, Display, BoneWorldTransform.GetTranslation(), ParentBoneWorldTransform.GetTranslation(), Color, TEXT(""));
		}
	}
}
#endif // ENABLE_VISUAL_LOG


