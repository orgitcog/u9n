// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SkeletonTemplateFrameworkRuntime : ModuleRules
{
	public SkeletonTemplateFrameworkRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "STFRun";

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HierarchyTableRuntime",
			}
		);
	}
}
