// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDBaseSceneObject.h"
#include "GameFramework/Actor.h"

void FChaosVDBaseSceneObject::SetParentParentActor(AActor* NewParent)
{
	ParentActor = NewParent;
}
