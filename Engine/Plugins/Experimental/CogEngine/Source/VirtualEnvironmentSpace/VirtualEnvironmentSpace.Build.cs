// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualEnvironmentSpace : ModuleRules
{
	public VirtualEnvironmentSpace(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",
			"NavigationSystem",
			"OpenCogCore",
			"AgentZeroKernel",
			"KoboldAIStoryweaver",
			"DeepTreeEcho",
			"AphroditeEngine",
			"MardukLab"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore"
		});
	}
}
