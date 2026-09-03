// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneStateComponent.h"
#include "AvaSceneStatePlayer.h"

UAvaSceneStateComponent::UAvaSceneStateComponent(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer.SetDefaultSubobjectClass<UAvaSceneStatePlayer>(UAvaSceneStateComponent::SceneStatePlayerName))
{
}
