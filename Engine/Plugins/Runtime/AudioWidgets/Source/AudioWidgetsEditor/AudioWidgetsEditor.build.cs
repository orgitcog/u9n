// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioWidgetsEditor : ModuleRules
{
	public AudioWidgetsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange
		(
			new string[]
			{
					"Core",
					"CoreUObject",
					"Engine",
					"AudioExtensions",
					"PropertyEditor"
			}
		);

	}
}