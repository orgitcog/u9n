// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class HairStrandsSolver : ModuleRules
	{
		public HairStrandsSolver(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"HairStrandsCore",
					"DataflowCore",
					"DataflowEngine",
					"DataflowSimulation",
					"Engine",
					"RHI",
					"RenderCore",
					"OptimusCore",
					"ComputeFramework", 
					"ChaosCaching"
				}
			);
		}
	}
}