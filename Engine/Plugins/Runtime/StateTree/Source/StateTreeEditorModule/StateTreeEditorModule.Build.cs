// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StateTreeEditorModule : ModuleRules
	{
		public StateTreeEditorModule(ReadOnlyTargetRules Target) : base(Target)
		{
			CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;

			PublicIncludePaths.AddRange(
			new string[] {
			}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"AssetTools",
				"EditorFramework",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"PropertyEditor",
				"StateTreeModule",
				"SourceControl",
				"Projects",
				"BlueprintGraph",
				"PropertyBindingUtils",
				"PropertyBindingUtilsEditor",
				"PropertyAccessEditor",
				"StructUtilsEditor",
				"GameplayTags",
				"EditorSubsystem"
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetDefinition",
				"RenderCore",
				"GraphEditor",
				"KismetWidgets",
				"PropertyPath",
				"SourceCodeAccess",
				"ToolMenus",
				"ToolWidgets",
				"TraceLog",
				"TraceServices",
				"ApplicationCore",
				"DeveloperSettings",
				"GameplayInsights",
				"RewindDebuggerInterface",
				"RewindDebuggerRuntimeInterface",
				"DetailCustomizations",
				"AppFramework",
				"Kismet",
				"KismetCompiler",
				"EditorInteractiveToolsFramework",
				"EditorWidgets",
				"InteractiveToolsFramework",
			}
			);

			PrivateIncludePathModuleNames.AddRange(new string[] {
				"MessageLog",
			});

			PublicDefinitions.Add("WITH_STATETREE_TRACE=1");
			PublicDefinitions.Add("WITH_STATETREE_TRACE_DEBUGGER=1");
		}
	}
}
