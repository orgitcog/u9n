// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MardukLab : ModuleRules
{
	public MardukLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
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
			}
			);
	}
}
