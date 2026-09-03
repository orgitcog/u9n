// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StorageServerWidgets : ModuleRules
{
	public StorageServerWidgets(ReadOnlyTargetRules Target)
		 : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"InputCore",
				"Json",
				"Projects",	
				"Slate",
				"SlateCore",
				"Zen",
				"DesktopPlatform"
			});

		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
