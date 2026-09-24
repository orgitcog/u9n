// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChaosInsightsUI : ModuleRules
	{
		public ChaosInsightsUI(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"SlateCore",
				"Slate",
				"InputCore",
				"TraceInsights",
				"TraceInsightsCore",
				"TraceServices",
				"ChaosInsightsAnalysis",
			});
		}
	}
}

