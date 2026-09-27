// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MovieSceneAnimMixerEditor : ModuleRules
	{
		public MovieSceneAnimMixerEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimationCore",
					"AnimGraph",
					"AnimGraphRuntime", 
					"Core",
					"CoreUObject",
				}
			);
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"BlueprintGraph",
					"Engine",
					"MovieScene",
					"MovieSceneAnimMixer",
					"MovieSceneTracks",
					"MovieSceneTools",
					"Sequencer",
					"SequencerCore",
					"Slate",
					"SlateCore",
					"UnrealEd",
				}
			);
		}
	}
}