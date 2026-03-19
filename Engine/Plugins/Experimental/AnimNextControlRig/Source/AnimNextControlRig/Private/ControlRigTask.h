// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EvaluationVM/EvaluationTask.h"
#include "Animation/AnimCurveTypes.h"
#include "LODPose.h"
#include "ControlRigTrait.h"
#include "AnimNextStats.h"
// --- ---
#include "ControlRigTask.generated.h"

namespace UE::AnimNext
{
struct FKeyframeState;
}

DECLARE_CYCLE_STAT_EXTERN(TEXT("UAF Task: ControlRig"), STAT_AnimNext_Task_ControlRig, STATGROUP_AnimNext, ANIMNEXTCONTROLRIG_API);

USTRUCT()
struct ANIMNEXTCONTROLRIG_API FAnimNextControlRigTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextControlRigTask)

	static FAnimNextControlRigTask Make(const UE::AnimNext::FControlRigTrait::FSharedData* SharedData, UE::AnimNext::FControlRigTrait::FInstanceData* InstanceData);

	// Task entry point
	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

private:
	void ExecuteControlRig(UE::AnimNext::FEvaluationVM& VM, UE::AnimNext::FKeyframeState& KeyFrameState, UControlRig* ControlRig) const;
	void UpdateInput(UE::AnimNext::FEvaluationVM& VM, UE::AnimNext::FKeyframeState& KeyFrameState, UControlRig* ControlRig, UE::AnimNext::FKeyframeState& InOutput) const;
	void UpdateOutput(UE::AnimNext::FEvaluationVM& VM, UE::AnimNext::FKeyframeState& KeyFrameState, UControlRig* ControlRig, UE::AnimNext::FKeyframeState& InOutput) const;

	bool CanExecute(const UControlRig* ControlRig) const;

	void QueueControlRigDrawInstructions(UControlRig* ControlRig, FRigVMDrawInterface* DebugDrawInterface, const FTransform& ComponentTransform) const;

	const UE::AnimNext::FControlRigTrait::FSharedData* SharedData = nullptr;
	UE::AnimNext::FControlRigTrait::FInstanceData* InstanceData = nullptr;
};
