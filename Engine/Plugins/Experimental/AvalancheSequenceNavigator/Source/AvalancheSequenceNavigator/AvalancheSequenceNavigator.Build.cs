// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheSequenceNavigator : ModuleRules
{
	public AvalancheSequenceNavigator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Avalanche",
			"AvalancheEditorCore",
			"AvalancheSequence",
			"AvalancheSequencer",
			"CoreUObject",
			"Engine",
			"MovieScene",
			"Sequencer",
			"SequenceNavigator",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd"
		});
	}
}
