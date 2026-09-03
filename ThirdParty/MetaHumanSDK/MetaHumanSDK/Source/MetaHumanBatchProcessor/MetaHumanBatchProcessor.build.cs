// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanBatchProcessor : ModuleRules
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

	public MetaHumanBatchProcessor(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] 
		{
			// ... add public include paths required here ...
		});


		PrivateIncludePaths.AddRange(new string[] 
		{
			// ... add other private include paths required here ...
		});

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Engine",
			"MetaHumanCore",
			"MetaHumanPerformance",
			"MetaHumanPipelineCore",
			"MetaHumanPipeline",
			"MetaHumanCoreTech",
			"MetaHumanCoreTechLib",
			"MetaHumanSpeech2Face"
		});

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"SlateCore",
				"Slate",
				"ToolMenus",
				"ContentBrowser",
				"ContentBrowserData",
				"UnrealEd"
			});
		}
	}
}
