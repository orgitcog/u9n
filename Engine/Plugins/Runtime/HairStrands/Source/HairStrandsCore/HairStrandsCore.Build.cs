// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class HairStrandsCore : ModuleRules
	{
		public HairStrandsCore(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"GeometryCache",
					"Projects",
					"MeshDescription",
					"MovieScene",
					"NiagaraCore",
					"NiagaraShader",
					"DataflowCore",
					"DataflowEngine",
					"RenderCore",
					"Renderer",
					"VectorVM",
					"RHI",
					"TraceLog",
					"StaticMeshDescription",
					"Eigen",
					"ChaosCore",
					"Chaos",
					"GeometryCore"
				});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Niagara",
					"ChaosCaching"
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"Shaders",
				});

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"DerivedDataCache",
					});

				PrivateIncludePathModuleNames.AddRange(
					new string[]
					{
						"DerivedDataCache",
					});
			}
		}
	}
}
