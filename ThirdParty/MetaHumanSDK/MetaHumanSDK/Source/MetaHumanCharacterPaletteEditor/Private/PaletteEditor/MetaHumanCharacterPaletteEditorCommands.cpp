// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaletteEditor/MetaHumanCharacterPaletteEditorCommands.h"

#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterPaletteEditor"

FMetaHumanCharacterPaletteEditorCommands::FMetaHumanCharacterPaletteEditorCommands()
	: TCommands<FMetaHumanCharacterPaletteEditorCommands>(
		TEXT("MetaHumanCharacterPaletteEditor"),
		LOCTEXT("ContextDescription", "MetaHuman Character Palette Editor"),
		NAME_None,
		FAppStyle::GetAppStyleSetName()
	)
{
}

void FMetaHumanCharacterPaletteEditorCommands::RegisterCommands()
{
	// These are part of the asset editor UI
	UI_COMMAND(Build, "Build", "Build the Character Palette to apply all changes made while editing", EUserInterfaceActionType::Button, FInputChord{});
}

#undef LOCTEXT_NAMESPACE
