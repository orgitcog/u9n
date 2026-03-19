// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChaosInsightsAnalysis : ModuleRules
	{
		public ChaosInsightsAnalysis(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(new string[] { "Core" });
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"TraceLog",
					"TraceAnalysis",
					"TraceServices",
				});
		}
	}
}

