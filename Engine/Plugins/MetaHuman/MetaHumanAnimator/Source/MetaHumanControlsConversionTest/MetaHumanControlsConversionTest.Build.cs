// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanControlsConversionTest : ModuleRules
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

	public MetaHumanControlsConversionTest(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] 
		{
			// ... add public include paths required here ...
		});


		PrivateIncludePaths.AddRange(new string[] 
		{
		});

		PublicDependencyModuleNames.AddRange(new string[]
		{
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"MetaHumanCore",
			"MetaHumanCoreTech",
			"MetaHumanConfig",
			"RigLogicModule",
			"Json",
			"DNAInterchange",
			"Projects",
			"MeshTrackerInterface",
		});
	}
}
