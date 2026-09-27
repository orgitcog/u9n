// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/PushPose.h"
#include "EvaluationVM/EvaluationVM.h"

DEFINE_STAT(STAT_AnimNext_Task_PushPose);

FAnimNextPushPoseTask::FAnimNextPushPoseTask(const FAnimNextGraphLODPose* InGraphPose)
	: GraphPose(InGraphPose)
{
}

FAnimNextPushPoseTask FAnimNextPushPoseTask::Make(const FAnimNextGraphLODPose* InGraphPose)
{
	return FAnimNextPushPoseTask(InGraphPose);
}

FAnimNextPushPoseTask& FAnimNextPushPoseTask::operator=(const FAnimNextPushPoseTask& Other)
{
	GraphPose = Other.GraphPose;
	return *this;
}

void FAnimNextPushPoseTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_PushPose);

	using namespace UE::AnimNext;

	FKeyframeState Keyframe = VM.MakeUninitializedKeyframe(GraphPose->LODPose.IsAdditive());

	Keyframe.Pose.CopyFrom(GraphPose->LODPose);
	Keyframe.Curves.CopyFrom(GraphPose->Curves);
	Keyframe.Attributes.CopyFrom(GraphPose->Attributes);

	VM.PushValue(KEYFRAME_STACK_NAME, MakeUnique<FKeyframeState>(MoveTemp(Keyframe)));
}