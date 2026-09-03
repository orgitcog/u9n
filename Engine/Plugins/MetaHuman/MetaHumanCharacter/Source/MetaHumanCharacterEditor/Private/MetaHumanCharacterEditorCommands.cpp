// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorCommands.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Tools/MetaHumanCharacterEditorBodyConformTool.h"
#include "Tools/MetaHumanCharacterEditorBodyEditingTools.h"
#include "Tools/MetaHumanCharacterEditorConformTool.h"
#include "Tools/MetaHumanCharacterEditorCostumeTools.h"
#include "Tools/MetaHumanCharacterEditorEyesTool.h"
#include "Tools/MetaHumanCharacterEditorFaceEditingTools.h"
#include "Tools/MetaHumanCharacterEditorHeadModelTool.h"
#include "Tools/MetaHumanCharacterEditorMakeupTool.h"
#include "Tools/MetaHumanCharacterEditorPresetsTool.h"
#include "Tools/MetaHumanCharacterEditorSkinTool.h"
#include "Tools/MetaHumanCharacterEditorWardrobeTools.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

FMetaHumanCharacterEditorCommands::FMetaHumanCharacterEditorCommands()
	: TCommands<FMetaHumanCharacterEditorCommands>(
		TEXT("MetaHumanCharacterEditor"),
		LOCTEXT("MetaHumanCharacterEditorCommandsContext", "MetaHuman Character Editor"),
		NAME_None,
		FMetaHumanCharacterEditorStyle::Get().GetStyleSetName()
	)
{
}

