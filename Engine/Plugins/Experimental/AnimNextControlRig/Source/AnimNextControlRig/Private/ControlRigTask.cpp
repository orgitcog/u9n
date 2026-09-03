// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigTask.h"

#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "TransformArrayOperations.h"
#include "BoneContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Math/Transform.h"
#include "GenerationTools.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimNodeBase.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#if ENABLE_ANIM_DEBUG
#include "AnimNode_ControlRigBase.h"
#endif

static TAutoConsoleVariable<int32> CVarControlRigDisableExecutionAnimNext(TEXT("ControlRig.DisableExecutionInAnimNext"), 0, TEXT("if nonzero we disable the execution of Control Rigs inside Anim Next Trrait."));

DEFINE_STAT(STAT_AnimNext_Task_ControlRig);

FAnimNextControlRigTask FAnimNextControlRigTask::Make(const UE::AnimNext::FControlRigTrait::FSharedData* SharedData, UE::AnimNext::FControlRigTrait::FInstanceData* InstanceData)
{
	FAnimNextControlRigTask Task;
	Task.InstanceData = InstanceData;
	Task.SharedData = SharedData;
	return Task;
}

void FAnimNextControlRigTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_ControlRig);
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	using namespace UE::AnimNext;

	TUniquePtr<FKeyframeState> KeyframeOut;

	// Try to get a Keyframe from the stack
	if (!VM.PopValue(UE::AnimNext::KEYFRAME_STACK_NAME, KeyframeOut))
	{
		// if there is none, just create a reference frame
		KeyframeOut = MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(false));
	}

	ExecuteControlRig(VM, *KeyframeOut.Get(), InstanceData->ControlRig);

	// Push our blended result back
	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeOut));

	InstanceData->LastLOD = VM.GetCurrentLOD();
}

void FAnimNextControlRigTask::ExecuteControlRig(UE::AnimNext::FEvaluationVM& VM, UE::AnimNext::FKeyframeState& KeyFrameState, UControlRig* ControlRig) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (ControlRig)
	{
		FMemMark Mark(FMemStack::Get());

		// Before we start modifying the RigHierarchy, we need to lock the rig to avoid corrupted state
		// TODO : Is this really needed ? 
		//FScopeLock LockRig(&ControlRig->GetEvaluateMutex());

		URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
		if (Hierarchy == nullptr)
		{
			return;
		}

		const UE::AnimNext::FLODPoseStack& Pose = KeyFrameState.Pose;
		const UE::AnimNext::FReferencePose& RefPose = Pose.GetRefPose();

		// remap LOD pose attributes to mesh bone pose indices
		UE::Anim::FMeshAttributeContainer MeshAttributeContainer;
		UE::AnimNext::FGenerationTools::RemapAttributes(Pose, KeyFrameState.Attributes, MeshAttributeContainer);

		// temporarily give control rig access to the stack allocated attribute container
		// control rig may have rig units that can add/get attributes to/from this container
		UControlRig::FAnimAttributeContainerPtrScope AttributeScope(ControlRig, MeshAttributeContainer);

		const int32 CurrentLOD = VM.GetCurrentLOD();
		const bool bIsLODChange = InstanceData->LastLOD != CurrentLOD;
		const bool bRunPrepareForExecution = bIsLODChange /* || (RefPoseBonesSerialNumber != LastRefPoseBonesSerialNumber)*/; // TODO : Detect RefPose changes
		if (InstanceData->bUpdateInputOutputMapping || bRunPrepareForExecution)
		{
			if (ControlRig->IsConstructionModeEnabled() ||
				(ControlRig->IsConstructionRequired() && (InstanceData->bUpdateInputOutputMapping || bRunPrepareForExecution)))
			{
				//UpdateGetAssetUserDataDelegate(ControlRig);
				ControlRig->Execute(FRigUnit_PrepareForExecution::EventName);
			}

			// UpdateInputOutputMappingIfRequired was done in CacheBones, but there is no AnimNext equivalent
			InstanceData->ControlRigHierarchyMappings.UpdateInputOutputMappingIfRequired(ControlRig
				, ControlRig->GetHierarchy()
				, RefPose
				, CurrentLOD
				, TArray<FBoneReference>()
				, TArray<FBoneReference>()
				, InstanceData->NodeMappingContainer
				, SharedData->bTransferPoseInGlobalSpace
				, SharedData->bResetInputPoseToInitial);

			InstanceData->bUpdateInputOutputMapping = false;
		}

		if (!InstanceData->ControlRigHierarchyMappings.IsUpdateToDate(Hierarchy))
		{
			InstanceData->ControlRigHierarchyMappings.PerformUpdateToDate(ControlRig
				, Hierarchy
				, RefPose
				, CurrentLOD
				, InstanceData->NodeMappingContainer
				, SharedData->bTransferPoseInGlobalSpace
				, SharedData->bResetInputPoseToInitial);
		}

		// first update input to the system
		UpdateInput(VM, KeyFrameState, ControlRig, KeyFrameState);

		if (InstanceData->bExecute)
		{
			const TGuardValue<bool> ResetCurrentTransfromsAfterConstructionGuard = ControlRig->GetResetCurrentTransformsAfterConstructionGuard(true);

#if WITH_EDITOR
			if (Hierarchy->IsTracingChanges())
			{
				Hierarchy->StorePoseForTrace(TEXT("FAnimNode_ControlRigBase::BeforeEvaluate"));
			}
#endif

			// pick the event to run
			if (SharedData->EventQueue.IsEmpty())
			{
				if (InstanceData->bClearEventQueueRequired)
				{
					ControlRig->SetEventQueue({ FRigUnit_BeginExecution::EventName });
					InstanceData->bClearEventQueueRequired = false;
				}
			}
			else
			{
				TArray<FName> EventNames;
				Algo::Transform(SharedData->EventQueue, EventNames, [](const FControlRigEventName& InEventName)
					{
						return InEventName.EventName;
					});
				ControlRig->SetEventQueue(EventNames);
				InstanceData->bClearEventQueueRequired = true;
			}

			if (ControlRig->IsAdditive())
			{
				ControlRig->ClearPoseBeforeBackwardsSolve();
			}

			// evaluate control rig
			// UpdateGetAssetUserDataDelegate(ControlRig); // Removed in AnimNext, do we need an equivalent ?
			ControlRig->Evaluate_AnyThread();

#if ENABLE_ANIM_DEBUG
#if UE_ENABLE_DEBUG_DRAWING
			// When Control Rig is at editing time (in CR editor), draw instructions are consumed by ControlRigEditMode, so we need to skip drawing here.
			const bool bShowDebug = (CVarAnimNodeControlRigDebug.GetValueOnAnyThread() == 1 && ControlRig->ExecutionType != ERigExecutionType::Editing);
			if (bShowDebug)
			{
				if (FRigVMDrawInterface* DebugDrawInterface = InstanceData->DebugDrawInterface)
				{
					QueueControlRigDrawInstructions(ControlRig, InstanceData->DebugDrawInterface, InstanceData->ComponentTransform);
				}
			}
#endif
#endif

#if WITH_EDITOR
			if (Hierarchy->IsTracingChanges())
			{
				Hierarchy->StorePoseForTrace(TEXT("FAnimNode_ControlRigBase::AfterEvaluate"));
			}
#endif
		}

		// now update output
		UpdateOutput(VM, KeyFrameState, ControlRig, KeyFrameState);
		
		// remap mesh bone index attributes back to stack container (LOD/compact bone indices)
		UE::AnimNext::FGenerationTools::RemapAttributes(Pose, MeshAttributeContainer, KeyFrameState.Attributes);
	}
}

