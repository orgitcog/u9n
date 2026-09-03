// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ZenDashboard : ModuleRules
{
	public ZenDashboard(ReadOnlyTargetRules Target) : base(Target)
	{
		bTreatAsEngineModule = true;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"Core",
				"Json",
				"Projects",
				"Slate",
				"SlateCore",
				"StandaloneRenderer",
				"StorageServerWidgets",
				"Zen"
			});

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnixCommonStartup"
				}
			);
		}

		PublicIncludePathModuleNames.Add("Launch");
	}
}
