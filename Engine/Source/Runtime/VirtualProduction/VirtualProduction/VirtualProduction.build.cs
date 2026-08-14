// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualProduction  : ModuleRules
{
	public VirtualProduction(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine"
			});
	}
}
