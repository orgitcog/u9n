// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelSequenceNavigatorBridge : ModuleRules
{
	public LevelSequenceNavigatorBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {

		});

		PrivateIncludePaths.AddRange(new string[] {

		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject",
			"Engine",
			"LevelSequence",
			"MovieScene",
			"Sequencer",
			"SequenceNavigator",
			"Slate",
			"SlateCore"
		});

		DynamicallyLoadedModuleNames.AddRange(new string[] {

		});
	}
}
