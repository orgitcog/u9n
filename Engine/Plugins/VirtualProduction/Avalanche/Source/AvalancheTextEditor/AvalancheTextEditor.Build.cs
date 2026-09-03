// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class AvalancheTextEditor : ModuleRules
{
	public AvalancheTextEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"AvalancheShapesEditor",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Avalanche",
				"AvalancheComponentVisualizers",
				"AvalancheEditorCore",
				"AvalancheInteractiveTools",
				"AvalancheLevelViewport",
				"AvalancheText",
				"ComponentVisualizers",
				"Core",
				"CoreUObject",
				"DynamicMaterialEditor",
				"Engine",
				"EditorScriptingUtilities",
				"InteractiveToolsFramework",
				"InputCore",
				"Projects",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"Text3D",
				"UnrealEd",
			}
		);
	}
}
