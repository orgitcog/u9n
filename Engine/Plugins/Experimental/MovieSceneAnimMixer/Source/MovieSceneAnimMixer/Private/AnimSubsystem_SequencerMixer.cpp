// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimSubsystem_SequencerMixer.h"

void FAnimSubsystem_SequencerMixer::Initialize_WorkerThread()
{
	TargetToTaskMap.Empty();
}

void FAnimSubsystem_SequencerMixer::RegisterEvalTask(const FName TargetName,
                                                     TSharedPtr<FAnimNextEvaluationTask> EvalTask)
{
	TargetToTaskMap.FindOrAdd(TargetName) = EvalTask;
}

const TSharedPtr<FAnimNextEvaluationTask>* FAnimSubsystem_SequencerMixer::GetEvalTask(const FName TargetName) const
{
	return TargetToTaskMap.Find(TargetName);
}

void FAnimSubsystem_SequencerMixer::ResetEvalTasks()
{
	TargetToTaskMap.Reset();
}
