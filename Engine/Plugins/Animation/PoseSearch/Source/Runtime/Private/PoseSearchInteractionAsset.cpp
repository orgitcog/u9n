// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchInteractionAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchMirrorDataCache.h"

bool UPoseSearchInteractionAsset::IsLooping() const
{
	float CommonPlayLength = -1.f;
	for (const FPoseSearchInteractionAssetItem& Item : Items)
	{
		if (const UAnimationAsset* AnimationAsset = Item.Animation.Get())
		{
			if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimationAsset))
			{
				if (!SequenceBase->bLoop)
				{
					return false;
				}
			}
			else if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset))
			{
				if (!BlendSpace->bLoop)
				{
					return false;
				}
			}
			else
			{
				unimplemented();
			}

			if (CommonPlayLength < 0.f)
			{
				CommonPlayLength = AnimationAsset->GetPlayLength();
			}
			else if (!FMath::IsNearlyEqual(CommonPlayLength, AnimationAsset->GetPlayLength()))
			{
				return false;
			}
		}
	}
	return true;
}

bool UPoseSearchInteractionAsset::HasRootMotion() const
{
	bool bHasAtLeastOneValidItem = false;
	bool bHasRootMotion = true;

	for (const FPoseSearchInteractionAssetItem& Item : Items)
	{
		if (const UAnimationAsset* AnimationAsset = Item.Animation.Get())
		{
			if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimationAsset))
			{
				bHasRootMotion &= SequenceBase->HasRootMotion();
			}
			else if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset))
			{
				BlendSpace->ForEachImmutableSample([&bHasRootMotion](const FBlendSample& Sample)
				{
					if (const UAnimSequence* Sequence = Sample.Animation.Get())
					{
						bHasRootMotion &= Sequence->HasRootMotion();
					}
				});
			}
			else
			{
				unimplemented();
			}
			bHasAtLeastOneValidItem = true;
		}
	}

	return bHasAtLeastOneValidItem && bHasRootMotion;
}

float UPoseSearchInteractionAsset::GetPlayLength(const FVector& BlendParameters) const
{
	float MaxPlayLength = 0.f;
	for (const FPoseSearchInteractionAssetItem& Item : Items)
	{
		if (const UAnimationAsset* AnimationAsset = Item.Animation.Get())
		{
			if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset))
			{
				int32 TriangulationIndex = 0;
				TArray<FBlendSampleData> BlendSamples;
				BlendSpace->GetSamplesFromBlendInput(BlendParameters, BlendSamples, TriangulationIndex, true);
				const float PlayLength = BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);
				MaxPlayLength = FMath::Max(MaxPlayLength, PlayLength);
			}
			else
			{
				MaxPlayLength = FMath::Max(MaxPlayLength, AnimationAsset->GetPlayLength());
			}
		}
	}
	return MaxPlayLength;
}

FQuat UPoseSearchInteractionAsset::FindReferenceOrientationNoBanking(const TArrayView<const FTransform> Transforms, const TArrayView<int32> SortedByWarpingWeightRotationItemIndex, const TArrayView<float> NormalizedWarpingWeightRotation) const
{
	check(Items.Num() > 0);
	check(Items.Num() == Transforms.Num());
	check(Items.Num() == SortedByWarpingWeightRotationItemIndex.Num());
	check(Items.Num() == NormalizedWarpingWeightRotation.Num());

	// @todo: use a Slerp or a proper FastLerp
	// for now we don't account for the shortest path while FastLerping those queternion together
	FQuat WeightedQuaternion = FQuat::Identity * 0.f;
	for (int32 ItemIndex : SortedByWarpingWeightRotationItemIndex)
	{
		WeightedQuaternion += Transforms[ItemIndex].GetRotation() * NormalizedWarpingWeightRotation[ItemIndex];
	}
	WeightedQuaternion.Normalize();
	return WeightedQuaternion;
}

