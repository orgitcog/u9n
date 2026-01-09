// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanDefaultPipeline : ModuleRules
{
	public MetaHumanDefaultPipeline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HairStrandsCore",
			"ChaosClothAssetEngine",
			"ChaosOutfitAssetEngine",
			"MetaHumanCharacter",
			"MetaHumanCharacterPalette",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{

		});
	}
}
