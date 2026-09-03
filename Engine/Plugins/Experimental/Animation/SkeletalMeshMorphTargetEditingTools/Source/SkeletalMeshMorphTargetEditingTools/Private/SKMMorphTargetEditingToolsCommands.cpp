// Copyright Epic Games, Inc. All Rights Reserved.

#include "SKMMorphTargetEditingToolsCommands.h"
#include "SKMMorphTargetEditingToolsStyle.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshMorphTargetEditingToolsCommands"



FSkeletalMeshMorphTargetEditingToolsCommands::FSkeletalMeshMorphTargetEditingToolsCommands()
	: TCommands<FSkeletalMeshMorphTargetEditingToolsCommands>(
		"SkeletalMeshMorphTargetEditingToolsCommands", // Context name for fast lookup
		NSLOCTEXT("Contexts", "SkeletalMeshMorphTargetEditingToolsCommands", "Skeletal Mesh Morph Target Editing Tools"), // Localized context name for displaying
		NAME_None, // Parent
		FSkeletalMeshMorphTargetEditingToolsStyle::Get().GetStyleSetName() // Icon Style Set
		)
{
}

void FSkeletalMeshMorphTargetEditingToolsCommands::RegisterCommands()
{
	UI_COMMAND(BeginMorphTargetTool, "Morph", "Edit Morph Targets", EUserInterfaceActionType::ToggleButton, FInputChord());
	
	UI_COMMAND(BeginMorphTargetSculptTool, "Sculpt Morph Target", "MorphTarget Sculpt Tool", EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE

