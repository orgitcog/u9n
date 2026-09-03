// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PCGEditor : ModuleRules
	{
		public PCGEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Projects",
					"Engine",
					"CoreUObject",
					"PlacementMode",
				});

			if (Target.WithAutomationTests)
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"LevelEditor"
					});
			}

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AdvancedPreviewScene",
					"AppFramework",
					"ApplicationCore",
					"AssetDefinition",
					"AssetTools",
					"AssetRegistry",
					"BlueprintGraph",
					"ContentBrowser",
					"ContentBrowserData",
					"DesktopWidgets",
					"DetailCustomizations",
					"DeveloperSettings",
					"EditorFramework",
					"EditorScriptingUtilities",
					"EditorStyle",
					"EditorSubsystem",
					"EditorWidgets",
					"GameProjectGeneration",
					"GraphEditor",
					"InputCore",
					"Kismet",
					"KismetWidgets",
					"PCG",
					"PropertyEditor",
					"RHI",
					"RenderCore",
					"Slate",
					"SlateCore",
					"SourceControl",
					"StructUtilsEditor",
					"ToolMenus",
					"ToolWidgets",
					"TypedElementFramework",
					"TypedElementRuntime",
					"UnrealEd",
					"LevelEditor",
					"SceneOutliner"
				});
		}
	}
}
