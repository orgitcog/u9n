// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/ApplyAdditiveKeyframe.h"

#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "TransformArrayOperations.h"

FAnimNextApplyAdditiveKeyframeTask FAnimNextApplyAdditiveKeyframeTask::Make(float BlendWeight)
{
	FAnimNextApplyAdditiveKeyframeTask Task;
	Task.BlendWeight = BlendWeight;

	return Task;
}

FAnimNextApplyAdditiveKeyframeTask FAnimNextApplyAdditiveKeyframeTask::Make(const FName& AlphaSourceCurveName, const int8 AlphaCurveInputIndex, TFunction<float(float)> InputScaleBiasClampFn)
{
	FAnimNextApplyAdditiveKeyframeTask Task;
	Task.AlphaSourceCurveName = AlphaSourceCurveName;
	Task.AlphaCurveInputIndex = AlphaCurveInputIndex;
	Task.InputScaleBiasClampFn = MoveTemp(InputScaleBiasClampFn);
	return Task;
}

void FAnimNextApplyAdditiveKeyframeTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	// Pop our top two poses, we'll re-use the top keyframe for our result

	TUniquePtr<FKeyframeState> AdditiveKeyframe;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, AdditiveKeyframe))
	{
		// We have no inputs, nothing to do
		return;
	}

	TUniquePtr<FKeyframeState> BaseKeyframe;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, BaseKeyframe))
	{
		// We have a single input, discard it since it must be the additive pose, either way something went wrong
		// Push the reference pose since we'll expect a non-additive pose
		VM.PushValue(KEYFRAME_STACK_NAME, MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(false)));
		return;
	}

	if (!AdditiveKeyframe->Pose.IsAdditive())
	{
		// Additive must be addtive type, push reference pose if not the case
		VM.PushValue(KEYFRAME_STACK_NAME, MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(false)));
		return;
	}

	const float AdditiveWeight = GetInterpolationAlpha(BaseKeyframe.Get(), AdditiveKeyframe.Get());

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		check(BaseKeyframe->Pose.GetNumBones() == AdditiveKeyframe->Pose.GetNumBones());

		const FTransformArrayView BaseTransformsView = BaseKeyframe->Pose.LocalTransforms.GetView();

		if (AdditiveKeyframe->Pose.IsMeshSpaceAdditive())
		{
			BlendWithIdentityAndAccumulateMesh(
				BaseTransformsView, AdditiveKeyframe->Pose.LocalTransforms.GetConstView(), 
				AdditiveKeyframe->Pose.GetLODBoneIndexToParentLODBoneIndexMap(), AdditiveWeight);
		}
		else
		{
			BlendWithIdentityAndAccumulate(BaseTransformsView, AdditiveKeyframe->Pose.LocalTransforms.GetConstView(), AdditiveWeight);
		}

		NormalizeRotations(BaseTransformsView);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		BaseKeyframe->Curves.Accumulate(AdditiveKeyframe->Curves, AdditiveWeight);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		UE::Anim::Attributes::AccumulateAttributes(AdditiveKeyframe->Attributes, BaseKeyframe->Attributes, AdditiveWeight, AAT_None);
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(BaseKeyframe));
}

float FAnimNextApplyAdditiveKeyframeTask::GetInterpolationAlpha(const UE::AnimNext::FKeyframeState* KeyframeA, const UE::AnimNext::FKeyframeState* KeyframeB) const
{
	float Alpha = BlendWeight;

	if (AlphaSourceCurveName != NAME_None && AlphaCurveInputIndex != INDEX_NONE)
	{
		if (ensure(KeyframeA != nullptr && KeyframeB != nullptr))
		{
			const FBlendedCurve& Curves = AlphaCurveInputIndex == 0 ? KeyframeA->Curves : KeyframeB->Curves;
			Alpha = Curves.Get(AlphaSourceCurveName); // if the curve does not exist, it returns 0.f

			if (InputScaleBiasClampFn.IsSet())
			{
				Alpha = InputScaleBiasClampFn(Alpha);
			}
		}
	}

	return FMath::Clamp(Alpha, 0.0f, 1.0f);
}
