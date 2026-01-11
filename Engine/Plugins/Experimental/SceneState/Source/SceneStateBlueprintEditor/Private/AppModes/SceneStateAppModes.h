// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"

class FText;

namespace UE::SceneState::Editor
{

/** Describes all the available App Modes for the Scene State Editor */
struct FAppModes
{
	/** Available App Modes */
	static const FName Blueprint;

	/** Retrieves the display name for a given app mode */
	static FText GetAppModeDisplayName(FName InAppMode);
};

} // UE::SceneState::Editor
