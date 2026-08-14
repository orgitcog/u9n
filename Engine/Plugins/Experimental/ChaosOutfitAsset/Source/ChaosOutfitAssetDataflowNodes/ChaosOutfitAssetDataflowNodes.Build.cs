// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosOutfitAssetDataflowNodes : ModuleRules
{
	public ChaosOutfitAssetDataflowNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"Core",
				"CoreUObject",
				"Chaos",
				"ChaosClothAsset",
				"ChaosClothAssetEngine",
				"ChaosOutfitAssetEngine",
				"DataflowCore",
				"DataflowNodes",
				"Engine",
				"MeshResizingCore",
			}
		);
	}
}
