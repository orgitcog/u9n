// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ElectraProtron : ModuleRules
	{
		public ElectraProtron(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"ElectraProtronFactory",
					"Core",
					"Engine",
					"MediaUtils",
					"RenderCore",
					"RHI",
					"ElectraBase",
                    "ElectraDecoders",
                    "ElectraPlayerPlugin",      // for early start and init of what we want to access
                    "ElectraPlayerRuntime",     // sharing the resource manager
					"ElectraSamples",           // and renderer
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				PrivateDependencyModuleNames.Add("D3D12RHI");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			}
		}
	}
}
