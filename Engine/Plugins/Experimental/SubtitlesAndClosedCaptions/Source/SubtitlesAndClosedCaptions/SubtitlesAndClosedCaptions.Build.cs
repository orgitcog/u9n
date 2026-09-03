// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SubtitlesAndClosedCaptions : ModuleRules
	{
		public SubtitlesAndClosedCaptions(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PrivateDependencyModuleNames.AddRange
			(
				new string[]
				{
					"Slate",
					"SlateCore",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DeveloperSettings",
					"Engine",
					"InputCore",
					"MovieSceneTracks",
					"MovieScene",
					"UMG"
				}
			);
		}
	}
}
