// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/AnimationAuthoringSettings.h"

UAnimationAuthoringSettings::UAnimationAuthoringSettings(const FObjectInitializer& ObjectInitializer)
	: UDeveloperSettings(ObjectInitializer)
{}

UAnimationAuthoringSettings::FOnUpdateSettings UAnimationAuthoringSettings::OnSettingsChange;

void UAnimationAuthoringSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	OnSettingsChange.Broadcast(this);
}
