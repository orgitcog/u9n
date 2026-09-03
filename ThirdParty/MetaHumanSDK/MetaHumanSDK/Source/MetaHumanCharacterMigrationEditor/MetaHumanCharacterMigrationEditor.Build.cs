// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanCharacterMigrationEditor : ModuleRules
{
	public MetaHumanCharacterMigrationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{

		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
            "Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"SlateCore",
			"Slate",
			"ToolWidgets",
			"Json",
			"JsonUtilities",
			"AssetTools",
			"HairStrandsCore",
			"RigLogicModule",
			"MetaHumanSDKRuntime",
			"MetaHumanSDKEditor",
			"MetaHumanCharacter",
			"MetaHumanCharacterEditor",
			"MetaHumanCharacterPalette",
			"MetaHumanCharacterPaletteEditor",
			"MetaHumanDefaultPipeline"
		});
	}
}
