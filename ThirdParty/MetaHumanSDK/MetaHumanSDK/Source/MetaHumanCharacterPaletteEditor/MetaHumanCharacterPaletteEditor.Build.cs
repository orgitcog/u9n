// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanCharacterPaletteEditor : ModuleRules
{
	public MetaHumanCharacterPaletteEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{

		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"UnrealEd",
			"Engine",
			"SlateCore",
			"Slate",
			"InputCore",
			"Projects",
			"AssetDefinition",
			"AdvancedPreviewScene",
			"EditorFramework",
			"InteractiveToolsFramework",
			"EditorInteractiveToolsFramework",
			"ToolMenus",
			"PropertyEditor",
			"ContentBrowser",
			"MessageLog",

			"MetaHumanCharacter",
			"MetaHumanCharacterPalette",
			"HairStrandsCore",
		});
	}
}