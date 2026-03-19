// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanDefaultEditorPipeline : ModuleRules
{
	public MetaHumanDefaultEditorPipeline(ReadOnlyTargetRules Target) : base(Target)
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
			"MetaHumanCharacterEditor",
			"MetaHumanCharacterPalette",
			"MetaHumanCharacterPaletteEditor",
			"MetaHumanDefaultPipeline",
			"MetaHumanSDKRuntime",
			"TextureGraph",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"MetaHumanCoreTechLib",
			"UnrealEd",
			"Kismet",
			"SubobjectDataInterface",
			"DataflowEngine",
			"MetaHumanSDKEditor",
			"PerformanceCaptureCore",
			"IKRig",
			"FileUtilities",
			"RigLogicModule",
			"Projects",
			"RenderCore",
			"AssetTools",
			"MaterialEditor",
			"Json",
			"PluginUtils",
			"RigLogicModule",
			"ControlRigDeveloper",
			"EditorScriptingUtilities",
			"Blutility",
			"GeometryScriptingCore",
			"SkeletalMeshUtilitiesCommon",
		});
	}
}
