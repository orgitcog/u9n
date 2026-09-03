// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SmartObjectsEditorModule : ModuleRules
	{
		public SmartObjectsEditorModule(ReadOnlyTargetRules Target) : base(Target)
		{
			CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;

			PublicIncludePaths.AddRange(
			new string[] {
			}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"AdvancedPreviewScene",
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"SmartObjectsModule",
				"SourceControl",
				"UnrealEd",
				"WorldConditions",
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"AssetDefinition",
				"BlueprintGraph",
				"ComponentVisualizers",
				"InputCore",
				"PropertyAccessEditor",
				"PropertyBindingUtils",
				"PropertyBindingUtilsEditor",
				"PropertyEditor",
				"RenderCore",
				"Slate",
				"SlateCore",
				"StructUtilsEditor",
				"ToolWidgets",
				"ToolMenus"
			}
			);
		}

	}
}
