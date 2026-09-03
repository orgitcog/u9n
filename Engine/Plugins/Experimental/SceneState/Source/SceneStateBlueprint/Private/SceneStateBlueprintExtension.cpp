// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateBlueprintExtension.h"
#include "SceneStateBlueprint.h"

void USceneStateBlueprintExtension::GetAllGraphs(TArray<UEdGraph*>& OutGraphs) const
{
	if (USceneStateBlueprint* Blueprint = Cast<USceneStateBlueprint>(GetOuter()))
	{
		OutGraphs.Append(Blueprint->StateMachineGraphs);
	}
}
