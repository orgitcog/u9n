// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Tools/InteractiveToolsCommands.h"

class FMetaHumanCharacterPaletteEditorCommands : public TCommands<FMetaHumanCharacterPaletteEditorCommands>
{
public:
	FMetaHumanCharacterPaletteEditorCommands();

	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> Build;
};

