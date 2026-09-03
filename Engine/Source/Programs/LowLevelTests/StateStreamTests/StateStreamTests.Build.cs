// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class StateStreamTests : TestModuleRules
{
	public StateStreamTests(ReadOnlyTargetRules Target) : base(Target, true)
	{
		TestMetadata = new Metadata()
		{
			TestName = "StateStream",
			TestShortName = "StateStream",
			ReportType = "xml",
			SupportedPlatforms = {
				UnrealTargetPlatform.Win64,
				UnrealTargetPlatform.Linux,
				UnrealTargetPlatform.Mac,
				UnrealTargetPlatform.Android,
				UnrealTargetPlatform.IOS }
		};

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"StateStream",
			});
	}
}
