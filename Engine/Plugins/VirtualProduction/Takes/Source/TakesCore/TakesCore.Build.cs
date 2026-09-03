// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TakesCore : ModuleRules
{
	public TakesCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorFramework",
				"UnrealEd",
				"NamingTokens",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"LevelSequence",
				"SlateCore",
				"MovieScene",
				"MovieSceneTracks",
				"MovieSceneTools",
				"LevelSequence",
				"LevelSequenceEditor",
				"Engine",
				"SerializedRecorderInterface",

			}
		);
		
		PublicIncludePathModuleNames.AddRange(
			new string[]
			{
				"TakeRecorderNamingTokens"
			}
		);
	}
}
