// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioExperimentalRuntime : ModuleRules
{
	public AudioExperimentalRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);
		
		// When the Editor builds all plugins, it will build with engine. In that case add a dependency on CoreUObject.
		// If we don't do this we will get linker errors for the Engine expecting to find stuff defined here.
		if (Target.bCompileAgainstEngine)
		{
			PublicDependencyModuleNames.Add("CoreUObject");
		}
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"SignalProcessing",
			}
		);
	}
}