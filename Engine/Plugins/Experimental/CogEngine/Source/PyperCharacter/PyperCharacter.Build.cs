// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PyperCharacter : ModuleRules
{
	public PyperCharacter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeepTreeEcho",
				"Live2DCubismCore"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
