// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectLauncher : ModuleRules
{
	public ProjectLauncher(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
		[
			"Core",
			"CoreUObject",
			"TargetPlatform",
			"DesktopPlatform",
			"ApplicationCore",
			"InputCore",
			"WorkspaceMenuStructure",
			"ToolWidgets",
			"TargetDeviceServices",
			"Projects",
			"DeveloperToolSettings",
			"Zen",
		]);

		PublicDependencyModuleNames.AddRange(
		[
			"Slate",
			"SlateCore",
			"LauncherServices",
		]);

		if (Target.bCompileAgainstEditor)
		{
			PrivateDependencyModuleNames.AddRange(
			[
				"Engine",
				"AssetRegistry",
			]);
		}
	}
}
