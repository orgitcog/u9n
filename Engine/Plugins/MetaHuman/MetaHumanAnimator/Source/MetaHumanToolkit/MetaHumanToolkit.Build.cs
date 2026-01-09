// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanToolkit : ModuleRules
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

	public MetaHumanToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"AdvancedPreviewScene",
			"MovieScene",
			"Sequencer",
			"ImgMedia",
			"MovieSceneTracks",

			"MetaHumanCaptureData",
			"MetaHumanImageViewerEditor",
			"MetaHumanSequencer",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Engine",
			"InputCore",
			"ToolMenus",
			"PropertyEditor",
			"Projects",
			"MediaAssets",
			"ProceduralMeshComponent",
			"RenderCore",
			"MetaHumanCore",
			"CaptureDataCore"
		});
	}
}
