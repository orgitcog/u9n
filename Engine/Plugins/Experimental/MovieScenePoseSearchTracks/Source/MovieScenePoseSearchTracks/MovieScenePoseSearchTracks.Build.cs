// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MovieScenePoseSearchTracks : ModuleRules
	{
		public MovieScenePoseSearchTracks(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"MovieScene",
					"MovieSceneTracks",
					"MovieSceneAnimMixer",
					"AnimNext",
					"AnimNextPoseSearch",
					"AnimNextAnimGraph",
					"AnimGraphRuntime",
					"PoseSearch"
				}
			);
		}
	}
}