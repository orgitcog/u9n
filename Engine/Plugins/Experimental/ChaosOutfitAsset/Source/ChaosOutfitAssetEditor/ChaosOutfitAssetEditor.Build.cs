// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosOutfitAssetEditor : ModuleRules
{
	public ChaosOutfitAssetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetDefinition",
				"BaseCharacterFXEditor",
				"ChaosClothAsset",
				"ChaosClothAssetEngine",
				"ChaosClothAssetEditor",
				"ChaosOutfitAssetEngine",
				"ContentBrowser",
				"Core",
				"CoreUObject",
				"DataflowEditor",
				"DataflowEngine",
				"Engine",
				"InputCore",
				"InteractiveToolsFramework",
				"Projects",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd"
			}
		);
	}
}
