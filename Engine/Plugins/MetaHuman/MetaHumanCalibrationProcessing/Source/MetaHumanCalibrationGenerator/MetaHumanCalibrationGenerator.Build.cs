// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanCalibrationGenerator : ModuleRules
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

	public MetaHumanCalibrationGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		ShortName = "MHStereoCalibrator";

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"CaptureDataCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"MetaHumanCalibrationLib",
			"CaptureDataEditor",
			"CaptureDataUtils",
			"MetaHumanCaptureData",
			"SlateCore",
			"Slate",
			"ImageWrapper",
			"ImgMedia",
			"CaptureUtils",
			"MetaHumanImageViewer",
			"SequencerWidgets",
			"Projects",
			"InputCore",
			"OutputLog",
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"ToolMenus",
				"ToolWidgets",
				"ContentBrowser",
				"UnrealEd"
			});
		}
	}
}