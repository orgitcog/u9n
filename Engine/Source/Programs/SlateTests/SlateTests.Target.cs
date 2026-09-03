// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class SlateTestsTarget : TestTargetRules
{
	public SlateTestsTarget(TargetInfo Target) : base(Target)
	{
		bTestsRequireApplicationCore = true;
		bTestsRequireCoreUObject = true;

		bBuildWithEditorOnlyData = Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop)
			&& (Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.Development);
	}
}
