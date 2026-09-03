// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanDepthGenerator : ModuleRules
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

	public MetaHumanDepthGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
		{
            "Core",
			"CoreUObject",
			"CaptureDataCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"SlateCore",
			"Slate",
			"MetaHumanCore",
			"MetaHumanPipelineCore",
			"MetaHumanPipeline",
			"MetaHumanCaptureSource",
			"CameraCalibrationCore",
			"ImgMedia",
			"CaptureDataEditor",
			"CaptureDataUtils",
			"MetaHumanCaptureData",
			"MeshTrackerInterface"
		});

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"ToolMenus",
				"ToolWidgets",
				"ContentBrowser",
				"ContentBrowserData",
				"UnrealEd"
			});
		}
	}
}
