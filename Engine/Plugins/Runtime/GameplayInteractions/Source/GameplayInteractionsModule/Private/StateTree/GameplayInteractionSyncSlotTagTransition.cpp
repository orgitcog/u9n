// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionSyncSlotTagTransition.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionSyncSlotTagTransition)

#define LOCTEXT_NAMESPACE "GameplayInteractions"

FGameplayInteractionSyncSlotTagTransitionTask::FGameplayInteractionSyncSlotTagTransitionTask()
{
	// No tick needed.
	bShouldCallTick = false;
	// No need to update bound properties after enter state, we assume the slot does not change.
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FGameplayInteractionSyncSlotTagTransitionTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

EStateTreeRunStatus FGameplayInteractionSyncSlotTagTransitionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.OnEventHandle.Reset();
	
	if (!InstanceData.TargetSlot.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Expected valid TargetSlot handle."));
		return EStateTreeRunStatus::Failed;
	}

	FOnSmartObjectEvent* OnEventDelegate = SmartObjectSubsystem.GetSlotEventDelegate(InstanceData.TargetSlot);
	if (OnEventDelegate == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Expected to find event delegate for the slot."));
		return EStateTreeRunStatus::Failed;
	}

	bool bHasTransitionToTag = false;
	bool bHasTransitionFromTag = false;
	const bool bValidSlowView = SmartObjectSubsystem.ReadSlotData(InstanceData.TargetSlot
		, [this, &bHasTransitionToTag, &bHasTransitionFromTag](const FConstSmartObjectSlotView& SlotView)
		{
			if (SlotView.GetTags().HasTag(TransitionToTag))
			{
				bHasTransitionToTag = true;
			}
			else if (SlotView.GetTags().HasTag(TransitionFromTag))
			{
				bHasTransitionFromTag = true;
			}
		});

	if (!bValidSlowView)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Expected valid slot view."));
		return EStateTreeRunStatus::Failed;
	}

	// Check initial state
	InstanceData.State = EGameplayInteractionSyncSlotTransitionState::WaitingForFromTag;
	if (bHasTransitionToTag)
	{
		// Signal the other slot to change.
		Context.SendEvent(TransitionEventTag);
		InstanceData.State = EGameplayInteractionSyncSlotTransitionState::Completed;

		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Sync transition (initial): (%s) WaitingForToTag match [%s] -> Event %s"),
			*LexToString(InstanceData.TargetSlot), *TransitionToTag.ToString(), *TransitionEventTag.ToString());
	}
	else if (bHasTransitionFromTag)
	{
		// Signal the other slot to change.
		SmartObjectSubsystem.SendSlotEvent(InstanceData.TargetSlot, TransitionEventTag);
		InstanceData.State = EGameplayInteractionSyncSlotTransitionState::WaitingForToTag;

		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Sync transition (initial): (%s) WaitingForFromTag match [%s] -> SOEvent %s"),
			*LexToString(InstanceData.TargetSlot), *TransitionFromTag.ToString(), *TransitionEventTag.ToString());
	}

	// Event queue and the node are safe to access in the delegate.
	// InstanceData can be moved in memory, so we need to capture what we need by value.
	if (InstanceData.State != EGameplayInteractionSyncSlotTransitionState::Completed)
	{
		InstanceData.OnEventHandle = OnEventDelegate->AddLambda(
			[this, TargetSlot = InstanceData.TargetSlot, InstanceDataRef = Context.GetInstanceDataStructRef(*this), SmartObjectSubsystem = &SmartObjectSubsystem, WeakExecutionContext = Context.MakeWeakExecutionContext()](const FSmartObjectEventData& Data) mutable
			{
				if (TargetSlot == Data.SlotHandle
					&& Data.Reason == ESmartObjectChangeReason::OnTagAdded)
				{
					if (FInstanceDataType* InstanceData = InstanceDataRef.GetPtr())
					{
						UE_VLOG_UELOG(WeakExecutionContext.GetOwner().Get(), LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Sync transition: (%s) Tag %s added"),
							*LexToString(InstanceData->TargetSlot), *Data.Tag.ToString());

						if (InstanceData->State == EGameplayInteractionSyncSlotTransitionState::WaitingForFromTag)
						{
							if (Data.Tag.MatchesTag(TransitionFromTag))
							{
								SmartObjectSubsystem->SendSlotEvent(InstanceData->TargetSlot, TransitionEventTag);
								InstanceData->State = EGameplayInteractionSyncSlotTransitionState::WaitingForToTag;

								UE_VLOG_UELOG(WeakExecutionContext.GetOwner().Get(), LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Sync transition: (%s) WaitingForFromTag match [%s] -> SOEvent %s"),
									*LexToString(InstanceData->TargetSlot), *TransitionFromTag.ToString(), *TransitionEventTag.ToString());
							}
						}
						else if (InstanceData->State == EGameplayInteractionSyncSlotTransitionState::WaitingForToTag)
						{
							if (Data.Tag.MatchesTag(TransitionToTag))
							{
								WeakExecutionContext.SendEvent(TransitionEventTag);
								InstanceData->State = EGameplayInteractionSyncSlotTransitionState::Completed;

								UE_VLOG_UELOG(WeakExecutionContext.GetOwner().Get(), LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Sync transition: (%s) WaitingForToTag match [%s] -> Event %s"),
									*LexToString(InstanceData->TargetSlot), *TransitionToTag.ToString(), *TransitionEventTag.ToString());
							}
						}
					}
				}
			});
	}

	return EStateTreeRunStatus::Running;
}

void FGameplayInteractionSyncSlotTagTransitionTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
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
}

#if WITH_EDITOR
EDataValidationResult FGameplayInteractionSyncSlotTagTransitionTask::Compile(UE::StateTree::ICompileNodeContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!TransitionFromTag.IsValid())
	{
		Context.AddValidationError(LOCTEXT("MissingTransitionFromTag", "TransitionFromTag property is empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	if (!TransitionToTag.IsValid())
	{
		Context.AddValidationError(LOCTEXT("MissingTransitionToTag", "TransitionToTag property is empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	if (!TransitionEventTag.IsValid())
	{
		Context.AddValidationError(LOCTEXT("MissingTransitionEventTag", "TransitionEventTag property is empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

FText FGameplayInteractionSyncSlotTagTransitionTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
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
		? LOCTEXT("SyncSlotTagTransitionRich", "<b>Sync Tag Transition</> <s>from</> {FromTag} <s>to</> {ToTag} <s>on slot</> {Slot}")
		: LOCTEXT("SyncSlotTagTransition", "Sync Tag Transition from {FromTag} to {ToTag} on slot {Slot}");

	return FText::FormatNamed(Format,
		TEXT("FromTag"), FText::FromString(TransitionFromTag.ToString()),
		TEXT("ToTag"), FText::FromString(TransitionToTag.ToString()),
		TEXT("Slot"), SlotValue);
}
#endif

#undef LOCTEXT_NAMESPACE
