// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SpatialReadinessTests : TestModuleRules
{
	static SpatialReadinessTests()
	{
		TestMetadata = new Metadata()
		{
			TestName = "SpatialReadinessTests",
			TestShortName = "SpatialReadinessTests",
		};
	}

	public SpatialReadinessTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"Core",
			"SpatialReadiness",
			"Chaos",
		});
	}
}