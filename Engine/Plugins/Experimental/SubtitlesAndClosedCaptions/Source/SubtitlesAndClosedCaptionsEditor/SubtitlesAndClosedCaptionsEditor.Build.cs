// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SubtitlesAndClosedCaptionsEditor : ModuleRules
	{
		public SubtitlesAndClosedCaptionsEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PrivateDependencyModuleNames.AddRange
			(
				new string[]
				{
					"AssetTools",
					"LevelSequence",
					"MovieScene",
					"Sequencer",
					"SequencerCore",
					"Slate",
					"SlateCore",
					"SubtitlesAndClosedCaptions",
				}
			);

			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"AssetDefinition",
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealEd"
				}
			);
		}
	}
}
