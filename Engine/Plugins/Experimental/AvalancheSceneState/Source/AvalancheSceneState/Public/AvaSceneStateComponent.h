// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateComponent.h"
#include "AvaSceneStateComponent.generated.h"

UCLASS(HideCategories=(Activation, AssetUserData, Cooking, Input, Navigation, Tags))
class UAvaSceneStateComponent : public USceneStateComponent
{
	GENERATED_BODY()

	UAvaSceneStateComponent(const FObjectInitializer& InObjectInitializer);
};
