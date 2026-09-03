// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ComponentVisualizers : ModuleRules
{
	public ComponentVisualizers(ReadOnlyTargetRules Target) : base(Target)
	{
		// Enable truncation warnings in this module
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"Engine",
				"Slate",
                "SlateCore",
				"EditorFramework",
				"UnrealEd",
				"LevelEditor",
                "PropertyEditor",
				"AIModule",
				"ViewportInteraction"
			}
		);
	}
}
