// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MovieSceneAnimMixer : ModuleRules
	{
		public MovieSceneAnimMixer(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"MovieScene",
					"MovieSceneTracks",
					"AnimGraphRuntime",
					"AnimNext",
					"AnimNextAnimGraph"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"MovieSceneTracks",
				}
			);

			if (Target.bBuildWithEditorOnlyData && Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(new string[]
					{
						"EditorFramework",
						"UnrealEd"
					});
			}
		}
	}
}