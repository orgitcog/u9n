// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorldBookmark : ModuleRules
{
	public WorldBookmark(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"AssetDefinition",
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser",
				"ContentBrowserData",
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"EditorSubsystem",
				"EditorConfig",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"LevelEditor",
				"Projects",
				"PropertyEditor",
				"SceneOutliner",
				"SourceControl",
				"Slate",
				"SlateCore",				
				"ToolMenus",
				"ToolWidgets",
				"UncontrolledChangelists",
				"UnrealEd",
				"WorkspaceMenuStructure"
			}
		);
	}
}
