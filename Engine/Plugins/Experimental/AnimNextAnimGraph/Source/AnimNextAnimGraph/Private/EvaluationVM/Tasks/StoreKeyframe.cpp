// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/StoreKeyframe.h"

#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "TransformArrayOperations.h"

FAnimNextSwapTransformsTask FAnimNextSwapTransformsTask::Make(UE::AnimNext::FTransformArraySoAHeap* A, UE::AnimNext::FTransformArraySoAHeap* B)
{
	FAnimNextSwapTransformsTask Task;
	Task.A = A;
	Task.B = B;
	return Task;
}

void FAnimNextSwapTransformsTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	Swap(*A, *B);
}

FAnimNextStoreKeyframeTransformsTask FAnimNextStoreKeyframeTransformsTask::Make(UE::AnimNext::FTransformArraySoAHeap* Dest)
{
	FAnimNextStoreKeyframeTransformsTask Task;
	Task.Dest = Dest;
	return Task;
}

void FAnimNextStoreKeyframeTransformsTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		if (const TUniquePtr<FKeyframeState>* Keyframe = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0))
		{
			Dest->SetNumUninitialized((*Keyframe)->Pose.LocalTransforms.Num());
			CopyTransforms(Dest->GetView(), (*Keyframe)->Pose.LocalTransforms.GetConstView());
		}
	}
}

// --- FAnimNextDuplicateTopKeyframeTask  ---

FAnimNextDuplicateTopKeyframeTask FAnimNextDuplicateTopKeyframeTask::Make()
{
	FAnimNextDuplicateTopKeyframeTask Task;
	return Task;
}

// Task entry point
void FAnimNextDuplicateTopKeyframeTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	TUniquePtr<FKeyframeState> KeyframeOut;

	// Try to get a Keyframe from the stack without consuming it
	// If there is a Keyframe, create a copy
	if (const TUniquePtr<FKeyframeState>* KeyframeIn = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0))
	{
		KeyframeOut = MakeUnique<FKeyframeState>(VM.MakeUninitializedKeyframe(false));
		KeyframeOut->Pose.CopyFrom((*KeyframeIn)->Pose);
		KeyframeOut->Curves.CopyFrom((*KeyframeIn)->Curves);
		KeyframeOut->Attributes.CopyFrom((*KeyframeIn)->Attributes);
	}
	else
	{
		// If there is no Keyframe, generate a reference key frame
		KeyframeOut = MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(false));
	}

	// Push the copy or reference keyframe to the stack
	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeOut));
}
