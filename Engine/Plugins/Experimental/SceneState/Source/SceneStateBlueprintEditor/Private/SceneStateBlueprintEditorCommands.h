// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

namespace UE::SceneState::Editor
{

class FBlueprintEditorCommands : public TCommands<FBlueprintEditorCommands>
{
public:
	FBlueprintEditorCommands();

	//~ Begin TCommands
	virtual void RegisterCommands() override;
	//~ End TCommands

	TSharedPtr<FUICommandInfo> AddStateMachine;

	TSharedPtr<FUICommandInfo> DebugPushEvent;
	TSharedPtr<FUICommandInfo> DebugRunSelection;
};

} // UE::SceneState::Editor
