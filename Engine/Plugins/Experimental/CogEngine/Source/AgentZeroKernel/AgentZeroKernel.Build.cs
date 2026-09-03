// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AgentZeroKernel : ModuleRules
{
	public AgentZeroKernel(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Sockets",
				"Networking"
			}
			);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
