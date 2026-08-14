// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/MassFindSmartObjectTargetTask.h"

#include "MassAIBehaviorTypes.h"
#include "SmartObjectSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FMassFindSmartObjectTargetTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	
	return true;
}

EStateTreeRunStatus FMassFindSmartObjectTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.SmartObjectLocation.Reset();

	if (!InstanceData.ClaimedSlot.SmartObjectHandle.IsValid())
	{
		MASSBEHAVIOR_LOG(Error, TEXT("Invalid claimed smart object ID."));
		return EStateTreeRunStatus::Failed;
	}

	const FTransform Transform = SmartObjectSubsystem.GetSlotTransform(InstanceData.ClaimedSlot).Get(FTransform::Identity);

	InstanceData.SmartObjectLocation.EndOfPathIntent = EMassMovementAction::Stand;
	InstanceData.SmartObjectLocation.EndOfPathPosition = Transform.GetLocation();
	
	return EStateTreeRunStatus::Running;
}
