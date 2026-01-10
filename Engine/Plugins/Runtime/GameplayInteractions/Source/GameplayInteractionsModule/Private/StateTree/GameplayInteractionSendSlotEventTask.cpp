// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionSendSlotEventTask.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionSendSlotEventTask)

#define LOCTEXT_NAMESPACE "GameplayInteractions"

FGameplayInteractionSendSlotEventTask::FGameplayInteractionSendSlotEventTask()
{
	// No tick needed.
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
}

bool FGameplayInteractionSendSlotEventTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);

	bShouldStateChangeOnReselect = bShouldTriggerOnReselect;
	// Copy properties on exit state if the event is sent then.
	bShouldCopyBoundPropertiesOnExitState = (Trigger == EGameplayInteractionTaskTrigger::OnExitState);

	return true;
}

EStateTreeRunStatus FGameplayInteractionSendSlotEventTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (Trigger == EGameplayInteractionTaskTrigger::OnEnterState)
	{
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

		if (InstanceData.TargetSlot.IsValid())
		{
			// Send the event
			SmartObjectSubsystem.SendSlotEvent(InstanceData.TargetSlot, EventTag, Payload);
		}
		else
		{
			UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSendSlotEventTask] Expected valid TargetSlot handle."));
		}
	}

	return EStateTreeRunStatus::Running;
}

void FGameplayInteractionSendSlotEventTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const bool bLastStateFailed = Transition.CurrentRunStatus == EStateTreeRunStatus::Failed
								|| (bHandleExternalStopAsFailure &&  Transition.CurrentRunStatus == EStateTreeRunStatus::Stopped);
	
	if (Trigger == EGameplayInteractionTaskTrigger::OnExitState
		|| (bLastStateFailed && Trigger == EGameplayInteractionTaskTrigger::OnExitStateFailed)
		|| (!bLastStateFailed && Trigger == EGameplayInteractionTaskTrigger::OnExitStateSucceeded))
	{
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

		if (InstanceData.TargetSlot.IsValid())
		{
			// Send the event
			SmartObjectSubsystem.SendSlotEvent(InstanceData.TargetSlot, EventTag, Payload);
		}
		else
		{
			UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSendSlotEventTask] Expected valid TargetSlot handle."));
		}
	}
}

#if WITH_EDITOR
EDataValidationResult FGameplayInteractionSendSlotEventTask::Compile(UE::StateTree::ICompileNodeContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!EventTag.IsValid() && !Payload.IsValid())
	{
		Context.AddValidationError(LOCTEXT("MissingEventData", "EventTag and Payload properties are empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

FText FGameplayInteractionSendSlotEventTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	// Slot
	FText SlotValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, TargetSlot)), Formatting);
	if (SlotValue.IsEmpty())
	{
		SlotValue = LOCTEXT("None", "None");
	}

	const FText Format = (Formatting == EStateTreeNodeFormatting::RichText)
		? LOCTEXT("SendSlotEventRich", "<b>Send Event</> {Tag} <s>to slot</> {Slot}")
		: LOCTEXT("SendSlotEvent", "Send Event {Tag} to slot {Slot}");

	return FText::FormatNamed(Format,
		TEXT("Tag"), FText::FromString(EventTag.ToString()),
		TEXT("Slot"), SlotValue);
}
#endif

#undef LOCTEXT_NAMESPACE
