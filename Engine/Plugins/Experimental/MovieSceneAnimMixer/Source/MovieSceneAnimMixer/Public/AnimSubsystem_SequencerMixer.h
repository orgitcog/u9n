// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EvaluationVM/EvaluationTask.h"
#include "Animation/AnimSubsystemInstance.h"
#include "Animation/AnimInstance.h"

#include "AnimSubsystem_SequencerMixer.generated.h"

/** Allows sequencer's anim mixer system to communicate with mixer nodes in an anim blueprint*/
USTRUCT()
struct FAnimSubsystem_SequencerMixer : public FAnimSubsystemInstance
{
	GENERATED_BODY()

	// FAnimSubsystemInstance interface
	MOVIESCENEANIMMIXER_API virtual void Initialize_WorkerThread() override;

	void RegisterEvalTask(FName TargetName, TSharedPtr<FAnimNextEvaluationTask> EvalTask);
	
	const TSharedPtr<FAnimNextEvaluationTask>* GetEvalTask(FName TargetName) const;

	void ResetEvalTasks();

private:
	TMap<FName, TSharedPtr<FAnimNextEvaluationTask>> TargetToTaskMap;
	
};
