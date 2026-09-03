// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataflowNodes : ModuleRules
	{
		public DataflowNodes(ReadOnlyTargetRules Target) : base(Target)
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
					"DataflowEnginePlugin",
					"DataflowAssetTools",
					"Engine",
					"MeshConversion",
					"MeshDescription",
					"SkeletalMeshDescription",
					"GeometryCore"
				}
			);

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"SkeletalMeshUtilitiesCommon"
					}
				);
			}
		}
	}
}
