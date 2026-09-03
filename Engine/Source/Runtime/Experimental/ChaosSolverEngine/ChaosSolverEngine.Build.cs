// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChaosSolverEngine : ModuleRules
	{
        public ChaosSolverEngine(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "RHI",
					"DeveloperSettings", 
					"DataflowCore",
					"DataflowEngine",
					"DataflowSimulation",
					"ChaosVDRuntime" // If CVD is compiled out, this only provides access to its structs data types
				}
				);

			SetupModulePhysicsSupport(Target);
			SetupModuleChaosVisualDebuggerSupport(Target);
			PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");
		}
	}
}
