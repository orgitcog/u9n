// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class PSDImporterCore : ModuleRules
{
	public PSDImporterCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "PsdSDK", "Includes"));
		PublicSystemLibraryPaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "PsdSDK", "Libraries"));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"ImageCore",
				"ImageWrapper",
				"PsdSDK",
			}
		);
	}
}
