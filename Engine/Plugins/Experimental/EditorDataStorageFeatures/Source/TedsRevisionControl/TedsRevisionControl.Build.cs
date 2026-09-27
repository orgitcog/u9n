// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TedsRevisionControl : ModuleRules
{
	public TedsRevisionControl(ReadOnlyTargetRules Target) : base(Target)
	{
		// Enable truncation warnings in this module
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (Target.bBuildEditor)
		{
			PublicIncludePaths.AddRange(new string[] {});
			PrivateIncludePaths.AddRange(new string[] {});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"SourceControl",
					"TypedElementFramework",
				});

			DynamicallyLoadedModuleNames.AddRange(new string[] {});
		}
	}
}