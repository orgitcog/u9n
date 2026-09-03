// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RuntimeTests : ModuleRules
{
	public RuntimeTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"MassEntity",
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"AsyncMessageSystem",
				"GameplayTags",
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("ScreenShotComparisonTools");
		}
	}
}
