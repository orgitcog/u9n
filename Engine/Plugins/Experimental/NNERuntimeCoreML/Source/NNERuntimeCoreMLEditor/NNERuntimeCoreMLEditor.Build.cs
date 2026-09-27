// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNERuntimeCoreMLEditor : ModuleRules
{
	public NNERuntimeCoreMLEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange
		(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MainFrame",
				"NNE",
				"NNERuntimeCoreML",
				"Slate",
				"SlateCore",
				"UnrealEd"
			}
		);
	}
}
