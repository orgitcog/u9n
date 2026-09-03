// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class Reflex : ModuleRules
	{
		public Reflex(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange([
				"Core",
				"Engine",
				"RHI",
				"CoreUObject",
				"SlateCore",
				"Slate"
			]);

			// Grab NVAPI
			AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");

			// Grab ReflexStat
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty"));
		}
	}
}
