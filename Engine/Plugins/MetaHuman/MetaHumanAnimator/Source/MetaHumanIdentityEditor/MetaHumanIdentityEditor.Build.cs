// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanIdentityEditor : ModuleRules
{
	protected bool BuildForDevelopment
	{
		get
		{
			// Check if source is available
			string SourceFilesPath = Path.Combine(ModuleDirectory, "Private");
			return Directory.Exists(SourceFilesPath) &&
				   Directory.GetFiles(SourceFilesPath).Length > 0;
		}
	}

	public MetaHumanIdentityEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Projects",
			"Sequencer",
			"MediaAssets",
			"ImgMedia",
			"DesktopPlatform",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"ToolWidgets",
			"MovieScene",
			"GeometryFramework",
			"GeometryCore",
			"MeshConversion",
			"AdvancedPreviewScene",
			"InputCore",
			"ControlRigDeveloper",
			"RigVMDeveloper",
			"RigLogicModule",
			"AnimGraph",
			"ControlRig",
			"AssetDefinition",
			"SkeletalMeshDescription",
			"NNE",
			"ImageCore",
			"MetaHumanCore",
			"MetaHumanCoreEditor",
			"MetaHumanFaceContourTracker",
			"MetaHumanFaceFittingSolver",
			"MetaHumanImageViewerEditor",
			"MetaHumanCaptureData",
			"MetaHumanCaptureDataEditor",
			"MetaHumanPipeline",
			"MetaHumanSequencer",
			"MetaHumanIdentity",
			"MetaHumanToolkit",
			"MetaHumanPlatform",
			"CaptureDataCore",
			"MetaHumanCaptureUtils",
			"CaptureDataUtils",
			"MetaHumanCoreTechLib",
			"MetaHumanCoreTech"
		});
	}
}