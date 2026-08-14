// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/SceneStateTaskInstance.h"

namespace UE::SceneState::Private
{
	
uint16 GetNextInstanceId()
{
	static uint16 Id = 0;
	return ++Id; // Starts with 1
}

} // UE::SceneState::Private

FSceneStateTaskInstance::FSceneStateTaskInstance()
	: InstanceId(UE::SceneState::Private::GetNextInstanceId())
{
}
