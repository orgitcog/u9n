// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CollectionManager : ModuleRules
{
	public CollectionManager(ReadOnlyTargetRules Target) : base(Target)
	{
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"DirectoryWatcher",
				"Analytics",
				"SourceControl",
				"DeveloperSettings"
			}
			);
	}
}
