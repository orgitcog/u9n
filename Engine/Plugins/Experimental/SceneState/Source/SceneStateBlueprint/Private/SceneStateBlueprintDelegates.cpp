// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateBlueprintDelegates.h"

namespace UE::SceneState::Graph
{

TMulticastDelegate<void(const FBlueprintDebugObjectChange&)> OnBlueprintDebugObjectChanged;

} // UE::SceneState
