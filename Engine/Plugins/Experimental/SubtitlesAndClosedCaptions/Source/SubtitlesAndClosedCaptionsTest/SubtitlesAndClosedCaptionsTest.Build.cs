// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class SubtitlesAndClosedCaptionsTest : ModuleRules
	{
		public SubtitlesAndClosedCaptionsTest(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PrivateDependencyModuleNames.AddRange
			(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"MovieScene"
				}
			);

			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"SubtitlesAndClosedCaptions",
				}
			);
		}
	}
}
