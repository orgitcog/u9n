// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Tools/InteractiveToolsCommands.h"

class FMetaHumanCharacterEditorCommands : public TCommands<FMetaHumanCharacterEditorCommands>
{
public:

	FMetaHumanCharacterEditorCommands();

	virtual void RegisterCommands() override;

public:

	TSharedPtr<FUICommandInfo> SaveThumbnail;
	TSharedPtr<FUICommandInfo> AutoRigFaceBlendShapes;
	TSharedPtr<FUICommandInfo> AutoRigFaceJointsOnly;
	TSharedPtr<FUICommandInfo> RemoveFaceRig;
	TSharedPtr<FUICommandInfo> DownloadHighResTextures2k;
	TSharedPtr<FUICommandInfo> DownloadHighResTextures4k;
	TSharedPtr<FUICommandInfo> DownloadHighResTextures8k;
	TSharedPtr<FUICommandInfo> RefreshPreview;
};

class FMetaHumanCharacterEditorDebugCommands : public TCommands<FMetaHumanCharacterEditorDebugCommands>
{
public:

	FMetaHumanCharacterEditorDebugCommands();

	virtual void RegisterCommands() override;

public:

	TSharedPtr<FUICommandInfo> ExportFaceSkelMesh;
	TSharedPtr<FUICommandInfo> ExportBodySkelMesh;
	TSharedPtr<FUICommandInfo> ExportCombinedSkelMesh;
	TSharedPtr<FUICommandInfo> SaveFaceState;
	TSharedPtr<FUICommandInfo> SaveFaceStateToDNA;
	TSharedPtr<FUICommandInfo> DumpFaceStateDataForAR;
	TSharedPtr<FUICommandInfo> SaveBodyState;
	TSharedPtr<FUICommandInfo> SaveFaceDNA;
	TSharedPtr<FUICommandInfo> SaveBodyDNA;
	TSharedPtr<FUICommandInfo> SaveFaceTextures;
	TSharedPtr<FUICommandInfo> SaveEyePreset;
	TSharedPtr<FUICommandInfo> TakeHighResScreenshot;
};

class FMetaHumanCharacterEditorToolCommands : public TInteractiveToolCommands<FMetaHumanCharacterEditorToolCommands>
{
public:

	FMetaHumanCharacterEditorToolCommands();

	virtual void RegisterCommands() override;

	virtual void GetToolDefaultObjectList(TArray<UInteractiveTool*>& OutToolCDOs) override;

public:

	TSharedPtr<FUICommandInfo> AcceptOrCompleteActiveTool;
	TSharedPtr<FUICommandInfo> CancelOrCompleteActiveTool;

	// Preset tools
	TSharedPtr<FUICommandInfo> LoadPresetsTools;
	TSharedPtr<FUICommandInfo> BeginPresetsTool;
	TSharedPtr<FUICommandInfo> PresetProperties;
	TSharedPtr<FUICommandInfo> ApplyPreset;

	// Body tools
	TSharedPtr<FUICommandInfo> LoadBodyTools;
	TSharedPtr<FUICommandInfo> BeginBodyConformTools;
	TSharedPtr<FUICommandInfo> BeginBodyConformImportBodyDNATool;
	TSharedPtr<FUICommandInfo> BeginBodyConformImportBodyTemplateTool;
	TSharedPtr<FUICommandInfo> BeginBodyModelTool;
	TSharedPtr<FUICommandInfo> BeginBodyModelParametricTool;
	TSharedPtr<FUICommandInfo> BeginBodyFixedCompatibilityTool;
	TSharedPtr<FUICommandInfo> BeginBodyBlendTool;

	// Head tools
	TSharedPtr<FUICommandInfo> LoadHeadTools;
	TSharedPtr<FUICommandInfo> BeginConformTools;
	TSharedPtr<FUICommandInfo> BeginConformImportDNATool;
	TSharedPtr<FUICommandInfo> BeginConformImportIdentityTool;
	TSharedPtr<FUICommandInfo> BeginConformImportTemplateTool;
	TSharedPtr<FUICommandInfo> BeginHeadModelTools;
	TSharedPtr<FUICommandInfo> BeginHeadModelEyelashesTool;
	TSharedPtr<FUICommandInfo> BeginHeadModelTeethTool;
	TSharedPtr<FUICommandInfo> BeginHeadMaterialsTools;
	TSharedPtr<FUICommandInfo> BeginHeadMaterialsTeethTool;
	TSharedPtr<FUICommandInfo> BeginHeadMaterialsEyelashesTool;
	TSharedPtr<FUICommandInfo> BeginFaceMoveTool;
	TSharedPtr<FUICommandInfo> BeginFaceSculptTool;
	TSharedPtr<FUICommandInfo> BeginFaceBlendTool;

	// Materials tools
	TSharedPtr<FUICommandInfo> LoadMaterialsTools;
	TSharedPtr<FUICommandInfo> BeginSkinTool;
	TSharedPtr<FUICommandInfo> BeginEyesTool;
	TSharedPtr<FUICommandInfo> BeginMakeupTool;

	// Hair & Clothing tools
	TSharedPtr<FUICommandInfo> LoadHairAndClothingTools;
	TSharedPtr<FUICommandInfo> BeginWardrobeSelectionTool;
	TSharedPtr<FUICommandInfo> BeginCostumeDetailsTool;
	TSharedPtr<FUICommandInfo> PrepareAccessory;
	TSharedPtr<FUICommandInfo> UnprepareAccessory;
	TSharedPtr<FUICommandInfo> WearAcceessory;
	TSharedPtr<FUICommandInfo> RemoveAccessory;
	TSharedPtr<FUICommandInfo> AccessoryProperties;

	// Pipeline tools
	TSharedPtr<FUICommandInfo> LoadPipelineTools;
	TSharedPtr<FUICommandInfo> BeginPipelineTool;
};