FQuat UPoseSearchInteractionAsset::FindReferenceOrientationFullBanking(const TArrayView<const FTransform> Transforms, const TArrayView<int32> SortedByWarpingWeightRotationItemIndex, const TArrayView<float> NormalizedWarpingWeightRotation) const
{
	const int32 ItemsNum = Items.Num();

	check(ItemsNum > 0);
	check(ItemsNum == Transforms.Num());
	check(ItemsNum == SortedByWarpingWeightRotationItemIndex.Num());
	check(ItemsNum == NormalizedWarpingWeightRotation.Num());

	if (ItemsNum > 1)
	{
		const int32 LastItemIndex = ItemsNum - 1;
		FVector OtherItemsPositionsSum = Transforms[SortedByWarpingWeightRotationItemIndex[0]].GetTranslation();
		for (int32 ItemIndex = 1; ItemIndex < LastItemIndex; ++ItemIndex)
		{
			OtherItemsPositionsSum += Transforms[SortedByWarpingWeightRotationItemIndex[ItemIndex]].GetTranslation();
		}

		const FVector OtherItemsPositionAverage = OtherItemsPositionsSum / LastItemIndex;
		const FVector DeltaPosition = OtherItemsPositionAverage - Transforms[SortedByWarpingWeightRotationItemIndex[LastItemIndex]].GetTranslation();

		if (!DeltaPosition.IsNearlyZero())
		{
			return DeltaPosition.ToOrientationQuat();
		}
	}

	return FindReferenceOrientationNoBanking(Transforms, SortedByWarpingWeightRotationItemIndex, NormalizedWarpingWeightRotation);
}

FQuat UPoseSearchInteractionAsset::FindReferenceOrientation(const TArrayView<const FTransform> Transforms, const TArrayView<int32> SortedByWarpingWeightRotationItemIndex, const TArrayView<float> NormalizedWarpingWeightRotation) const
{
	if (WarpingBankingWeight < UE_KINDA_SMALL_NUMBER)
	{
		return FindReferenceOrientationNoBanking(Transforms, SortedByWarpingWeightRotationItemIndex, NormalizedWarpingWeightRotation);
	}

	if (WarpingBankingWeight > 1.f - UE_KINDA_SMALL_NUMBER)
	{
		return FindReferenceOrientationFullBanking(Transforms, SortedByWarpingWeightRotationItemIndex, NormalizedWarpingWeightRotation);
	}

	return FQuat::Slerp(
		FindReferenceOrientationNoBanking(Transforms, SortedByWarpingWeightRotationItemIndex, NormalizedWarpingWeightRotation),
		FindReferenceOrientationFullBanking(Transforms, SortedByWarpingWeightRotationItemIndex, NormalizedWarpingWeightRotation),
		WarpingBankingWeight);
}

FVector UPoseSearchInteractionAsset::FindReferencePosition(const TArrayView<const FTransform> Transforms, const TArrayView<float> NormalizedWarpingWeightTranslation) const
{
	const int32 ItemsNum = Items.Num();
	
	check(ItemsNum > 0);
	check(Transforms.Num() == ItemsNum);
	check(Transforms.Num() == NormalizedWarpingWeightTranslation.Num());

	FVector PositionsSum = FVector::ZeroVector;
	for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
	{
		PositionsSum += Transforms[ItemIndex].GetTranslation() * NormalizedWarpingWeightTranslation[ItemIndex];
	}

	return PositionsSum;
}

UAnimationAsset* UPoseSearchInteractionAsset::GetAnimationAsset(const UE::PoseSearch::FRole& Role) const
{
	for (const FPoseSearchInteractionAssetItem& Item : Items)
	{
		if (Item.Role == Role)
		{
			return Item.Animation;
		}
	}
	return nullptr;
}

FTransform UPoseSearchInteractionAsset::GetOrigin(const UE::PoseSearch::FRole& Role) const
{
	for (const FPoseSearchInteractionAssetItem& Item : Items)
	{
		if (Item.Role == Role)
		{
			return Item.Origin;
		}
	}
	return FTransform::Identity;
}

