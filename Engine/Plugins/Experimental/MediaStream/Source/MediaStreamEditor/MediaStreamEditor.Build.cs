// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MediaStreamEditor : ModuleRules
{
	public MediaStreamEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"DesktopWidgets",
				"LevelSequence",
				"LevelSequenceEditor",
				"MediaAssets",
				"MediaCompositing",
				"MediaPlayerEditor",
				"MediaStream",
				"MediaUtils",
				"MovieScene",
				"Projects",
				"PropertyEditor",
				"Sequencer",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
			}
		);
	}
}
