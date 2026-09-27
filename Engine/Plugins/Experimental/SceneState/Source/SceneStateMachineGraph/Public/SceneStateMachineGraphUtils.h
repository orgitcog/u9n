// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class UEdGraph;

namespace UE::SceneState::Graph
{
	SCENESTATEMACHINEGRAPH_API bool CanDirectlyRemoveGraph(UEdGraph* InGraph);

	SCENESTATEMACHINEGRAPH_API void RemoveGraph(UEdGraph* InGraph);

} // UE::SceneState::Graph
