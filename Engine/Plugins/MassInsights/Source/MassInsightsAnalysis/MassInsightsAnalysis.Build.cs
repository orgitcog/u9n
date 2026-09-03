// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MassInsightsAnalysis : ModuleRules
	{
		public MassInsightsAnalysis(ReadOnlyTargetRules Target) : base(Target)
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

