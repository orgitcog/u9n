// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CaptureManagerPipeline : ModuleRules
{
	public CaptureManagerPipeline(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"CaptureManagerTakeMetadata"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ControlFlows",
			"Core",
			"CoreUObject",
			"ImageWrapper",
			"CaptureUtils",
			"Projects"
		});
	}
}
