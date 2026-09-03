// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanCharacterPalette : ModuleRules
{
	public MetaHumanCharacterPalette(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"MetaHumanSDKRuntime",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{

		});
	}
}
