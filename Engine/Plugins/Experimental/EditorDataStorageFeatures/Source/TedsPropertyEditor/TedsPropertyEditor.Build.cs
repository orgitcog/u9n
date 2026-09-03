// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TedsPropertyEditor : ModuleRules
{
	public TedsPropertyEditor(ReadOnlyTargetRules Target) : base(Target)
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
					"SceneOutliner",
					"Slate",
					"SlateCore",
					"TypedElementFramework",
					"TedsOutliner",
					"UnrealEd", // For EditorUndoClient
				});

			DynamicallyLoadedModuleNames.AddRange(new string[] {});
		}
	}
}
