// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateTaskBlueprint.h"
#include "Tasks/SceneStateTaskGeneratedClass.h"

UClass* USceneStateTaskBlueprint::GetBlueprintClass() const
{
	return USceneStateTaskGeneratedClass::StaticClass();
}

void USceneStateTaskBlueprint::GetReparentingRules(TSet<const UClass*>& OutAllowedChildrenOfClasses, TSet<const UClass*>& OutDisallowedChildrenOfClasses) const
{
	OutAllowedChildrenOfClasses.Add(USceneStateTaskGeneratedClass::StaticClass());
}

bool USceneStateTaskBlueprint::SupportedByDefaultBlueprintFactory() const
{
	return false;
}
