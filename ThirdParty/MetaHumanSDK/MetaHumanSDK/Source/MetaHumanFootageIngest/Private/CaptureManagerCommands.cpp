// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureManagerCommands.h"
#include "MetaHumanFootageRetrievalWindowStyle.h"

#define LOCTEXT_NAMESPACE "MetaHumanCaptureManagerCommands"

FCaptureManagerCommands::FCaptureManagerCommands()
	: TCommands<FCaptureManagerCommands>(TEXT("CaptureManager"),
		LOCTEXT("CaptureManagerCommandsContext", "CaptureManager Toolkit Context"),
		NAME_None,
		FMetaHumanFootageRetrievalWindowStyle::Get().GetStyleSetName())
{}

void FCaptureManagerCommands::RegisterCommands()
{
	UI_COMMAND(Save, "Save", "Save imported assets for the currently selected source", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(SaveAll, "Save All", "Save all imported assets", EUserInterfaceActionType::Button, FInputChord{});

	UI_COMMAND(Refresh, "Refresh", "Refresh Take View for this Capture Source", EUserInterfaceActionType::Button, FInputChord{});

	UI_COMMAND(StartStopCapture, "Start/Stop Capture", "Start/stop capturing on a remote device", EUserInterfaceActionType::ToggleButton, FInputChord{});
}

#undef LOCTEXT_NAMESPACE