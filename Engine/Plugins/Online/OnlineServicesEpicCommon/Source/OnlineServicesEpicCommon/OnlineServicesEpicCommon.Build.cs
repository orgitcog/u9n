// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineServicesEpicCommon : ModuleRules
{
	public OnlineServicesEpicCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"OnlineServicesCommon"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"EOSSDK",
				"EOSShared",
				"OnlineServicesInterface"
			}
		);
	}
}
