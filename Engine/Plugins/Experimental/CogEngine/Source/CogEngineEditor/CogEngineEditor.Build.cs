// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CogEngineEditor : ModuleRules
{
	public CogEngineEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"EditorFramework",
				"CogEngine"
			}
			);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"InputCore",
				"PropertyEditor"
			}
			);
	}
}
