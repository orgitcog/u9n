// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextPoseSearch : ModuleRules
	{
		public AnimNextPoseSearch(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimNext",
					"AnimNextAnimGraph",
					"Core",
					"CoreUObject",
					"Engine",
					"HierarchyTableRuntime",
					"HierarchyTableAnimationRuntime",
					"PoseSearch",
					"Chooser",
					"RigVM",
				}
			);
		}
	}
}