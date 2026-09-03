// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CineAssemblyToolsEditor : ModuleRules
	{
		public CineAssemblyToolsEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetDefinition",
					"AssetRegistry",
					"AssetTools",
					"CineAssemblyTools",
					"ClassViewer",
					"ContentBrowser",
					"ContentBrowserData",
					"Core",
					"CoreUObject",
					"DesktopPlatform",
					"DeveloperSettings",
					"DirectoryPlaceholder",
					"EditorWidgets",
					"Engine",
					"InputCore",
					"Json",
					"JsonUtilities",
					"LevelSequence",
					"LevelSequenceEditor",
					"MovieRenderPipelineCore",
					"MovieScene",
					"MovieSceneTools",
					"NamingTokens",
					"Projects",
					"PropertyEditor",
					"SharedSettingsWidgets",
					"Slate",
					"SlateCore",
					"SourceControl",
					"TimeManagement",
					"TakeRecorder",
					"TakesCore",
					"ToolMenus",
					"UnrealEd",
					"WorkspaceMenuStructure", 
				}
			);
		}
	}
}
