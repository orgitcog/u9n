// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;

public class HierarchyTableEditor : ModuleRules
{
	public HierarchyTableEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "HTEd";

		PrivateDependencyModuleNames.AddAll(
			"AssetDefinition",
			"Core",
			"CoreUObject",
			"Engine", 
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"HierarchyTableRuntime",
			"ToolMenus",
			"ToolWidgets"
		);
	}
}
