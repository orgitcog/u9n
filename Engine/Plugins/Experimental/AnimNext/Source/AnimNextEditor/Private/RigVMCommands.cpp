// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCommands.h"

#define LOCTEXT_NAMESPACE "AnimNextRigVMCommands"

namespace UE::AnimNext
{

FRigVMCommands::FRigVMCommands()
	: TCommands<FRigVMCommands>("AnimNextRigVM", LOCTEXT("AnimNextRigVMCommands", "RigVM"), NAME_None, "AnimNextStyle")
{
}

void FRigVMCommands::RegisterCommands()
{
	UI_COMMAND(Compile, "Compile", "Compile all relevant assets", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AutoCompile, "Auto Compile", "Automatically compile on every edit", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(CompileWholeWorkspace, "Compile Whole Workspace", "When manually compiling, whether to compile the current file or the whole workspace", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(CompileDirtyFiles, "Compile Dirty Files", "When manually compiling, whether to compile only dirty files or files with errors. Files that are not dirty and do not have errors will get skipped.", EUserInterfaceActionType::ToggleButton, FInputChord());
}

}

#undef LOCTEXT_NAMESPACE
