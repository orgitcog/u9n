// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigModularRigCommands.h"

#define LOCTEXT_NAMESPACE "ControlRigModularRigCommands"

void FControlRigModularRigCommands::RegisterCommands()
{
	UI_COMMAND(AddModuleItem, "New Module", "Add new module to the rig.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RenameModuleItem, "Rename", "Rename module to the rig.", EUserInterfaceActionType::Button, FInputChord(EKeys::F2));
	UI_COMMAND(DeleteModuleItem, "Delete", "Delete module from the rig.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
	UI_COMMAND(MirrorModuleItem, "Mirror", "Mirror module from the rig.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReresolveModuleItem, "Auto Resolve", "Auto Resolve secondary connectors.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SwapModuleClassItem, "Swap Module", "Swap module of common class.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CopyModuleSettings, "Copy Settings", "Copies the module(s) settings to the clipboard.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::C));
	UI_COMMAND(PasteModuleSettings, "Paste Settings", "Pastes the module(s) settings from the clipboard.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::V));
}

#undef LOCTEXT_NAMESPACE
