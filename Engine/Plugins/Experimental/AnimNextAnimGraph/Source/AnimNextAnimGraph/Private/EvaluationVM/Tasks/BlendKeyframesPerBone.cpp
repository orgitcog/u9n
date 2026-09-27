// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/BlendKeyframesPerBone.h"

#include "AnimationRuntime.h"
#include "Animation/AnimCurveUtils.h"
#include "Animation/AttributeTypes.h"
#include "Animation/BlendProfile.h"
#include "Animation/SkeletonRemapping.h"
#include "Animation/SkeletonRemappingRegistry.h"
#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "TransformArrayOperations.h"

FAnimNextBlendOverwriteKeyframePerBoneWithScaleTask FAnimNextBlendOverwriteKeyframePerBoneWithScaleTask::Make(const IBlendProfileInterface* BlendProfile, const FBlendSampleData& BlendData, float ScaleFactor)
{
	FAnimNextBlendOverwriteKeyframePerBoneWithScaleTask Task;
	Task.BlendProfile = BlendProfile;
	Task.SourceSkeleton = BlendProfile->GetSkeleton();
	Task.BlendData = &BlendData;
	Task.ScaleFactor = ScaleFactor;

	return Task;
}

void FAnimNextBlendOverwriteKeyframePerBoneWithScaleTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	if (BlendProfile == nullptr)
	{
		// If we don't have a blend profile, we blend the whole pose
		Super::Execute(VM);
		return;
	}

	TUniquePtr<FKeyframeState> Keyframe;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, Keyframe))
	{
		// We have no inputs, nothing to do
		return;
	}

	const USkeleton* TargetSkeleton = Keyframe->Pose.GetSkeletonAsset();
	TSharedPtr<IInterpolationIndexProvider::FPerBoneInterpolationData> Data = BlendProfile->GetPerBoneInterpolationData(TargetSkeleton);

	const FSkeletonRemapping& SkeletonRemapping = UE::Anim::FSkeletonRemappingRegistry::Get().GetRemapping(BlendProfile->GetSkeleton(), TargetSkeleton);

	const TArrayView<const FBoneIndexType> LODBoneIndexToSkeletonBoneIndexMap = Keyframe->Pose.GetLODBoneIndexToSkeletonBoneIndexMap();
	const int32 NumLODBones = LODBoneIndexToSkeletonBoneIndexMap.Num();

	TArray<int32> LODBoneIndexToWeightIndexMap;
	LODBoneIndexToWeightIndexMap.AddUninitialized(NumLODBones);

	if (SkeletonRemapping.IsValid())
	{
		for (int32 LODBoneIndex = 0; LODBoneIndex < NumLODBones; ++LODBoneIndex)
		{
			const FSkeletonPoseBoneIndex TargetSkeletonBoneIndex(LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]);
			const FSkeletonPoseBoneIndex SourceSkeletonBoneIndex(SkeletonRemapping.GetSourceSkeletonBoneIndex(TargetSkeletonBoneIndex.GetInt()));
			LODBoneIndexToWeightIndexMap[LODBoneIndex] = SourceSkeletonBoneIndex.IsValid() ? BlendProfile->GetPerBoneInterpolationIndex(SourceSkeletonBoneIndex, TargetSkeleton, Data.Get()) : INDEX_NONE;
		}
	}
	else
	{
		for (int32 LODBoneIndex = 0; LODBoneIndex < NumLODBones; ++LODBoneIndex)
		{
			const FSkeletonPoseBoneIndex SkeletonBoneIndex(LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]);
			LODBoneIndexToWeightIndexMap[LODBoneIndex] = BlendProfile->GetPerBoneInterpolationIndex(SkeletonBoneIndex, TargetSkeleton, Data.Get());
		}
	}

	const float BlendWeight = BlendData->GetClampedWeight();

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		BlendOverwritePerBoneWithScale(
			Keyframe->Pose.LocalTransforms.GetView(), Keyframe->Pose.LocalTransforms.GetConstView(),
			LODBoneIndexToWeightIndexMap, BlendData->PerBoneBlendData, BlendWeight);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		// Curves cannot override in place
		FBlendedCurve Result;
		Result.Override(Keyframe->Curves, BlendWeight);

		Keyframe->Curves = MoveTemp(Result);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		UE::Anim::FStackAttributeContainer OutputAttributes;
		UE::Anim::Attributes::BlendAttributesPerBone({ Keyframe->Attributes }, { LODBoneIndexToWeightIndexMap }, { *BlendData }, { 0 }, { OutputAttributes });
		Keyframe->Attributes.MoveFrom(OutputAttributes);
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(Keyframe));
}

FAnimNextBlendAddKeyframePerBoneWithScaleTask FAnimNextBlendAddKeyframePerBoneWithScaleTask::Make(const IBlendProfileInterface* BlendProfile, const FBlendSampleData& BlendDataA, const FBlendSampleData& BlendDataB, float ScaleFactor)
{
	FAnimNextBlendAddKeyframePerBoneWithScaleTask Task;
	Task.BlendProfile = BlendProfile;
	Task.BlendDataA = &BlendDataA;
	Task.BlendDataB = &BlendDataB;
	Task.ScaleFactor = ScaleFactor;

	return Task;
}

void FAnimNextBlendAddKeyframePerBoneWithScaleTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	if (BlendProfile == nullptr)
	{
		// If we don't have a blend profile, we blend the whole pose
		Super::Execute(VM);
		return;
	}

	// Pop our top two poses, we'll re-use the top keyframe for our result

	TUniquePtr<FKeyframeState> KeyframeB;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, KeyframeB))
	{
		// We have no inputs, nothing to do
		return;
	}

	TUniquePtr<FKeyframeState> KeyframeA;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, KeyframeA))
	{
		// We have a single input, leave it on top of the stack
		VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeB));
		return;
	}

	const USkeleton* TargetSkeleton = KeyframeB->Pose.GetSkeletonAsset();
	TSharedPtr<IInterpolationIndexProvider::FPerBoneInterpolationData> Data = BlendProfile->GetPerBoneInterpolationData(TargetSkeleton);

	const FSkeletonRemapping& SkeletonRemapping = UE::Anim::FSkeletonRemappingRegistry::Get().GetRemapping(BlendProfile->GetSkeleton(), TargetSkeleton);

	const TArrayView<const FBoneIndexType> LODBoneIndexToSkeletonBoneIndexMap = KeyframeB->Pose.GetLODBoneIndexToSkeletonBoneIndexMap();
	const int32 NumLODBones = LODBoneIndexToSkeletonBoneIndexMap.Num();

	TArray<int32> LODBoneIndexToWeightIndexMap;
	LODBoneIndexToWeightIndexMap.AddUninitialized(NumLODBones);

	if (SkeletonRemapping.IsValid())
	{
		for (int32 LODBoneIndex = 0; LODBoneIndex < NumLODBones; ++LODBoneIndex)
		{
			const FSkeletonPoseBoneIndex TargetSkeletonBoneIndex(LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]);
			const FSkeletonPoseBoneIndex SourceSkeletonBoneIndex(SkeletonRemapping.GetSourceSkeletonBoneIndex(TargetSkeletonBoneIndex.GetInt()));
			LODBoneIndexToWeightIndexMap[LODBoneIndex] = SourceSkeletonBoneIndex.IsValid() ? BlendProfile->GetPerBoneInterpolationIndex(SourceSkeletonBoneIndex, TargetSkeleton, Data.Get()) : INDEX_NONE;
		}
	}
	else
	{
		for (int32 LODBoneIndex = 0; LODBoneIndex < NumLODBones; ++LODBoneIndex)
		{
			const FSkeletonPoseBoneIndex SkeletonBoneIndex(LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]);
			LODBoneIndexToWeightIndexMap[LODBoneIndex] = BlendProfile->GetPerBoneInterpolationIndex(SkeletonBoneIndex, TargetSkeleton, Data.Get());
		}
	}

	const float BlendWeightA = BlendDataA->GetClampedWeight();

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		check(KeyframeA->Pose.GetNumBones() == KeyframeB->Pose.GetNumBones());

		BlendAddPerBoneWithScale(
			KeyframeB->Pose.LocalTransforms.GetView(), KeyframeA->Pose.LocalTransforms.GetConstView(),
			LODBoneIndexToWeightIndexMap, BlendDataA->PerBoneBlendData, BlendWeightA);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		KeyframeB->Curves.Accumulate(KeyframeA->Curves, BlendWeightA);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		// TODO: Might need to be revisited, might not work as intended
		UE::Anim::FStackAttributeContainer OutputAttributes;
		UE::Anim::Attributes::BlendAttributesPerBone(
			{ KeyframeA->Attributes, KeyframeB->Attributes },
			{ LODBoneIndexToWeightIndexMap },
			{ *BlendDataA, *BlendDataB },
			{ 0, 1 },
			{ OutputAttributes });

		KeyframeA->Attributes.MoveFrom(OutputAttributes);
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeB));
}