void FMetaHumanCharacterEditorCommands::RegisterCommands()
{
	// These are part of the asset editor UI
	UI_COMMAND(SaveThumbnail, "Save Thumbnail", "Save the character preview thumbnail.", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(AutoRigFaceBlendShapes, "Create Full Rig", "Calls Auto-Rigging service and retrieves full DNA (blend shapes included).", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(AutoRigFaceJointsOnly, "Create Joints Only Rig", "Calls Auto-Rigging service and retrieves joints-only DNA.", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(RemoveFaceRig, "Remove Rig", "Remove rig from the character allowing it to be edited", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(DownloadHighResTextures2k, "Download 2k Resolution Textures", "Request to download 2k resolution textures", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(DownloadHighResTextures4k, "Download 4k Resolution Textures", "Request to download 4k resolution textures", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(DownloadHighResTextures8k, "Download 8k Resolution Textures", "Request to download 8k resolution textures", EUserInterfaceActionType::Button, FInputChord{});

	UI_COMMAND(RefreshPreview, "Refresh Preview", "Rebuild the preview actor", EUserInterfaceActionType::Button, FInputChord{});
}

FMetaHumanCharacterEditorDebugCommands::FMetaHumanCharacterEditorDebugCommands()
	: TCommands<FMetaHumanCharacterEditorDebugCommands>(
		TEXT("MetaHumanCharacterEditorDebug"),
		LOCTEXT("MetaHumanCharacterEditorDebugCommandsContext", "MetaHuman Character Editor Debug"),
		NAME_None,
		FMetaHumanCharacterEditorStyle::Get().GetStyleSetName()
	)
{
}

void FMetaHumanCharacterEditorDebugCommands::RegisterCommands()
{
	UI_COMMAND(ExportFaceSkelMesh, "Export Face Skeletal Mesh", "Exports the preview actor face to the project as a skeletal mesh", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(ExportBodySkelMesh, "Export Body Skeletal Mesh", "Exports the preview actor body to the project as a skeletal mesh", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(ExportCombinedSkelMesh, "Export Combined Skel Mesh", "Exports a combined face and body skeletal mesh skinned to the body skeleton.\nDoes not support facial animation.\nCan be used as a source body for resizing Chaos Outfits.", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(SaveFaceState, "Save Face State", "Saves the internal state of the edited face to a file", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(SaveFaceStateToDNA, "Save Face State to DNA", "Saves the internal state of the edited face to a DNA file", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(DumpFaceStateDataForAR, "Dump Face Data for AR", "Dumps Auto Rigging debug data for the face state to a folder", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(SaveBodyState, "Save Body State", "Saves the internal state of the edited body to a file", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(SaveFaceDNA, "Save Face DNA", "Saves the DNA of the edited face (if any) to a file", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(SaveBodyDNA, "Save Body DNA", "Saves the DNA of the edited body to a file", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(SaveFaceTextures, "Save Face Textures", "Saves all the synthesized textures of the face (if any) to a target folder", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(SaveEyePreset, "Save Eye Preset", "Saves the current eye settings as a preset", EUserInterfaceActionType::Button, FInputChord{});
	UI_COMMAND(TakeHighResScreenshot, "Take High Res Screenshot", "Takes a high resolution screenshot of the MetaHuman Character viewport", EUserInterfaceActionType::Button, FInputChord{});
}

FMetaHumanCharacterEditorToolCommands::FMetaHumanCharacterEditorToolCommands()
	: TInteractiveToolCommands<FMetaHumanCharacterEditorToolCommands>(
		TEXT("MetaHumanCharacterEditorTools"), // Context name for fast lookup and in the style to assign icons to commands
		LOCTEXT("MetaHumanCharacterEditorToolsCommandsContext", "MetaHumanh Character Editor Tools"),
		NAME_None,
		FMetaHumanCharacterEditorStyle::Get().GetStyleSetName()
	)
{
}

void FMetaHumanCharacterEditorToolCommands::RegisterCommands()
{
	TInteractiveToolCommands<FMetaHumanCharacterEditorToolCommands>::RegisterCommands();

	// These allow us to link up to pressed keys
	UI_COMMAND(AcceptOrCompleteActiveTool, "Accept", "Accept the active tool", EUserInterfaceActionType::Button, FInputChord{ EKeys::Enter });
	UI_COMMAND(CancelOrCompleteActiveTool, "Cancel", "Cancel the active tool or clear current selection", EUserInterfaceActionType::Button, FInputChord{ EKeys::Escape });

	// These get linked to various tool buttons.
	UI_COMMAND(LoadPresetsTools, "Presets", "Preset Library, Tools to manage your collection of monitored folders that provide characters for selection and blending", EUserInterfaceActionType::RadioButton, FInputChord{});
	UI_COMMAND(BeginPresetsTool, "Edit Presets", "Edit Presets Library", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(PresetProperties, "Presets Properties", "Edit Preset Properties", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(ApplyPreset, "Apply Preset", "Apply the Preset", EUserInterfaceActionType::ToggleButton, FInputChord{});

	UI_COMMAND(LoadBodyTools, "Body", "Body Geometry Editing, Tools to import, match, or parametrically edit the shape of the Character’s body", EUserInterfaceActionType::RadioButton, FInputChord{});
	UI_COMMAND(BeginBodyConformTools, "Conform", "Conform, Match the body’s geometry and skeleton to in-project assets, or import external MetaHuman DNA files", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginBodyConformImportBodyDNATool, "Import DNA", "Import a MetaHuman DNA File to alter the body geometry and, optionally, the joint positions of the skeleton", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginBodyConformImportBodyTemplateTool, "From Template", "Conform the body’s geometry to a Static Mesh, or the geometry and Skeleton to a Skeletal Mesh", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginBodyModelTool, "Model", "Model, Define Body proportions by setting semantic and measurement based values", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginBodyModelParametricTool, "Parametric", "Parametric Body Tool", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginBodyFixedCompatibilityTool, "Fixed (Compatibility)", "Fixed Compatibility Body Tool", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginBodyBlendTool, "Blend", "Blend, Select and Blend regions or the entire body between presets from your library", EUserInterfaceActionType::ToggleButton, FInputChord{});

	UI_COMMAND(LoadHeadTools, "Head", "Head Geometry Editing, Tools to import, match, or move and sculpt the shape of the Character’s head, Teeth, and Eyes", EUserInterfaceActionType::RadioButton, FInputChord{});
	UI_COMMAND(BeginConformTools, "Conform", "Conform, Match the head’s geometry and skeleton to in-project assets, or import external MetaHuman DNA files", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginConformImportDNATool, "Import DNA", "Import a MetaHuman DNA File to alter the head’s geometry and, optionally, the joint positions of the skeleton and morph targets", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginConformImportIdentityTool, "From Identity", "Conform from Identity, conform the head’s geometry to an Identity asset", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginConformImportTemplateTool, "From Template", "Conform from Template, conform the head’s geometry to a Static or Skeletal Mesh", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginHeadModelTools, "Teeth & Eyelashes", "Teeth and Eyelashes, select and configure the geometric details of teeth and eyelashes", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginHeadModelEyelashesTool, "Eyelashes", "Eyelashes, selection of eyelash presets with corresponding grooms", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginHeadModelTeethTool, "Teeth", "Teeth, parametrically adjust the teeth geometry", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginFaceMoveTool, "Transform", "Transform, rigidly transform a fixed set of the head’s features", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginFaceSculptTool, "Sculpt", "Sculpt, Add, remove, and manipulate markers to sculpt the appearance of the head", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginFaceBlendTool, "Blend", "Blend, Select and Blend regions or the entire head between presets from your library", EUserInterfaceActionType::ToggleButton, FInputChord{});

	UI_COMMAND(LoadMaterialsTools, "Materials", "Materials, Tools to configure the materials of all parts of your Character", EUserInterfaceActionType::RadioButton, FInputChord{});
	UI_COMMAND(BeginSkinTool, "Skin", "Skin, edit the Character’s look through skin parameters, textures, and accents", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginEyesTool, "Eyes", "Eyes, select from presets and customize the look of the Character’s eyes", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginMakeupTool, "Makeup", "Makeup, select from presets and customize the makeup of the face’s regions", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginHeadMaterialsTools, "Teeth & Eyelashes", "Teeth and Eyelashes, select and configure the materials details of teeth and eyelashes", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginHeadMaterialsTeethTool, "Teeth", "Teeth, customize the teeth’ material parameters", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginHeadMaterialsEyelashesTool, "Eyelashes", "Eyelashes, customize the eyelashes’ material parameters", EUserInterfaceActionType::ToggleButton, FInputChord{});

	UI_COMMAND(LoadHairAndClothingTools, "Hair & Clothing", "Hair and Clothing, Tools to configure available options for Clothing and Hair, select worn items, and configure their details", EUserInterfaceActionType::RadioButton, FInputChord{});
	UI_COMMAND(BeginWardrobeSelectionTool, "Selection", "Selection, Select clothing and hair to accessorize the Character", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(BeginCostumeDetailsTool, "Details", "Details, change the parameters for each selected clothing and groom accessory", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(PrepareAccessory, "Prepare", "Prepare the selected accessories", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(UnprepareAccessory, "Unprepare", "Unprepare the selected accessories", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(WearAcceessory, "Wear", "Wear the selected accessories", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(RemoveAccessory, "Remove", "Remove the selected accessories", EUserInterfaceActionType::ToggleButton, FInputChord{});
	UI_COMMAND(AccessoryProperties, "Accessory Properties", "Open accessory properties", EUserInterfaceActionType::ToggleButton, FInputChord{});

	UI_COMMAND(LoadPipelineTools, "Assembly", "Assembly, Creation of Runtime-Ready assets for UE, UEFN, and DCCs", EUserInterfaceActionType::RadioButton, FInputChord{});
	UI_COMMAND(BeginPipelineTool, "Edit Assembly", "Assembly Tool", EUserInterfaceActionType::ToggleButton, FInputChord{});
}

void FMetaHumanCharacterEditorToolCommands::GetToolDefaultObjectList(TArray<UInteractiveTool*>& OutToolCDOs)
{
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorPresetsTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorBodyConformTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorBodyModelTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorBodyBlendTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorConformTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorHeadModelTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorFaceMoveTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorFaceSculptTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorFaceBlendTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorSkinTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorEyesTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorMakeupTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorWardrobeTool>());
	OutToolCDOs.Add(GetMutableDefault<UMetaHumanCharacterEditorCostumeTool>());
}

#undef LOCTEXT_NAMESPACE
