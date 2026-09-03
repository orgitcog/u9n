// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosOutfitAssetEngine : ModuleRules
{
	public ChaosOutfitAssetEngine(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ChaosClothAssetEngine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Chaos",
				"ChaosClothAsset",
				"DataflowEngine",
				"Engine",
				"MeshResizingCore",
				"RenderCore",
				"RHI",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"DerivedDataCache",
			}
		);
	}
}
