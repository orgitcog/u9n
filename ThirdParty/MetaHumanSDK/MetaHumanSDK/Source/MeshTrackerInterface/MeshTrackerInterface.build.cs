// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MeshTrackerInterface : ModuleRules
{
	public MeshTrackerInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);	
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"CoreUObject",
				"Projects",
			}
		);
	}
}