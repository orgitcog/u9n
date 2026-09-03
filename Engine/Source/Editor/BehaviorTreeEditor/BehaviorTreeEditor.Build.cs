// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BehaviorTreeEditor : ModuleRules
{
	public BehaviorTreeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AIGraph",
				"AIModule",
				"ApplicationCore",
				"AssetDefinition",
				"Core",
				"CoreUObject",
				"EditorWidgets",
				"Engine",
				"GraphEditor",
				"GameplayTags",
				"InputCore",
				"KismetWidgets",
				"PropertyEditor",
				"RewindDebuggerInterface",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"TraceServices",
				"UnrealEd",
			}
		);
	}
}