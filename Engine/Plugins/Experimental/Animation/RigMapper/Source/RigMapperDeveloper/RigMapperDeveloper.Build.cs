// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RigMapperDeveloper : ModuleRules
{
	public RigMapperDeveloper(ReadOnlyTargetRules Target) : base(Target)
	{
			// Copying some of these from RigMapper.build.cs, our deps are likely leaner
			// and therefore these could be pruned if needed:
  
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ControlRig",
				"AnimationCore",
				"Json",
				"RigMapper"
			});
	}
}
