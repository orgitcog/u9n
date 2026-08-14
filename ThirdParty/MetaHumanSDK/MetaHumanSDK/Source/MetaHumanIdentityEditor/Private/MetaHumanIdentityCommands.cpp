// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityCommands.h"
#include "MetaHumanIdentityStyle.h"

#define LOCTEXT_NAMESPACE "FMetaHumanIdentityEditorCommands"

FMetaHumanIdentityEditorCommands::FMetaHumanIdentityEditorCommands()
	: TCommands<FMetaHumanIdentityEditorCommands>(TEXT("MetaHuman Identity"),
										  NSLOCTEXT("Contexts", "FMetaHumanIdentityModule", "MetaHuman Identity Asset Editor"),
										  NAME_None,
										  FMetaHumanIdentityStyle::Get().GetStyleSetName())
{}

void FMetaHumanIdentityEditorCommands::RegisterCommands()
{
	UI_COMMAND(ComponentsFromMesh, "Create Components from Mesh", "Create the required MetaHuman Identity Components from a Mesh", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(ComponentsFromFootage, "Create Components from Footage", "Create the required MetaHuman Identity Components from some Footage", EUserInterfaceActionType::Button, FInputChord{});

	UI_COMMAND(TrackCurrent, "Track Markers (Active Frame)", "Track the active frame", EUserInterfaceActionType::Button, FInputChord{});

	UI_COMMAND(ActivateMarkersForCurrent, "Activate Markers (Active Frame)", "Activate the display of markers for the selected frame", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(ActivateMarkersForAll, "Activate Markers (All Frames)", "Activate the display of markers for all promoted frames", EUserInterfaceActionType::Button, FInputChord{});

	UI_COMMAND(ResetTemplateMesh, "Reset Template Mesh", "Resets the Template Mesh", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(IdentitySolve, "MetaHuman Identity Solve", "MetaHuman Identity Solve", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(MeshToMetaHumanDNAOnly, "Auto-Rig MetaHuman Identity", "", EUserInterfaceActionType::Button, FInputChord{}); //note: the tooltip is dynamic, so its string is empty here to avoid confusion
	UI_COMMAND(ImportDNA, "Import DNA", "Load DNA from disk to create the MetaHuman Identity", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(ExportDNA, "Export DNA", "Save DNA and brows data for current MetaHuman Identity", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(FitTeeth, "Fit Teeth", "Apply teeth fitting to the MetaHuman Identity", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(PrepareForPerformance, "Prepare for Performance", "Prepare the MetaHuman Identity so it can be used for processing Performance assets", EUserInterfaceActionType::Button, FInputChord{});

	UI_COMMAND(RigidFitCurrent, "Rigid Fit (Active Frame)", "Rigid fit the head of the active frame", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(RigidFitAll, "Rigid Fit (All Frames)", "Rigid fit the head in all frames", EUserInterfaceActionType::Button, FInputChord{});

	UI_COMMAND(PromoteFrame, "Promote Frame", "Promote a Frame", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(DemoteFrame, "Demote Frame", "Demote a Frame", EUserInterfaceActionType::Button, FInputChord{});

	UI_COMMAND(ToggleConformalMesh, "Template Mesh", "Toggle the visibility of the Template Mesh", EUserInterfaceActionType::ToggleButton, FInputChord{ EKeys::Eight });
	UI_COMMAND(ToggleRig, "Skeletal Mesh", "Toggle the visibility of the Skeletal Mesh", EUserInterfaceActionType::ToggleButton, FInputChord{ EKeys::Nine });
	UI_COMMAND(ToggleCurrentPose, "Current Pose", "Toggle selected pose", EUserInterfaceActionType::ToggleButton, FInputChord{});

	UI_COMMAND(TogglePlayback, "Toggle playback", "Toggle playback of selected animation", EUserInterfaceActionType::ToggleButton, FInputChord{ EKeys::SpaceBar });
	UI_COMMAND(ExportTemplateMesh, "Export Template Mesh", "Export conformed template mesh as a static mesh", EUserInterfaceActionType::Button, FInputChord{});
}

#undef LOCTEXT_NAMESPACE
