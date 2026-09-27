// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CogEngine : ModuleRules
{
	public CogEngine(ReadOnlyTargetRules Target) : base(Target)
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
				"Sockets",
				"Networking",
				"HTTP",
				"Json",
				"JsonUtilities"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"OpenCogCore",
				"AgentZeroKernel",
				"KoboldAIStoryweaver",
				"DeepTreeEcho",
				"AphroditeEngine",
				"MardukLab"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
