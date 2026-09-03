// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InstallBundleManager : ModuleRules
{
	public InstallBundleManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core",
				"IoStoreOnDemandCore",
				"Json"
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Json"
			}
		);
	}
}
