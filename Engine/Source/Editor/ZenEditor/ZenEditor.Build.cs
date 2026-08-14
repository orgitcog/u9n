// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ZenEditor : ModuleRules
{
	public ZenEditor(ReadOnlyTargetRules Target)
		 : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"DerivedDataCache",
				"DerivedDataWidgets",
				"EditorFramework",
				"EditorSubsystem",
				"Engine",
				"InputCore",
				"MessageLog",
				"OutputLog",
				"Slate",
				"SlateCore",
				"StorageServerWidgets",
				"ToolMenus",
				"ToolWidgets",
				"UATHelper",
				"UnrealEd",
				"WorkspaceMenuStructure",
				"Zen"
			});

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
			});


		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			});
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
