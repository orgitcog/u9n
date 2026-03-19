// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenCogCore : ModuleRules
{
	public OpenCogCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AIModule",
				"NavigationSystem"
			}
			);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
