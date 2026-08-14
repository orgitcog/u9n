// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UbaCommonTarget : TargetRules
{
	public UbaCommonTarget(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "UbaCommon";
		bShouldCompileAsDLL = true;
		UbaAgentTarget.CommonUbaSettings(this, Target);
	}
}