FAnimNextBlendKeyframePerBoneWithScaleTask FAnimNextBlendKeyframePerBoneWithScaleTask::Make(TSharedPtr<IInterpolationIndexProvider> BlendProfile, const USkeleton* Skeleton, const TArray<float>& BoneMaskWeights, const UE::Anim::TNamedValueArray<FDefaultAllocator, UE::Anim::FCurveElement>& CurveMaskWeights, const TArray<FMaskedAttributeWeight>& AttributeMaskWeights, float ScaleFactor)
{
	FAnimNextBlendKeyframePerBoneWithScaleTask Task;
	Task.BlendProfile = BlendProfile;
	Task.SourceSkeleton = Skeleton;
	Task.BoneMaskWeights = BoneMaskWeights;
	Task.CurveMaskWeights = CurveMaskWeights;
	Task.AttributeMaskWeights = AttributeMaskWeights;
	Task.ScaleFactor = ScaleFactor;

	return Task;
}

void FAnimNextBlendKeyframePerBoneWithScaleTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	if (BlendProfile == nullptr)
	{
		// If we don't have a blend profile, we blend the whole pose
		Super::Execute(VM);
		return;
	}

	// Pop our top two poses, we'll re-use the top keyframe for our result

	TUniquePtr<FKeyframeState> KeyframeB;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, KeyframeB))
	{
		// We have no inputs, nothing to do
		return;
	}

	TUniquePtr<FKeyframeState> KeyframeA;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, KeyframeA))
	{
		// We have a single input, leave it on top of the stack
		VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeB));
		return;
	}

	const USkeleton* TargetSkeleton = KeyframeB->Pose.GetSkeletonAsset();
	TSharedPtr<IInterpolationIndexProvider::FPerBoneInterpolationData> Data = BlendProfile->GetPerBoneInterpolationData(TargetSkeleton);

	const FSkeletonRemapping& SkeletonRemapping = UE::Anim::FSkeletonRemappingRegistry::Get().GetRemapping(SourceSkeleton, TargetSkeleton);

	const TArrayView<const FBoneIndexType> LODBoneIndexToSkeletonBoneIndexMap = KeyframeB->Pose.GetLODBoneIndexToSkeletonBoneIndexMap();
	const int32 NumLODBones = LODBoneIndexToSkeletonBoneIndexMap.Num();

	TArray<int32, TMemStackAllocator<>> LODBoneIndexToWeightIndexMap;
	LODBoneIndexToWeightIndexMap.AddUninitialized(NumLODBones);

	if (SkeletonRemapping.IsValid())
	{
		for (int32 LODBoneIndex = 0; LODBoneIndex < NumLODBones; ++LODBoneIndex)
		{
			const FSkeletonPoseBoneIndex TargetSkeletonBoneIndex(LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]);
			const FSkeletonPoseBoneIndex SourceSkeletonBoneIndex(SkeletonRemapping.GetSourceSkeletonBoneIndex(TargetSkeletonBoneIndex.GetInt()));
			LODBoneIndexToWeightIndexMap[LODBoneIndex] = SourceSkeletonBoneIndex.IsValid() ? BlendProfile->GetPerBoneInterpolationIndex(SourceSkeletonBoneIndex, TargetSkeleton, Data.Get()) : INDEX_NONE;
		}
	}
	else
	{
		for (int32 LODBoneIndex = 0; LODBoneIndex < NumLODBones; ++LODBoneIndex)
		{
			const FSkeletonPoseBoneIndex SkeletonBoneIndex(LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]);
			LODBoneIndexToWeightIndexMap[LODBoneIndex] = BlendProfile->GetPerBoneInterpolationIndex(SkeletonBoneIndex, TargetSkeleton, Data.Get());
		}
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		check(KeyframeA->Pose.GetNumBones() == KeyframeB->Pose.GetNumBones());

		BlendOverwritePerBoneWithScale(
			KeyframeA->Pose.LocalTransforms.GetView(), KeyframeA->Pose.LocalTransforms.GetConstView(),
			LODBoneIndexToWeightIndexMap, BoneMaskWeights, 1.0f, true);

		BlendAddPerBoneWithScale(
			KeyframeA->Pose.LocalTransforms.GetView(), KeyframeB->Pose.LocalTransforms.GetConstView(),
			LODBoneIndexToWeightIndexMap, BoneMaskWeights, ScaleFactor);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		FBlendedCurve& OutCurve = KeyframeA->Curves;
		FBlendedCurve& TargetCurve = KeyframeB->Curves;

		if (FAnimWeight::IsRelevant(ScaleFactor))
		{
			FBlendedCurve FilteredCurves;

			// Multiply per-curve blend weights by matching blend pose curves
			UE::Anim::FNamedValueArrayUtils::Intersection(
				TargetCurve,
				CurveMaskWeights,
				[this, &FilteredCurves](const UE::Anim::FCurveElement& InBlendElement, const UE::Anim::FCurveElement& InMaskElement) mutable
				{
					FilteredCurves.Add(InBlendElement.Name, InBlendElement.Value * InMaskElement.Value);
				});

			// Override blend curve values with premultipled curves
			TargetCurve.Combine(FilteredCurves);

			// Remove curves that have been filtered by the mask, curves with no mask value defined remain, even with 0.0 value
			UE::Anim::FNamedValueArrayUtils::RemoveByPredicate(
				TargetCurve,
				CurveMaskWeights,
				[](const UE::Anim::FCurveElement& InBaseElement, const UE::Anim::FCurveElement& InMaskElement)
				{
					const bool bKeep = InMaskElement.Value == 0.0;
					return bKeep;
				}
			);

			// Combine base and filtered pre-multiplied blend curves
			UE::Anim::FNamedValueArrayUtils::Union(
				OutCurve,
				TargetCurve,
				[this](UE::Anim::FCurveElement& InOutBaseElement, const UE::Anim::FCurveElement& InBlendElement, UE::Anim::ENamedValueUnionFlags InFlags)
				{
					if (InFlags == UE::Anim::ENamedValueUnionFlags::BothArgsValid || InFlags == UE::Anim::ENamedValueUnionFlags::ValidArg1)
					{
						InOutBaseElement.Value = FMath::Lerp(InOutBaseElement.Value, InBlendElement.Value, ScaleFactor);
						InOutBaseElement.Flags |= InBlendElement.Flags;
					}
				});
		}
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		using namespace UE::Anim;

		FStackAttributeContainer& BaseAttributes = KeyframeA->Attributes;
		FStackAttributeContainer& BlendAttributes = KeyframeB->Attributes;

		FStackAttributeContainer OutputAttributes;

		// Attributes are to be masked out according to the mask weights in AttributeMaskWeights, if an attribute has no mask weight set then it
		// inherits the weight of whatever bone it is attached to. Below are possible configurations that we need to account for:

		// Root 0.0						Root set to 0.0 therefore a RootMotionDelta attribute will also be masked out without having to set an explicit entry in AttributeMaskWeights

		// Root 0.0						RootMotionDelta is set to 1.0 in AttributeMaskWeights despite the parent bone being masked out.
		//  \ RootMotionDelta 1.0

		// Root 1.0						RootMotionDelta is being masked out in AttributeMaskWeights despite the parent bone being kept.
		//  \ RootMotionDelta 0.0


		// Below is a table of the possible permutations of base/blend attributes being present/absent along with the possible mask values
		// k denotes some value in the range (0, 1) exclusive.
		// - denotes an absent attribute

		// Base | Blend | Weight | Output
		// ------------------------------
		// a    | b     | 1.0    | b
		// a    | b     | k      | lerp(a, b, k)
		// a    | b     | 0.0    | a
		// - - - - - - - - - - - - - - -
		// a    | -     | 1.0    | a
		// a    | -     | k      | a
		// a    | -     | 0.0    | a
		// - - - - - - - - - - - - - - -
		// -    | b     | 1.0    | b
		// -    | b     | k      | lerp(default, b, k)
		// -    | b     | 0.0    | -


		// 1. Blend attributes according to the bone blend weights,
		// i.e. an attribute's weight is determined by the weight of its attached bone.
		UE::Anim::Attributes::BlendAttributesPerBone(BaseAttributes, BlendAttributes, BoneMaskWeights, OutputAttributes);


		// 2. For each attribute that has a custom weight, i.e. one's that shouldn't be weighted by its attached bone, go and correct the blended value.
		for (const FMaskedAttributeWeight& MaskedAttribute : AttributeMaskWeights)
		{
			const TArray<TWeakObjectPtr<UScriptStruct>>& UniqueTypes = OutputAttributes.GetUniqueTypes();

			for (int32 UniqueTypeIndex = 0; UniqueTypeIndex < UniqueTypes.Num(); ++UniqueTypeIndex)
			{
				const TArray<FAttributeId>& AttributeKeys = OutputAttributes.GetKeys(UniqueTypeIndex);
				const TArray<TWrappedAttribute<FAnimStackAllocator>>& AttributeValues = OutputAttributes.GetValues(UniqueTypeIndex);
				
				const TWeakObjectPtr<UScriptStruct> AttributeType = UniqueTypes[UniqueTypeIndex];
				const IAttributeBlendOperator* Operator = UE::Anim::AttributeTypes::GetTypeOperator(AttributeType);

				TWrappedAttribute<FAnimStackAllocator> DefaultData(AttributeType.Get());
				AttributeType->InitializeStruct(DefaultData.GetPtr<void>());

				uint8* OutputData = OutputAttributes.Find(AttributeType.Get(), MaskedAttribute.Attribute);
				if (OutputData)
				{
					uint8* BaseData = BaseAttributes.Find(AttributeType.Get(), MaskedAttribute.Attribute);
					uint8* BlendData = BlendAttributes.Find(AttributeType.Get(), MaskedAttribute.Attribute);

					if (BaseData && BlendData)
					{
						// Base | Blend | Weight | Output
						// ------------------------------
						// a    | b     | 1.0    | b
						// a    | b     | k      | lerp(a, b, k)
						// a    | b     | 0.0    | a

						Operator->Interpolate(BaseData, BlendData, MaskedAttribute.Weight, OutputData);
					}
					else if (BaseData && !BlendData)
					{
						// Base | Blend | Weight | Output
						// ------------------------------
						// a    | -     | 1.0    | a
						// a    | -     | k      | a
						// a    | -     | 0.0    | a

						*OutputData = *BaseData;
					}
					else if (!BaseData && BlendData)
					{
						// Base | Blend | Weight | Output
						// ------------------------------
						// -    | b     | 1.0    | b
						// -    | b     | k      | lerp(default, b, k)
						// -    | b     | 0.0    | -

						if (MaskedAttribute.Weight != 0.0f)
						{
							Operator->Interpolate(DefaultData.GetPtr<void>(), BlendData, MaskedAttribute.Weight, OutputData);
						}
						else
						{
							OutputAttributes.Remove(AttributeType.Get(), MaskedAttribute.Attribute);
						}
					}
				}				
			}
		}

		BaseAttributes.MoveFrom(OutputAttributes);
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeA));
}