#if WITH_EDITOR
FTransform UPoseSearchInteractionAsset::GetDebugWarpOrigin(const UE::PoseSearch::FRole& Role, bool bComposeWithDebugWarpOffset) const
{
	for (int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
	{
		const FPoseSearchInteractionAssetItem& Item = Items[ItemIndex];
		if (Item.Role == Role)
		{
#if WITH_EDITORONLY_DATA
			if (bComposeWithDebugWarpOffset && bEnableDebugWarp && DebugWarpOffsets.IsValidIndex(ItemIndex))
			{
				return DebugWarpOffsets[ItemIndex] * Item.Origin;
			}
#endif // WITH_EDITORONLY_DATA

			return Item.Origin;
		}
	}
	return FTransform::Identity;
}

USkeletalMesh* UPoseSearchInteractionAsset::GetPreviewMesh(const UE::PoseSearch::FRole& Role) const
{
	for (int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
	{
		const FPoseSearchInteractionAssetItem& Item = Items[ItemIndex];
		if (Item.Role == Role)
		{
			return Item.PreviewMesh.Get();
		}
	}
	return nullptr;
}

#endif // WITH_EDITOR

void UPoseSearchInteractionAsset::CalculateWarpTransforms(float Time, const TConstArrayView<const FTransform> ActorRootBoneTransforms, TArrayView<FTransform> FullAlignedActorRootBoneTransforms, const TConstArrayView<const UMirrorDataTable*> MirrorDataTables, const TConstArrayView<bool> RelevantRoleIndexes) const
{
	using namespace UE::PoseSearch;

	check(ActorRootBoneTransforms.Num() == GetNumRoles());
	check(FullAlignedActorRootBoneTransforms.Num() == GetNumRoles());
	check(RelevantRoleIndexes.IsEmpty() || RelevantRoleIndexes.Num() == GetNumRoles());
	
	const int32 ItemsNum = Items.Num();

	int32 RelevantItemsNum;
	if (RelevantRoleIndexes.IsEmpty())
	{
		RelevantItemsNum = ItemsNum;
	}
	else
	{
		RelevantItemsNum = 0;
		for (bool bIsRelevant : RelevantRoleIndexes)
		{
			if (bIsRelevant)
			{
				++RelevantItemsNum;
			}
		}
	}

	if (RelevantItemsNum < 2)
	{
		for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
		{
			FullAlignedActorRootBoneTransforms[ItemIndex] = ActorRootBoneTransforms[ItemIndex];
		}
	}
	else
	{
		// we have at least one relevant item!

		TArray<FTransform, TInlineAllocator<PreallocatedRolesNum>> AssetRootBoneTransforms;
		AssetRootBoneTransforms.SetNum(ItemsNum);

		// ItemIndex is the RoleIndex and Role = Item.Role
		for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
		{
			const FPoseSearchInteractionAssetItem& Item = Items[ItemIndex];

			// sampling the AnimationAsset to extract the current time transform and the initial (time of 0) transform
			const FAnimationAssetSampler Sampler(Item.Animation, Item.Origin);
			AssetRootBoneTransforms[ItemIndex] = Sampler.ExtractRootTransform(Time);

			if (MirrorDataTables.IsValidIndex(ItemIndex) && MirrorDataTables[ItemIndex])
			{
				const FMirrorDataCache MirrorDataCache(MirrorDataTables[ItemIndex]);
				AssetRootBoneTransforms[ItemIndex] = MirrorDataCache.MirrorTransform(AssetRootBoneTransforms[ItemIndex]);
			}

#if ENABLE_ANIM_DEBUG && WITH_EDITOR
			if (Items[ItemIndex].Animation)
			{
				// array containing the bone index of the root bone (0)
				TArray<uint16, TInlineAllocator<1>> BoneIndices;
				BoneIndices.SetNumZeroed(1);

				// extracting the pose, containing only the root bone from the Sampler 
				FMemMark Mark(FMemStack::Get());
				FCompactPose Pose;
				FBoneContainer BoneContainer;
				BoneContainer.InitializeTo(BoneIndices, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *Items[ItemIndex].Animation->GetSkeleton());
				Pose.SetBoneContainer(&BoneContainer);
				Sampler.ExtractPose(Time, Pose);

				// making sure the animation root bone transform is Identity, so we can confuse the root with the root BONE transform and preserve performances!
				const FTransform& RootBoneTransform = Pose.GetBones()[0];
				if (!RootBoneTransform.Equals(FTransform::Identity))
				{
					const FVector Pos = RootBoneTransform.GetLocation();
					const FRotator Rot(RootBoneTransform.GetRotation());
					UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchInteractionAsset::CalculateWarpTransforms unsupported non identity root bone in %s at time %f Pos(%f, %f, %f), Rot(%f, %f, %f)"), *Items[ItemIndex].Animation->GetName(), Time, Pos.X, Pos.Y, Pos.Z, Rot.Pitch, Rot.Yaw, Rot.Roll);
				}
			}
#endif // ENABLE_ANIM_DEBUG && WITH_EDITOR
		}

		TArray<int32, TInlineAllocator<PreallocatedRolesNum>> SortedByWarpingWeightRotationItemIndex;
		TArray<float, TInlineAllocator<PreallocatedRolesNum>> NormalizedWarpingWeightRotation;
		TArray<float, TInlineAllocator<PreallocatedRolesNum>> NormalizedWarpingWeightTranslation;
		SortedByWarpingWeightRotationItemIndex.SetNum(ItemsNum);
		NormalizedWarpingWeightRotation.SetNum(ItemsNum);
		NormalizedWarpingWeightTranslation.SetNum(ItemsNum);

		float WarpingWeightTranslationSum = 0.f;
		float WarpingWeightRotationSum = 0.f;

		if (RelevantRoleIndexes.IsEmpty())
		{
			for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
			{
				SortedByWarpingWeightRotationItemIndex[ItemIndex] = ItemIndex;
				WarpingWeightTranslationSum += Items[ItemIndex].WarpingWeightTranslation;
				WarpingWeightRotationSum += Items[ItemIndex].WarpingWeightRotation;
			}
		}
		else
		{
			for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
			{
				SortedByWarpingWeightRotationItemIndex[ItemIndex] = ItemIndex;
				if (RelevantRoleIndexes[ItemIndex])
				{
					WarpingWeightTranslationSum += Items[ItemIndex].WarpingWeightTranslation;
					WarpingWeightRotationSum += Items[ItemIndex].WarpingWeightRotation;
				}
			}
		}

		const float NormalizedHomogeneousWeight = 1.f / RelevantItemsNum;
		if (RelevantRoleIndexes.IsEmpty())
		{
			if (WarpingWeightTranslationSum > UE_KINDA_SMALL_NUMBER)
			{
				for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
				{
					NormalizedWarpingWeightTranslation[ItemIndex] = Items[ItemIndex].WarpingWeightTranslation / WarpingWeightTranslationSum;
				}
			}
			else
			{
				for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
				{
					NormalizedWarpingWeightTranslation[ItemIndex] = NormalizedHomogeneousWeight;
				}
			}

			if (WarpingWeightRotationSum > UE_KINDA_SMALL_NUMBER)
			{
				SortedByWarpingWeightRotationItemIndex.Sort([this](const int32 A, const int32 B)
					{
						return Items[A].WarpingWeightRotation < Items[B].WarpingWeightRotation;
					});

				for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
				{
					NormalizedWarpingWeightRotation[ItemIndex] = Items[ItemIndex].WarpingWeightRotation / WarpingWeightRotationSum;
				}
			}
			else
			{
				for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
				{
					NormalizedWarpingWeightRotation[ItemIndex] = NormalizedHomogeneousWeight;
				}
			}
		}
		else
		{
			if (WarpingWeightTranslationSum > UE_KINDA_SMALL_NUMBER)
			{
				for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
				{
					if (RelevantRoleIndexes[ItemIndex])
					{
						NormalizedWarpingWeightTranslation[ItemIndex] = Items[ItemIndex].WarpingWeightTranslation / WarpingWeightTranslationSum;
					}
					else
					{
						NormalizedWarpingWeightTranslation[ItemIndex] = 0.f;
					}
				}
			}
			else
			{
				for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
				{
					if (RelevantRoleIndexes[ItemIndex])
					{
						NormalizedWarpingWeightTranslation[ItemIndex] = NormalizedHomogeneousWeight;
					}
					else
					{
						NormalizedWarpingWeightTranslation[ItemIndex] = 0.f;
					}
				}
			}

			if (WarpingWeightRotationSum > UE_KINDA_SMALL_NUMBER)
			{
				SortedByWarpingWeightRotationItemIndex.Sort([this](const int32 A, const int32 B)
					{
						return Items[A].WarpingWeightRotation < Items[B].WarpingWeightRotation;
					});

				for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
				{
					if (RelevantRoleIndexes[ItemIndex])
					{
						NormalizedWarpingWeightRotation[ItemIndex] = Items[ItemIndex].WarpingWeightRotation / WarpingWeightRotationSum;
					}
					else
					{
						NormalizedWarpingWeightRotation[ItemIndex] = 0.f;
					}
				}
			}
			else
			{
				for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
				{
					if (RelevantRoleIndexes[ItemIndex])
					{
						NormalizedWarpingWeightRotation[ItemIndex] = NormalizedHomogeneousWeight;
					}
					else
					{
						NormalizedWarpingWeightRotation[ItemIndex] = 0.f;
					}
				}
			}
		}

		const FQuat AssetReferenceOrientation = FindReferenceOrientation(AssetRootBoneTransforms, SortedByWarpingWeightRotationItemIndex, NormalizedWarpingWeightRotation);
		const FQuat ActorsReferenceOrientation = FindReferenceOrientation(ActorRootBoneTransforms, SortedByWarpingWeightRotationItemIndex, NormalizedWarpingWeightRotation);

		FQuat WeightedActorsReferenceOrientation = ActorsReferenceOrientation;
		if (WarpingWeightRotationSum > UE_KINDA_SMALL_NUMBER)
		{
			// ItemIndex are in order of WarpingWeightRotation. the last one is the one with the highest most WarpingWeightRotation, the most "important"
			for (int32 ItemIndex : SortedByWarpingWeightRotationItemIndex)
			{
				const FPoseSearchInteractionAssetItem& Item = Items[ItemIndex];
				if (NormalizedWarpingWeightRotation[ItemIndex] > NormalizedHomogeneousWeight)
				{
					// NormalizedHomogeneousWeight is one only if ItemsNum is one, 
					// BUT NormalizedWarpingWeightRotation[ItemIndex] > NormalizedHomogeneousWeight should always be false
					check(!FMath::IsNearlyEqual(NormalizedHomogeneousWeight, 1.f));

					// how much this item wants to reorient the ReferenceOrientation from the homogeneous "fair" value
					const float SlerpParam = (NormalizedWarpingWeightRotation[ItemIndex] - NormalizedHomogeneousWeight) / (1.f - NormalizedHomogeneousWeight);

					// calculating the reference orientation relative to the character
					// AssetReferenceOrientation in actor world orientation
					const FQuat ActorAssetReferenceOrientation = ActorRootBoneTransforms[ItemIndex].GetRotation() * (AssetRootBoneTransforms[ItemIndex].GetRotation().Inverse() * AssetReferenceOrientation);

					WeightedActorsReferenceOrientation = FQuat::Slerp(WeightedActorsReferenceOrientation, ActorAssetReferenceOrientation, SlerpParam);
				}
			}
		}

		const FVector AssetReferencePosition = FindReferencePosition(AssetRootBoneTransforms, NormalizedWarpingWeightTranslation);
		const FVector ActorsReferencePosition = FindReferencePosition(ActorRootBoneTransforms, NormalizedWarpingWeightTranslation);

		// aligning all the actors to ActorsReferencePosition, WeightedActorsReferenceOrientation
		const FTransform AssetReferenceTransform(AssetReferenceOrientation, AssetReferencePosition);
		const FTransform ActorsReferenceTransform(WeightedActorsReferenceOrientation, ActorsReferencePosition);
		const FTransform AssetReferenceInverseTransform = AssetReferenceTransform.Inverse();

		for (int32 ItemIndex = 0; ItemIndex < ItemsNum; ++ItemIndex)
		{
			FullAlignedActorRootBoneTransforms[ItemIndex] = (AssetRootBoneTransforms[ItemIndex] * AssetReferenceInverseTransform) * ActorsReferenceTransform;
		}
	}
}