// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditorModes.h"

namespace UE::SceneState::Editor
{
	class FSceneStateBlueprintEditor;
}

namespace UE::SceneState::Editor
{

/** Defines the layout for the 'Blueprint' mode in the Scene State Editor */
class FBlueprintAppMode : public FBlueprintEditorApplicationMode
{
public:
	explicit FBlueprintAppMode(const TSharedRef<FSceneStateBlueprintEditor>& InBlueprintEditor);
};

} // UE::SceneState::Editor

