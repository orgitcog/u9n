// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InterchangeImport : ModuleRules
	{
		public InterchangeImport(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"ClothingSystemRuntimeCommon",
					"Core",
					"CoreUObject",
					"Engine",
					"InterchangeCore",
					"InterchangeCommon",
					"InterchangeDispatcher",
					"InterchangeEngine",
					"InterchangeFactoryNodes",
					"InterchangeNodes",
					"LevelSequence",
					"MeshDescription",
					"MovieScene",
					"MovieSceneTracks",
					"StaticMeshDescription",
					"SkeletalMeshDescription",
				}
			);
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"CinematicCamera",
					"ClothingSystemRuntimeCommon",
					"GeometryCache",
					"GLTFCore",
					"IESFile",
					"ImageCore",
					"ImageWrapper",
					"InterchangeCommonParser",
					"InterchangeMessages",
					"Json",
					"RenderCore",
					"RHI",
					"TextureUtilitiesCommon",
					"VariantManagerContent",
				}
			);

			if (Target.bBuildEditor)
			{
				bEnableExceptions = true;
				bDisableAutoRTFMInstrumentation = true;

				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"BSPUtils",
						"InterchangeFbxParser",
						"MaterialEditor",
						"MaterialX",
						"SkeletalMeshUtilitiesCommon",
						"UnrealEd",
						"VariantManager",
					}
				);
			}
		}
	}
}
