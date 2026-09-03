// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SequenceNavigator : ModuleRules
{
	public SequenceNavigator(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(new string[]
		{
		});

		PrivateIncludePaths.AddRange(new string[]
		{
		});

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AppFramework",
			"BlueprintGraph",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"InputCore",
			"LevelSequence",
			"MovieScene",
			"MovieSceneTools",
			"MovieSceneTracks",
			"Projects",
			"Sequencer",
			"SequencerCore",
			"Slate",
			"SlateCore",
			"SourceControl",
			"ToolMenus",
			"ToolWidgets",
			"UnrealEd",
			"WorkspaceMenuStructure"
		});

		DynamicallyLoadedModuleNames.AddRange(new string[]
		{
		});
	}
}
