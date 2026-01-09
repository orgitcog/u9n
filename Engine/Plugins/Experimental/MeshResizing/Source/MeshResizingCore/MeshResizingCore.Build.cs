// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MeshResizingCore : ModuleRules
	{
        public MeshResizingCore(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Chaos",
					"Core",
					"CoreUObject",
					"GeometryCore",
					"HairStrandsCore",
					"MeshDescription"
				}
			);
		}
	}
}
