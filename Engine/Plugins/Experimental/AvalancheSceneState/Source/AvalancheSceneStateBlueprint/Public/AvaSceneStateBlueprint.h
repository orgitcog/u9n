// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateBlueprint.h"
#include "AvaSceneStateBlueprint.generated.h"

/**
 * Scene State Blueprint child that overrides the Property Binder class with a child class
 * @see UAvaSceneStateBinder
 */
UCLASS(MinimalAPI, DisplayName="Motion Design Scene State Blueprint")
class UAvaSceneStateBlueprint : public USceneStateBlueprint
{
	GENERATED_BODY()
};
