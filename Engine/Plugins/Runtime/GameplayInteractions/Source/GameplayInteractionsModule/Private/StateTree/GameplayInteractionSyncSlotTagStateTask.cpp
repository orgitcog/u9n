// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionSyncSlotTagStateTask.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionSyncSlotTagStateTask)

#define LOCTEXT_NAMESPACE "GameplayInteractions"

FGameplayInteractionSyncSlotTagStateTask::FGameplayInteractionSyncSlotTagStateTask()
{
	// No tick needed.
	bShouldCallTick = false;
	// No need to update bound properties after enter state, we assume the slot does not change.
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FGameplayInteractionSyncSlotTagStateTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

EStateTreeRunStatus FGameplayInteractionSyncSlotTagStateTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	InstanceData.OnEventHandle.Reset();

	if (!InstanceData.TargetSlot.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagStateTask] Expected valid TargetSlot handle."));
		return EStateTreeRunStatus::Failed;
	}

	FOnSmartObjectEvent* OnEventDelegate = SmartObjectSubsystem.GetSlotEventDelegate(InstanceData.TargetSlot);
	if (OnEventDelegate == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagStateTask] Expected to find event delegate for the slot."));
		return EStateTreeRunStatus::Failed;
	}

	
	// Check initial state
	bool bHasTagToMonitor = false;
	const bool bValidSlotView = SmartObjectSubsystem.ReadSlotData(InstanceData.TargetSlot
		, [this, &InstanceData, &bHasTagToMonitor](const FConstSmartObjectSlotView& SlotView)
		{
			InstanceData.bBreakSignalled = false;
			bHasTagToMonitor = SlotView.GetTags().HasTag(TagToMonitor);
		});

	if (!bValidSlotView)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagStateTask] Expected valid slot view."));
		return EStateTreeRunStatus::Failed;
	}

	if (!bHasTagToMonitor)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagStateTask] Sync state (initial): [%s] -> Event %s"), *TagToMonitor.ToString(), *BreakEventTag.ToString());

		// Signal the other slot to change.
		Context.SendEvent(BreakEventTag);
		SmartObjectSubsystem.SendSlotEvent(InstanceData.TargetSlot, BreakEventTag);
		InstanceData.bBreakSignalled = true;
	}

	if (!InstanceData.bBreakSignalled)
	{
		InstanceData.OnEventHandle = OnEventDelegate->AddLambda([TargetSlot = InstanceData.TargetSlot, this, InstanceDataRef = Context.GetInstanceDataStructRef(*this), SmartObjectSubsystem = &SmartObjectSubsystem, WeakExecutionContext = Context.MakeWeakExecutionContext()](const FSmartObjectEventData& Data) mutable
		{
			if (TargetSlot == Data.SlotHandle
				&& Data.Reason == ESmartObjectChangeReason::OnTagRemoved)
			{
				check(InstanceDataRef.IsValid());
				if (FInstanceDataType* InstanceData = InstanceDataRef.GetPtr())
				{
					if (!InstanceData->bBreakSignalled && Data.Tag.MatchesTag(TagToMonitor))
					{
						UE_VLOG_UELOG(WeakExecutionContext.GetOwner().Get(), LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagStateTask] Sync state: [%s] -> Event %s"), *TagToMonitor.ToString(), *BreakEventTag.ToString());

						SmartObjectSubsystem->SendSlotEvent(InstanceData->TargetSlot, BreakEventTag);
						WeakExecutionContext.SendEvent(BreakEventTag);
						InstanceData->bBreakSignalled = true;
					}
				}
			}
		});
	}

	return EStateTreeRunStatus::Running;
}

void FGameplayInteractionSyncSlotTagStateTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.OnEventHandle.IsValid())
	{
		if (FOnSmartObjectEvent* OnEventDelegate = SmartObjectSubsystem.GetSlotEventDelegate(InstanceData.TargetSlot))
		{
			OnEventDelegate->Remove(InstanceData.OnEventHandle);
		}
	}
	InstanceData.OnEventHandle.Reset();

	if (!InstanceData.bBreakSignalled)
	{
		Context.SendEvent(BreakEventTag);
		SmartObjectSubsystem.SendSlotEvent(InstanceData.TargetSlot, BreakEventTag);
		InstanceData.bBreakSignalled = true;
	}
}

#if WITH_EDITOR
EDataValidationResult FGameplayInteractionSyncSlotTagStateTask::Compile(UE::StateTree::ICompileNodeContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!TagToMonitor.IsValid())
	{
		Context.AddValidationError(LOCTEXT("MissingTagToMonitor", "TagToMonitor property is empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	if (!BreakEventTag.IsValid())
	{
		Context.AddValidationError(LOCTEXT("MissingBreakEventTag", "BreakEventTag property is empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

FText FGameplayInteractionSyncSlotTagStateTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
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
		? LOCTEXT("SyncSlotTagStateRich", "<b>Sync Tag State</> {Tag} <s>on slot</> {Slot}")
		: LOCTEXT("SyncSlotTagState", "Sync Tag State {Tag} on slot {Slot}");

	return FText::FormatNamed(Format,
		TEXT("Tag"), FText::FromString(TagToMonitor.ToString()),
		TEXT("Slot"), SlotValue);
}
#endif

#undef LOCTEXT_NAMESPACE
