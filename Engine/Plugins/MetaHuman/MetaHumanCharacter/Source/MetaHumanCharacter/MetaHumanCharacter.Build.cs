// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanCharacter : ModuleRules
{
	public MetaHumanCharacter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ImageCore",
			"MetaHumanCharacterPalette",
			"MetaHumanSDKRuntime",
			"Projects",
			"SlateCore"
		});
	}
}