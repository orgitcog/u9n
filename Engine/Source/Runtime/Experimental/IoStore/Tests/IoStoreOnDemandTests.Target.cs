// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class IoStoreOnDemandTestsTarget : TestTargetRules
{
	public IoStoreOnDemandTestsTarget(TargetInfo Target) : base(Target)
	{
		bCompileAgainstCoreUObject			= true;
		bCompileAgainstApplicationCore		= true;
		bAdaptiveUnityDisablesOptimizations = true;

		GlobalDefinitions.Add("CATCH_CONFIG_ENABLE_BENCHMARKING=1");
		GlobalDefinitions.Add("WITH_IOSTORE_ONDEMAND_TESTS=1");
	}
}