void FAnimNextControlRigTask::UpdateInput(UE::AnimNext::FEvaluationVM& VM, UE::AnimNext::FKeyframeState& KeyFrameState, UControlRig* ControlRig, UE::AnimNext::FKeyframeState& InOutput) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (!CanExecute(ControlRig))
	{
		return;
	}

	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	InstanceData->ControlRigHierarchyMappings.UpdateInput(ControlRig
		, InOutput
		, InstanceData->InputSettings
		, InstanceData->OutputSettings
		, InstanceData->NodeMappingContainer
		, InstanceData->bExecute
		, SharedData->bTransferInputPose
		, SharedData->bResetInputPoseToInitial
		, SharedData->bTransferPoseInGlobalSpace
		, SharedData->bTransferInputCurves);

	InstanceData->ControlRigVariableMappings.UpdateCurveInputs(InstanceData->ControlRig, SharedData->InputMapping, InOutput.Curves);
}

void FAnimNextControlRigTask::UpdateOutput(UE::AnimNext::FEvaluationVM& VM, UE::AnimNext::FKeyframeState& KeyFrameState, UControlRig* ControlRig, UE::AnimNext::FKeyframeState& InOutput) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (!CanExecute(ControlRig))
	{
		return;
	}

	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	InstanceData->ControlRigHierarchyMappings.UpdateOutput(ControlRig
		, InOutput
		, InstanceData->OutputSettings
		, InstanceData->NodeMappingContainer
		, InstanceData->bExecute
		, SharedData->bTransferPoseInGlobalSpace);

	InstanceData->ControlRigVariableMappings.UpdateCurveOutputs(InstanceData->ControlRig, SharedData->OutputMapping, InOutput.Curves);
}

bool FAnimNextControlRigTask::CanExecute(const UControlRig* ControlRig) const
{
	if (CVarControlRigDisableExecutionAnimNext->GetInt() != 0)
	{
		return false;
	}

	if (!InstanceData->ControlRigHierarchyMappings.CanExecute())
	{
		return false;
	}

	if (ControlRig)
	{
		return ControlRig->CanExecute();
	}

	return false;
}

void FAnimNextControlRigTask::QueueControlRigDrawInstructions(UControlRig* ControlRig, FRigVMDrawInterface* DebugDrawInterface, const FTransform& ComponentTransform) const
{
	ensure(ControlRig);
	ensure(DebugDrawInterface);

	if (ControlRig && DebugDrawInterface)
	{
		for (FRigVMDrawInstruction& Instruction : ControlRig->GetDrawInterface().Instructions)
		{
			if (!Instruction.IsValid())
			{
				continue;
			}

			const FTransform InstructionTransform = Instruction.Transform * ComponentTransform;
			Instruction.Transform = InstructionTransform;
			DebugDrawInterface->DrawInstruction(Instruction);
		}
	}
}
