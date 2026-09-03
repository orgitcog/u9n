// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/WidgetAnimationHandle.h"

#include "Animation/UMGSequencePlayer.h"
#include "Animation/WidgetAnimationState.h"
#include "Blueprint/UserWidget.h"
#include "EdGraph/EdGraphPin.h"

FWidgetAnimationHandle::FWidgetAnimationHandle()
{
}

FWidgetAnimationHandle::FWidgetAnimationHandle(UUserWidget* InUserWidget, int32 InStateIndex, uint32 InStateSerial)
	: WeakUserWidget(InUserWidget)
	, StateIndex(InStateIndex)
	, StateSerial(InStateSerial)
{
}

bool FWidgetAnimationHandle::IsValid() const
{
	return (WeakUserWidget.IsValid() && StateIndex >= 0);
}

UUMGSequencePlayer* FWidgetAnimationHandle::GetSequencePlayer() const
{
	if (FWidgetAnimationState* State = GetAnimationState())
	{
		return State->GetOrCreateLegacyPlayer();
	}
	return nullptr;
}

FWidgetAnimationState* FWidgetAnimationHandle::GetAnimationState() const
{
	if (UUserWidget* UserWidget = WeakUserWidget.Get())
	{
		if (UserWidget->ActiveAnimations.IsValidIndex(StateIndex))
		{
			FWidgetAnimationState& ActiveState = UserWidget->ActiveAnimations[StateIndex];
			if (ActiveState.SerialNumber == StateSerial)
			{
				return &ActiveState;
			}
		}
	}
	return nullptr;
}

FName FWidgetAnimationHandle::GetUserTag() const
{
	if (FWidgetAnimationState* State = GetAnimationState())
	{
		return State->GetUserTag();
	}
	return NAME_None;
}

void FWidgetAnimationHandle::SetUserTag(FName InUserTag)
{
	if (FWidgetAnimationState* State = GetAnimationState())
	{
		State->SetUserTag(InUserTag);
	}
}

FName UWidgetAnimationHandleFunctionLibrary::GetUserTag(const FWidgetAnimationHandle& Target)
{
	return Target.GetUserTag();
}

void UWidgetAnimationHandleFunctionLibrary::SetUserTag(FWidgetAnimationHandle& Target, FName InUserTag)
{
	Target.SetUserTag(InUserTag);
}

