// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ElectraHTTPStream : ModuleRules
{
	public ElectraHTTPStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ElectraBase",
				"HTTP"
			});
	}
}
