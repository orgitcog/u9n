// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CaptureUtils : ModuleRules
{
	public CaptureUtils(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Engine",
			"Networking",
			"Sockets"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}