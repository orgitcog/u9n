// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CSVToTelemetry : ModuleRules
{
	public CSVToTelemetry(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AnalyticsET",
			"ApplicationCore",
			"AssetRegistry",
			"BuildSettings",
			"Core",
			"CoreUObject",
			"CSVUtils",
			"Horde",
			"HTTP",
			"Projects",
			"RSA",
			"StudioTelemetry",
			"TargetPlatform"
		});
	}
}
