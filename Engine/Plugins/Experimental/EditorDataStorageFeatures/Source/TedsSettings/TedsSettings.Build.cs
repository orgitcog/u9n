// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TedsSettings : ModuleRules
{
	public TedsSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		// Enable truncation warnings in this module
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (Target.bBuildEditor)
		{
			PublicIncludePaths.AddRange(new string[] { });
			PrivateIncludePaths.AddRange(new string[] { });

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					"UnrealEd",
				});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorSubsystem",
					"TypedElementFramework",
				});
		}
	}
}
