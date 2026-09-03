// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

namespace UE::SceneState::Graph
{

/** Type of state machine node */
enum class EStateMachineNodeType : uint8
{
	Unspecified,
	Entry,
	Exit,
	State,
	Transition,
	Task,
	Conduit,
};

} // UE::SceneState::Graph
