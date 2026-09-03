// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class HairStrandsDataflow: ModuleRules
	{
		public HairStrandsDataflow(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"ChaosCore",
					"Chaos",
					"DataflowCore",
					"DataflowEngine",
					"DataflowEditor",
					"DataflowNodes",
					"DataflowEnginePlugin",
					"HairStrandsCore",
					"Engine",
					"GeometryAlgorithms",
					"GeometryCore",
					"MeshConversion",
					"MeshConversionEngineTypes",
					"MeshDescription",
					"DynamicMesh",
					"SkeletalMeshDescription",
					"StaticMeshDescription",
					"Slate",
					"SlateCore"
				}
			);
		}
	}
}