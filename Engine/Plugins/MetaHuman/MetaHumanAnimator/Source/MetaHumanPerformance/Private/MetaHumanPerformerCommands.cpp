// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformerCommands.h"
#include "UI/MetaHumanPerformanceStyle.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FMetaHumanPerformerCommands"

FMetaHumanPerformanceCommands::FMetaHumanPerformanceCommands() 
	: TCommands<FMetaHumanPerformanceCommands>(TEXT("Performance"),
											   LOCTEXT("FMetaHumanPerformerModule", "Performance Asset Editor"),
											   NAME_None,
											   FMetaHumanPerformanceStyle::Get().GetStyleSetName())
{}

void FMetaHumanPerformanceCommands::RegisterCommands()
{
	UI_COMMAND(StartProcessingShot, "Process", "Process the current shot to generate an animation", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(CancelProcessingShot, "Cancel", "Cancel the processing of the current shot", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(ExportAnimation, "Export Animation", "Bake an Animation Sequence for the current shot", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(ExportLevelSequence, "Export Level Sequence", "Exports a Level Sequence that matches the sequence in this Performance", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(ToggleRig, "Skeletal Mesh", "Toggle Skeletal Mesh visibility", EUserInterfaceActionType::ToggleButton, FInputChord{ EKeys::Nine });
	UI_COMMAND(ToggleFootage, "Footage", "Toggle Footage visibility", EUserInterfaceActionType::ToggleButton, FInputChord{ EKeys::Eight });
	UI_COMMAND(ToggleControlRigDisplay, "Control Rig", "Toggle the Control Rig display", EUserInterfaceActionType::ToggleButton, FInputChord{ EKeys::C });

	ViewSetupStore.SetNum(4);
	ViewSetupRestore.SetNum(ViewSetupStore.Num());

	UI_COMMAND(ViewSetupStore[0], "View Setup 1 Store", "Save the view setup in slot 1", EUserInterfaceActionType::Button, FInputChord{ EKeys::One, EModifierKey::Shift | EModifierKey::Control });
	UI_COMMAND(ViewSetupRestore[0], "View Setup 1 Restore", "Restore the view setup in slot 1", EUserInterfaceActionType::Button, FInputChord{ EKeys::One, EModifierKey::Control });

	UI_COMMAND(ViewSetupStore[1], "View Setup 2 Store", "Save the view setup in slot 2", EUserInterfaceActionType::Button, FInputChord{ EKeys::Two, EModifierKey::Shift | EModifierKey::Control });
	UI_COMMAND(ViewSetupRestore[1], "View Setup 2 Restore", "Restore the view setup in slot 2", EUserInterfaceActionType::Button, FInputChord{ EKeys::Two, EModifierKey::Control });

	UI_COMMAND(ViewSetupStore[2], "View Setup 3 Store", "Save the view setup in slot 3", EUserInterfaceActionType::Button, FInputChord{ EKeys::Three, EModifierKey::Shift | EModifierKey::Control });
	UI_COMMAND(ViewSetupRestore[2], "View Setup 3 Restore", "Restore the view setup in slot 3", EUserInterfaceActionType::Button, FInputChord{ EKeys::Three, EModifierKey::Control });

	UI_COMMAND(ViewSetupStore[3], "View Setup 4 Store", "Save the view setup in slot 4", EUserInterfaceActionType::Button, FInputChord{ EKeys::Four, EModifierKey::Shift | EModifierKey::Control });
	UI_COMMAND(ViewSetupRestore[3], "View Setup 4 Restore", "Restore the view setup in slot 4", EUserInterfaceActionType::Button, FInputChord{ EKeys::Four, EModifierKey::Control });

	UI_COMMAND(ToggleShowFramesAsTheyAreProcessed, "Show frames as they are processed", "Toggle show frames as they are processed", EUserInterfaceActionType::Button, FInputChord{ EKeys::F, EModifierKey::Control });
}

#undef LOCTEXT_NAMESPACE